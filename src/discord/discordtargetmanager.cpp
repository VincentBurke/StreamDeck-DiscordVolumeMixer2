#include "discordtargetmanager.h"

#include <QLocalSocket>
#include <QSet>

DiscordTargetManager::DiscordTargetManager(QObject *parent) : QObject(parent) {
	discoveryTimer_.setInterval(2500);
	discoveryTimer_.callOnTimeout(this, [this] {
		discoverTargets();
	});
	discoveryTimer_.start();
}

void DiscordTargetManager::setSharedCredentials(const QString &clientID, const QString &clientSecret) {
	if(clientID_ == clientID && clientSecret_ == clientSecret)
		return;

	clientID_ = clientID;
	clientSecret_ = clientSecret;
	discoverTargets();
}

void DiscordTargetManager::setStoredTargetLabels(const QJsonObject &labels) {
	QHash<QString, QString> newLabels;
	for(auto it = labels.begin(), end = labels.end(); it != end; ++it) {
		const QString label = it.value().toString().trimmed();
		if(!label.isEmpty())
			newLabels.insert(it.key(), label);
	}

	if(targetLabels_ == newLabels)
		return;

	targetLabels_ = newLabels;
	rebuildTargets();
}

void DiscordTargetManager::setActiveTargetId(const QString &targetId) {
	if(activeTargetId_ == targetId)
		return;

	activeTargetId_ = targetId;
	emit activeTargetChanged(activeTargetId_);
	emit activeSessionStateChanged();
}

void DiscordTargetManager::setPrimaryTargetId(const QString &targetId) {
	if(primaryTargetId_ == targetId)
		return;

	primaryTargetId_ = targetId;
	emit primaryTargetChanged(primaryTargetId_);
	updateAutoActiveTarget();
}

bool DiscordTargetManager::activateFirstAvailableTarget() {
	const QString targetId = firstAvailableTargetId();
	if(targetId.isEmpty())
		return false;

	setActiveTargetId(targetId);
	return true;
}

void DiscordTargetManager::setTargetLabel(const QString &targetId, const QString &label) {
	if(isTemporaryDiscordTargetId(targetId))
		return;

	const QString trimmedLabel = label.trimmed();
	if(trimmedLabel.isEmpty())
		targetLabels_.remove(targetId);
	else
		targetLabels_.insert(targetId, trimmedLabel);

	rebuildTargets();
}

void DiscordTargetManager::discoverTargets(bool allowInteractiveAuth, const QString &preferredTargetId) {
	QSet<QString> availablePipes;
	for(int i = 0; i < 10; ++i) {
		const QString pipeName = QStringLiteral("discord-ipc-%1").arg(i);
		if(probePipe(pipeName))
			availablePipes.insert(pipeName);
	}

	for(const QString &pipeName : availablePipes)
		ensureSessionExists(pipeName);

	syncSessionAvailability(availablePipes);
	connectAvailableSessions(allowInteractiveAuth, preferredTargetId);
	rebuildTargets();
}

void DiscordTargetManager::updateAutoActiveTarget() {
	rebuildTargets();
}

QList<DiscordTarget> DiscordTargetManager::targets() const {
	return targets_.values();
}

const DiscordTarget *DiscordTargetManager::target(const QString &targetId) const {
	const auto it = targets_.constFind(targetId);
	return it == targets_.cend() ? nullptr : &it.value();
}

DiscordSession *DiscordTargetManager::session(const QString &targetId) const {
	const QString pipeName = targetToPipe_.value(targetId);
	return pipeName.isEmpty() ? nullptr : sessions_.value(pipeName);
}

DiscordSession *DiscordTargetManager::activeSession() const {
	return session(activeTargetId_);
}

QString DiscordTargetManager::firstAvailableTargetId() const {
	for(auto it = targets_.cbegin(), end = targets_.cend(); it != end; ++it) {
		if(it->isAvailable)
			return it.key();
	}

	return {};
}

QString DiscordTargetManager::targetLabel(const QString &targetId) const {
	return targetLabels_.value(targetId);
}

QString DiscordTargetManager::activeTargetId() const {
	return activeTargetId_;
}

QString DiscordTargetManager::primaryTargetId() const {
	return primaryTargetId_;
}

QString DiscordTargetManager::persistentTargetId(const QString &targetId) const {
	if(targetId.isEmpty())
		return {};

	if(!isTemporaryDiscordTargetId(targetId))
		return targets_.contains(targetId) ? targetId : QString{};

	if(DiscordSession *targetSession = preferredSession(targetId)) {
		const DiscordUserSummary &user = targetSession->userSummary();
		if(user.isValid())
			return user.userID;
	}

	return {};
}

void DiscordTargetManager::ensureSessionExists(const QString &pipeName) {
	if(sessions_.contains(pipeName))
		return;

	auto *targetSession = new DiscordSession(pipeName, this);
	connect(targetSession, &DiscordSession::stateChanged, this, [this, targetSession] {
		saveSessionAuth(*targetSession);
		rebuildTargets();
	});
	sessions_.insert(pipeName, targetSession);
}

