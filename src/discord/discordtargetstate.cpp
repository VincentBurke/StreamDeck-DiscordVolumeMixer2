#include "discordtargetstate.h"

namespace {
QString resolveDisplayName(const DiscordTarget &target, const QHash<QString, QString> &targetLabels) {
	if(!isTemporaryDiscordTargetId(target.id)) {
		if(const QString label = targetLabels.value(target.id).trimmed(); !label.isEmpty())
			return label;
	}

	if(!target.cachedUser.username.isEmpty())
		return target.cachedUser.username;

	return target.pipeName;
}

QString firstAvailableTargetId(const QMap<QString, DiscordTarget> &targets) {
	for(auto it = targets.cbegin(), end = targets.cend(); it != end; ++it) {
		if(it->isAvailable)
			return it.key();
	}

	return {};
}

bool containsAvailableTarget(const QMap<QString, DiscordTarget> &targets, const QString &targetId) {
	const auto it = targets.constFind(targetId);
	return it != targets.cend() && it->isAvailable;
}
}

QString temporaryDiscordTargetId(const QString &pipeName) {
	return QStringLiteral("pipe.%1").arg(pipeName);
}

bool isTemporaryDiscordTargetId(const QString &targetId) {
	return targetId.startsWith("pipe.");
}

DiscordTargetState buildDiscordTargetState(
	const QList<DiscordSessionSnapshot> &sessions,
	const QHash<QString, QString> &targetLabels,
	const QString &activeTargetId,
	const QString &primaryTargetId) {
	DiscordTargetState state;

	for(const DiscordSessionSnapshot &session : sessions) {
		DiscordTarget target;
		// The IPC slot changes across restarts, so persisted identity must come from the authenticated account.
		target.id = session.user.isValid() ? session.user.userID : temporaryDiscordTargetId(session.pipeName);
		target.pipeName = session.pipeName;
		target.cachedUser = session.user;
		target.isAvailable = session.isAvailable;
		target.isInVoiceChannel = session.isConnected && session.isInVoiceChannel;
		target.lastError = session.lastError;
		target.displayName = resolveDisplayName(target, targetLabels);
		state.targets.insert(target.id, target);
	}

	state.primaryTargetId = primaryTargetId;
	if(!containsAvailableTarget(state.targets, state.primaryTargetId))
		state.primaryTargetId = firstAvailableTargetId(state.targets);

	QStringList voiceTargets;
	for(auto it = state.targets.cbegin(), end = state.targets.cend(); it != end; ++it) {
		if(it->isInVoiceChannel)
			voiceTargets += it.key();
	}

	state.activeTargetId = activeTargetId;
	if(voiceTargets.size() == 1) {
		state.activeTargetId = voiceTargets.first();
	}
	else if(voiceTargets.size() > 1) {
		if(voiceTargets.contains(state.primaryTargetId))
			state.activeTargetId = state.primaryTargetId;
		else if(voiceTargets.contains(state.activeTargetId))
			state.activeTargetId = state.activeTargetId;
		else
			state.activeTargetId = voiceTargets.first();
	}
	else if(!containsAvailableTarget(state.targets, state.activeTargetId)) {
		if(containsAvailableTarget(state.targets, state.primaryTargetId))
			state.activeTargetId = state.primaryTargetId;
		else
			state.activeTargetId = firstAvailableTargetId(state.targets);
	}

	return state;
}
