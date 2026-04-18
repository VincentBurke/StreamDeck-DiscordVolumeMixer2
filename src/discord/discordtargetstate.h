#pragma once

#include <QHash>
#include <QList>
#include <QMap>
#include <QString>

#include "discordtypes.h"

struct DiscordSessionSnapshot {
	QString pipeName;
	DiscordUserSummary user;
	QString lastError;
	bool isAvailable = false;
	bool isConnected = false;
	bool isInVoiceChannel = false;
};

struct DiscordTargetState {
	QMap<QString, DiscordTarget> targets;
	QString activeTargetId;
	QString primaryTargetId;
};

QString temporaryDiscordTargetId(const QString &pipeName);
bool isTemporaryDiscordTargetId(const QString &targetId);
DiscordTargetState buildDiscordTargetState(
	const QList<DiscordSessionSnapshot> &sessions,
	const QHash<QString, QString> &targetLabels,
	const QString &activeTargetId,
	const QString &primaryTargetId);