void DiscordTargetManager::syncSessionAvailability(const QSet<QString> &availablePipes) {
	for(auto it = sessions_.begin(), end = sessions_.end(); it != end; ++it) {
		if(availablePipes.contains(it.key()))
			continue;

		if(it.value()->isConnected())
			it.value()->disconnect();
	}
}

void DiscordTargetManager::connectAvailableSessions(bool allowInteractiveAuth, const QString &preferredTargetId) {
	if(clientID_.isEmpty() || clientSecret_.isEmpty()) {
		for(auto it = sessions_.begin(), end = sessions_.end(); it != end; ++it) {
			if(it.value()->isConnected())
				it.value()->disconnect();
		}

		rebuildTargets();
		return;
	}

	for(auto it = sessions_.begin(), end = sessions_.end(); it != end; ++it)
		connectSession(it.value(), false);

	if(!allowInteractiveAuth)
		return;

	DiscordSession *targetSession = preferredSession(preferredTargetId);
	if(!targetSession && !preferredTargetId.isEmpty())
		return;

	if(!targetSession)
		targetSession = preferredSession(activeTargetId_);

	if(!targetSession)
		targetSession = preferredSession(firstAvailableTargetId());

	if(!targetSession)
		return;

	connectSession(targetSession, true);
}

void DiscordTargetManager::connectSession(DiscordSession *session, bool allowInteractiveAuth) {
	if(!session || !probePipe(session->pipeName()))
		return;

	if(clientID_.isEmpty() || clientSecret_.isEmpty()) {
		if(session->isConnected())
			session->disconnect();
		return;
	}

	if(session->ensureConnected(clientID_, clientSecret_, authStorage_.loadAllAuthData(), allowInteractiveAuth))
		saveSessionAuth(*session);
}

void DiscordTargetManager::saveSessionAuth(const DiscordSession &session) {
	const DiscordUserSummary &user = session.userSummary();
	if(!user.isValid())
		return;

	authStorage_.saveAuthData(user.userID, session.oauthData());
}

void DiscordTargetManager::rebuildTargets() {
	const QMap<QString, DiscordTarget> previousTargets = targets_;
	const QString previousActiveTargetId = activeTargetId_;
	const QString previousPrimaryTargetId = primaryTargetId_;

	const DiscordTargetState state = buildDiscordTargetState(sessionSnapshots(), targetLabels_, activeTargetId_, primaryTargetId_);

	targets_ = state.targets;
	targetToPipe_.clear();
	for(auto it = targets_.cbegin(), end = targets_.cend(); it != end; ++it)
		targetToPipe_.insert(it.key(), it->pipeName);

	activeTargetId_ = state.activeTargetId;
	primaryTargetId_ = state.primaryTargetId;

	if(previousTargets != targets_)
		emit targetsChanged();
	if(previousActiveTargetId != activeTargetId_)
		emit activeTargetChanged(activeTargetId_);
	if(previousPrimaryTargetId != primaryTargetId_)
		emit primaryTargetChanged(primaryTargetId_);
	if(previousActiveTargetId != activeTargetId_ || previousTargets != targets_)
		emit activeSessionStateChanged();
}

QList<DiscordSessionSnapshot> DiscordTargetManager::sessionSnapshots() const {
	QList<DiscordSessionSnapshot> snapshots;
	snapshots.reserve(sessions_.size());

	for(auto it = sessions_.cbegin(), end = sessions_.cend(); it != end; ++it) {
		DiscordSessionSnapshot snapshot;
		snapshot.pipeName = it.key();
		snapshot.user = it.value()->userSummary();
		snapshot.isAvailable = probePipe(it.key());
		snapshot.isConnected = it.value()->isConnected();
		snapshot.isInVoiceChannel = it.value()->isInVoiceChannel();
		snapshot.lastError = snapshot.isAvailable
			? (clientID_.isEmpty() || clientSecret_.isEmpty()
				? QStringLiteral("ERR 0")
				: it.value()->connectionError())
			: QStringLiteral("DISCONNECTED");
		snapshots += snapshot;
	}

	return snapshots;
}

DiscordSession *DiscordTargetManager::preferredSession(const QString &preferredTargetId) const {
	if(preferredTargetId.isEmpty())
		return nullptr;

	if(isTemporaryDiscordTargetId(preferredTargetId)) {
		const QString pipeName = preferredTargetId.sliced(QStringLiteral("pipe.").size());
		return sessions_.value(pipeName);
	}

	return session(preferredTargetId);
}

bool DiscordTargetManager::probePipe(const QString &pipeName) const {
	QLocalSocket socket;
	socket.connectToServer(pipeName);
	const bool connected = socket.waitForConnected(100);
	if(connected)
		socket.disconnectFromServer();

	return connected;
}

bool DiscordTargetManager::isTargetConnected(const QString &targetId) const {
	DiscordSession *targetSession = session(targetId);
	return targetSession && targetSession->isConnected();
}

bool DiscordTargetManager::isTargetInVoiceChannel(const QString &targetId) const {
	const DiscordTarget *discordTarget = target(targetId);
	if(!discordTarget || !discordTarget->isAvailable)
		return false;

	DiscordSession *targetSession = session(targetId);
	return targetSession && targetSession->isConnected() && targetSession->isInVoiceChannel();
}
