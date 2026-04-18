#pragma once

#include <QObject>
#include <QHash>
#include <QJsonObject>
#include <QMap>
#include <QSet>
#include <QTimer>

#include "discordauthstorage.h"
#include "discordsession.h"
#include "discordtargetstate.h"
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
	void discoverTargets(bool allowInteractiveAuth = false, const QString &preferredTargetId = {});
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
	QString persistentTargetId(const QString &targetId) const;

signals:
	void targetsChanged();
	void activeTargetChanged(const QString &targetId);
	void primaryTargetChanged(const QString &targetId);
	void activeSessionStateChanged();

private:
	void ensureSessionExists(const QString &pipeName);
	void syncSessionAvailability(const QSet<QString> &availablePipes);
	void connectAvailableSessions(bool allowInteractiveAuth, const QString &preferredTargetId);
	void connectSession(DiscordSession *session, bool allowInteractiveAuth);
	void saveSessionAuth(const DiscordSession &session);
	void rebuildTargets();
	QList<DiscordSessionSnapshot> sessionSnapshots() const;
	DiscordSession *preferredSession(const QString &preferredTargetId) const;
	bool probePipe(const QString &pipeName) const;
	bool isTargetConnected(const QString &targetId) const;
	bool isTargetInVoiceChannel(const QString &targetId) const;

private:
	QMap<QString, DiscordTarget> targets_;
	QHash<QString, DiscordSession *> sessions_;
	QHash<QString, QString> targetLabels_;
	QHash<QString, QString> targetToPipe_;
	DiscordAuthStorage authStorage_;
	QString clientID_;
	QString clientSecret_;
	QString activeTargetId_;
	QString primaryTargetId_;
	QTimer discoveryTimer_;
};
