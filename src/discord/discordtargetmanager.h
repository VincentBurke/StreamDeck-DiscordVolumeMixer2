#pragma once

#include <QObject>
#include <QHash>
#include <QJsonObject>
#include <QMap>
#include <QTimer>

#include "discordsession.h"
#include "discordtypes.h"

class DiscordTargetManager : public QObject {
Q_OBJECT

public:
	explicit DiscordTargetManager(QObject *parent = nullptr);

public:
	void setSharedCredentials(const QString &clientID, const QString &clientSecret);
	void setStoredTargetLabels(const QJsonObject &labels);
	void setActiveTargetId(const QString &targetId);
	void setPrimaryTargetId(const QString &targetId);
	bool activateFirstAvailableTarget();
	void setTargetLabel(const QString &targetId, const QString &label);
	void discoverTargets();
	void updateAutoActiveTarget();

public:
	QList<DiscordTarget> targets() const;
	const DiscordTarget *target(const QString &targetId) const;
	DiscordSession *session(const QString &targetId) const;
	DiscordSession *activeSession() const;
	QString firstAvailableTargetId() const;
	QString targetLabel(const QString &targetId) const;
	QString activeTargetId() const;
	QString primaryTargetId() const;

signals:
	void targetsChanged();
	void activeTargetChanged(const QString &targetId);
	void primaryTargetChanged(const QString &targetId);
	void activeSessionStateChanged();

private:
	void ensureTargetExists(const QString &targetId);
	QString oauthDataPathForTarget(const QString &targetId) const;
	QString resolvedDisplayName(const DiscordTarget &target) const;
	void syncTargetFromSession(const QString &targetId);
	bool probePipe(const QString &pipeName) const;
	bool isTargetConnected(const QString &targetId) const;
	bool isTargetInVoiceChannel(const QString &targetId) const;

private:
	QMap<QString, DiscordTarget> targets_;
	QHash<QString, DiscordSession *> sessions_;
	QHash<QString, QString> targetLabels_;
	QString clientID_;
	QString clientSecret_;
	QString activeTargetId_;
	QString primaryTargetId_;
	QTimer discoveryTimer_;
};
