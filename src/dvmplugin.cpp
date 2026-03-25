#include "dvmplugin.h"

#include <QJsonObject>

#include "action/action_back.h"
#include "action/action_deafen.h"
#include "action/action_microphone.h"
#include "action/action_openmixer.h"
#include "action/action_switchtarget.h"
#include "action/action_vcminfo.h"
#include "action/action_vcmpaging.h"
#include "action/action_vcmvolume.h"

namespace {
const QMap<QString, VoiceChannelMember> &emptyVoiceChannelMembers() {
	static const QMap<QString, VoiceChannelMember> empty;
	return empty;
}

const QSet<QString> &emptySpeakingMembers() {
	static const QSet<QString> empty;
	return empty;
}
}

DVMPlugin::DVMPlugin() {
	registerActionType<Action_OpenMixer>("cz.danol.discordmixer.openmixer");
	registerActionType<Action_VCMInfo>("cz.danol.discordmixer.user");
	registerActionType<Action_VCMVolume>("cz.danol.discordmixer.volumeup");
	registerActionType<Action_VCMVolume>("cz.danol.discordmixer.volumedown");
	registerActionType<Action_VCMPaging>("cz.danol.discordmixer.nextpage");
	registerActionType<Action_VCMPaging>("cz.danol.discordmixer.previouspage");
	registerActionType<Action_Back>("cz.danol.discordmixer.back");
	registerActionType<Action_Microphone>("cz.danol.discordmixer.microphone");
	registerActionType<Action_Deafen>("cz.danol.discordmixer.deafen");
	registerActionType<Action_SwitchTarget>("cz.danol.discordmixer.switchtarget");

	connect(this, &QStreamDeckPlugin::initialized, this, &DVMPlugin::onInitialized);
	connect(this, &QStreamDeckPlugin::eventReceived, this, &DVMPlugin::onStreamDeckEventReceived);
	connect(this, &QStreamDeckPlugin::globalSettingsChanged, this, &DVMPlugin::syncTargetManagerSettingsFromGlobals);

	connect(&targetManager_, &DiscordTargetManager::targetsChanged, this, [this] {
		emit targetsChanged();
		emit buttonsUpdateRequested();
	});
	connect(&targetManager_, &DiscordTargetManager::activeTargetChanged, this, [this](const QString &targetId) {
		if(globalSetting("active_target_id").toString() != targetId)
			setGlobalSetting("active_target_id", targetId);
		emit activeTargetChanged(targetId);
		emit buttonsUpdateRequested();
	});
	connect(&targetManager_, &DiscordTargetManager::primaryTargetChanged, this, [this](const QString &targetId) {
		if(globalSetting("primary_target_id").toString() != targetId)
			setGlobalSetting("primary_target_id", targetId);
	});
	connect(&targetManager_, &DiscordTargetManager::activeSessionStateChanged, this, &DVMPlugin::buttonsUpdateRequested);

	discordConnectTimeoutTimer_.setSingleShot(true);
	discordConnectTimeoutTimer_.setInterval(2000);
}

DVMPlugin::~DVMPlugin() {
}

void DVMPlugin::connectToDiscord() {
	targetManager_.discoverTargets();
}

void DVMPlugin::updateChannelMembersData() {
	if(DiscordSession *session = activeSession())
		session->updateChannelMembersData();
}

DiscordSession *DVMPlugin::activeSession() const {
	return targetManager_.activeSession();
}

QList<DiscordTarget> DVMPlugin::targets() const {
	return targetManager_.targets();
}

const DiscordTarget *DVMPlugin::target(const QString &targetId) const {
	return targetManager_.target(targetId);
}

QString DVMPlugin::activeTargetId() const {
	return targetManager_.activeTargetId();
}

QString DVMPlugin::primaryTargetId() const {
	return targetManager_.primaryTargetId();
}

bool DVMPlugin::setActiveTarget(const QString &targetId) {
	if(targetId.isEmpty())
		return false;

	targetManager_.setActiveTargetId(targetId);
	targetManager_.updateAutoActiveTarget();
	return targetManager_.activeTargetId() == targetId;
}

bool DVMPlugin::setPrimaryTarget(const QString &targetId) {
	if(targetId.isEmpty())
		return false;

	targetManager_.setPrimaryTargetId(targetId);
	return targetManager_.primaryTargetId() == targetId;
}

bool DVMPlugin::activateFirstAvailableTarget() {
	if(!targetManager_.activateFirstAvailableTarget())
		return false;

	return true;
}

