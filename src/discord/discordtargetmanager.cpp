#include "discordtargetmanager.h"

#include <QDir>
#include <QLocalSocket>
#include <QStandardPaths>
#include <QSet>

DiscordTargetManager::DiscordTargetManager(QObject *parent) : QObject(parent) {
	discoveryTimer_.setInterval(2500);
	discoveryTimer_.callOnTimeout(this, &DiscordTargetManager::discoverTargets);
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
		if(label.isEmpty())
			continue;

		newLabels.insert(it.key(), label);
		ensureTargetExists(it.key());
	}

	if(targetLabels_ == newLabels)
		return;

	targetLabels_ = newLabels;
	for(auto it = targets_.begin(), end = targets_.end(); it != end; ++it)
		it->displayName = resolvedDisplayName(it.value());

	emit targetsChanged();
}

void DiscordTargetManager::setActiveTargetId(const QString &targetId) {
	if(!targetId.isEmpty())
		ensureTargetExists(targetId);

	if(activeTargetId_ == targetId)
		return;

	activeTargetId_ = targetId;
	emit activeTargetChanged(activeTargetId_);
	emit activeSessionStateChanged();
}

void DiscordTargetManager::setPrimaryTargetId(const QString &targetId) {
	if(!targetId.isEmpty())
		ensureTargetExists(targetId);

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
	ensureTargetExists(targetId);

	const QString trimmedLabel = label.trimmed();
	if(trimmedLabel.isEmpty())
		targetLabels_.remove(targetId);
	else
		targetLabels_.insert(targetId, trimmedLabel);

	DiscordTarget &target = targets_[targetId];
	const QString newDisplayName = resolvedDisplayName(target);
	if(target.displayName == newDisplayName)
		return;

	target.displayName = newDisplayName;
	emit targetsChanged();
}

void DiscordTargetManager::discoverTargets() {
	QSet<QString> availableTargets;
	for(int i = 0; i < 10; i++) {
		const QString pipeName = QStringLiteral("discord-ipc-%1").arg(i);
		if(!probePipe(pipeName))
			continue;

		availableTargets.insert(pipeName);
		ensureTargetExists(pipeName);
	}

	bool didTargetsChange = false;
	bool activeSessionChanged = false;

	for(auto it = targets_.begin(), end = targets_.end(); it != end; ++it) {
		DiscordTarget &target = it.value();
		DiscordSession *targetSession = sessions_.value(target.id);
		const bool isAvailable = availableTargets.contains(target.id);

		if(target.isAvailable != isAvailable) {
			target.isAvailable = isAvailable;
			didTargetsChange = true;
			if(target.id == activeTargetId_)
				activeSessionChanged = true;
		}

		if(!isAvailable) {
			if(targetSession && targetSession->isConnected())
				targetSession->disconnect();

			const QString disconnectedError = QStringLiteral("DISCONNECTED");
			if(target.lastError != disconnectedError) {
				target.lastError = disconnectedError;
				didTargetsChange = true;
			}
			continue;
		}

		if(clientID_.isEmpty() || clientSecret_.isEmpty()) {
			if(targetSession)
				targetSession->disconnect();

			const QString missingCredentials = QStringLiteral("ERR 0");
			if(target.lastError != missingCredentials) {
				target.lastError = missingCredentials;
				didTargetsChange = true;
			}
			continue;
		}

		if(targetSession)
			targetSession->ensureConnected(clientID_, clientSecret_, oauthDataPathForTarget(target.id));

		const DiscordTarget previousTarget = target;
		syncTargetFromSession(target.id);
		if(previousTarget.displayName != target.displayName
			|| previousTarget.cachedUser.userID != target.cachedUser.userID
			|| previousTarget.cachedUser.username != target.cachedUser.username
			|| previousTarget.cachedUser.avatarID != target.cachedUser.avatarID
			|| previousTarget.isInVoiceChannel != target.isInVoiceChannel
			|| previousTarget.lastError != target.lastError) {
			didTargetsChange = true;
			if(target.id == activeTargetId_)
				activeSessionChanged = true;
		}
	}

	updateAutoActiveTarget();

	if(didTargetsChange)
		emit targetsChanged();
	if(activeSessionChanged)
		emit activeSessionStateChanged();
}