void DVMPlugin::setTargetDisplayName(const QString &targetId, const QString &label) {
	QJsonObject labels = globalSetting("target_labels").toObject();
	const QString trimmedLabel = label.trimmed();
	if(trimmedLabel.isEmpty())
		labels.remove(targetId);
	else
		labels.insert(targetId, trimmedLabel);

	targetManager_.setTargetLabel(targetId, trimmedLabel);
	setGlobalSetting("target_labels", labels);
}

QString DVMPlugin::targetLabel(const QString &targetId) const {
	return targetManager_.targetLabel(targetId);
}

QString DVMPlugin::targetDisplayName(const QString &targetId) const {
	if(const DiscordTarget *target = this->target(targetId))
		return target->displayName;

	return targetId;
}

QString DVMPlugin::resolveMixerTarget(const QString &pinnedTargetId, bool usePinnedTarget) {
	if(usePinnedTarget && !pinnedTargetId.isEmpty()) {
		setActiveTarget(pinnedTargetId);
		return activeTargetId();
	}

	if(!activeTargetId().isEmpty())
		return activeTargetId();

	targetManager_.updateAutoActiveTarget();
	if(!activeTargetId().isEmpty())
		return activeTargetId();

	if(activateFirstAvailableTarget())
		return activeTargetId();

	return {};
}

QString DVMPlugin::activeConnectionError() const {
	if(DiscordSession *session = activeSession()) {
		if(session->isConnected())
			return {};

		if(!session->connectionError().isEmpty())
			return session->connectionError();
	}

	if(const DiscordTarget *activeTarget = target(activeTargetId()); activeTarget && !activeTarget->lastError.isEmpty())
		return activeTarget->lastError;

	return "ERR 1";
}

bool DVMPlugin::isDiscordConnected() const {
	if(DiscordSession *session = activeSession())
		return session->isConnected();

	return false;
}

QImage DVMPlugin::userAvatar(const QString &userId, const QString &avatarId) const {
	if(DiscordSession *session = activeSession())
		return session->userAvatar(userId, avatarId);

	return {};
}

const QMap<QString, VoiceChannelMember> &DVMPlugin::voiceChannelMembers() const {
	if(DiscordSession *session = activeSession())
		return session->voiceChannelMembers();

	return emptyVoiceChannelMembers();
}

QMap<QString, VoiceChannelMember> *DVMPlugin::activeVoiceChannelMembers() {
	if(DiscordSession *session = activeSession())
		return &session->voiceChannelMembers();

	return nullptr;
}

const QSet<QString> &DVMPlugin::speakingVoiceChannelMembers() const {
	if(DiscordSession *session = activeSession())
		return session->speakingVoiceChannelMembers();

	return emptySpeakingMembers();
}

bool DVMPlugin::isDeafened() const {
	if(DiscordSession *session = activeSession())
		return session->isDeafened();

	return false;
}

bool DVMPlugin::isMicrophoneMuted() const {
	if(DiscordSession *session = activeSession())
		return session->isMicrophoneMuted();

	return false;
}

void DVMPlugin::adjustVoiceChannelMemberVolume(VoiceChannelMember &vcm, float stepSize, int numSteps) {
	if(DiscordSession *session = activeSession())
		session->adjustVoiceChannelMemberVolume(vcm, stepSize, numSteps);
}

void DVMPlugin::syncTargetManagerSettingsFromGlobals() {
	targetManager_.setStoredTargetLabels(globalSetting("target_labels").toObject());
	targetManager_.setActiveTargetId(globalSetting("active_target_id").toString());
	targetManager_.setPrimaryTargetId(globalSetting("primary_target_id").toString());
	targetManager_.setSharedCredentials(globalSetting("client_id").toString(), globalSetting("client_secret").toString());
}

void DVMPlugin::onInitialized() {
	setGlobalSettingDefault("voiceChannelVolumeButtonStep", 5);
	setGlobalSettingDefault("voiceChannelVolumeEncoderStep", 5);

	syncTargetManagerSettingsFromGlobals();
	connectToDiscord();
}

void DVMPlugin::onStreamDeckEventReceived(const QStreamDeckEvent &e) {
	using ET = QStreamDeckEvent::EventType;

	if(isDiscordConnected())
		return;

	const bool isInteractiveEvent =
		e.eventType == ET::touchTap
		|| e.eventType == ET::keyDown
		|| e.eventType == ET::dialDown
		|| e.eventType == ET::dialUp
		|| e.eventType == ET::dialRotate;

	if(!isInteractiveEvent || discordConnectTimeoutTimer_.isActive())
		return;

	discordConnectTimeoutTimer_.start();
	connectToDiscord();
}