void DiscordTargetManager::updateAutoActiveTarget() {
	if(primaryTargetId_.isEmpty()) {
		const QString defaultPrimaryTargetId = firstAvailableTargetId();
		if(!defaultPrimaryTargetId.isEmpty()) {
			primaryTargetId_ = defaultPrimaryTargetId;
			emit primaryTargetChanged(primaryTargetId_);
		}
	}

	QStringList voiceTargets;
	for(auto it = targets_.cbegin(), end = targets_.cend(); it != end; ++it) {
		if(isTargetInVoiceChannel(it.key()))
			voiceTargets += it.key();
	}

	QString resolvedTargetId = activeTargetId_;
	if(voiceTargets.size() == 1) {
		resolvedTargetId = voiceTargets.first();
	}
	else if(voiceTargets.size() > 1) {
		if(voiceTargets.contains(primaryTargetId_))
			resolvedTargetId = primaryTargetId_;
		else if(voiceTargets.contains(activeTargetId_))
			resolvedTargetId = activeTargetId_;
		else
			resolvedTargetId = voiceTargets.first();
	}
	else if(resolvedTargetId.isEmpty()) {
		if(isTargetConnected(primaryTargetId_) || (target(primaryTargetId_) && target(primaryTargetId_)->isAvailable))
			resolvedTargetId = primaryTargetId_;
		else
			resolvedTargetId = firstAvailableTargetId();
	}

	if(!resolvedTargetId.isEmpty())
		setActiveTargetId(resolvedTargetId);
}

QList<DiscordTarget> DiscordTargetManager::targets() const {
	return targets_.values();
}

const DiscordTarget *DiscordTargetManager::target(const QString &targetId) const {
	const auto it = targets_.constFind(targetId);
	return it == targets_.cend() ? nullptr : &it.value();
}

DiscordSession *DiscordTargetManager::session(const QString &targetId) const {
	return sessions_.value(targetId);
}

DiscordSession *DiscordTargetManager::activeSession() const {
	return sessions_.value(activeTargetId_);
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

void DiscordTargetManager::ensureTargetExists(const QString &targetId) {
	if(targets_.contains(targetId))
		return;

	DiscordTarget target;
	target.id = targetId;
	target.pipeName = targetId;
	target.displayName = targetLabels_.value(targetId, targetId);
	targets_.insert(targetId, target);

	auto *targetSession = new DiscordSession(targetId, targetId, this);
	connect(targetSession, &DiscordSession::stateChanged, this, [this, targetId] {
		syncTargetFromSession(targetId);
		updateAutoActiveTarget();
		emit targetsChanged();
		if(targetId == activeTargetId_)
			emit activeSessionStateChanged();
	});
	sessions_.insert(targetId, targetSession);
}

QString DiscordTargetManager::oauthDataPathForTarget(const QString &targetId) const {
	const QString basePath = QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
	QDir oauthDir(basePath);
	oauthDir.mkpath("oauth");
	return oauthDir.filePath(QStringLiteral("oauth/%1.json").arg(targetId));
}

QString DiscordTargetManager::resolvedDisplayName(const DiscordTarget &target) const {
	if(const QString label = targetLabels_.value(target.id).trimmed(); !label.isEmpty())
		return label;

	if(!target.cachedUser.username.isEmpty())
		return target.cachedUser.username;

	return target.pipeName;
}

void DiscordTargetManager::syncTargetFromSession(const QString &targetId) {
	auto it = targets_.find(targetId);
	if(it == targets_.end())
		return;

	DiscordTarget &target = it.value();
	DiscordSession *targetSession = sessions_.value(targetId);
	if(!targetSession)
		return;

	if(const DiscordUserSummary user = targetSession->userSummary(); user.isValid())
		target.cachedUser = user;

	target.isInVoiceChannel = targetSession->isInVoiceChannel();
	target.lastError = targetSession->connectionError();
	target.displayName = resolvedDisplayName(target);
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
