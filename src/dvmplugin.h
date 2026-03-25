#pragma once

#include <qtstreamdeck2/qstreamdeckplugin.h>
#include <QImage>
#include <QSet>

#include "declares.h"
#include "discord/discordtargetmanager.h"
#include "dvmdevice.h"
#include "voicechannelmember.h"

class DVMPlugin : public QStreamDeckPluginT<DVMDevice> {
Q_OBJECT

public:
	DVMPlugin();
	~DVMPlugin();

public slots:
	/// Attempts to discover targets and connect the active one.
	void connectToDiscord();

	/// Reloads channel member data for the active target.
	void updateChannelMembersData();

public:
	DiscordSession *activeSession() const;
	QList<DiscordTarget> targets() const;
	const DiscordTarget *target(const QString &targetId) const;
	QString activeTargetId() const;
	QString primaryTargetId() const;
	bool setActiveTarget(const QString &targetId);
	bool setPrimaryTarget(const QString &targetId);
	bool activateFirstAvailableTarget();
	void setTargetDisplayName(const QString &targetId, const QString &label);
	QString targetLabel(const QString &targetId) const;
	QString targetDisplayName(const QString &targetId) const;
	QString resolveMixerTarget(const QString &pinnedTargetId, bool usePinnedTarget);
	QString activeConnectionError() const;
	bool isDiscordConnected() const;
	QImage userAvatar(const QString &userId, const QString &avatarId) const;

public:
	const QMap<QString, VoiceChannelMember> &voiceChannelMembers() const;
	QMap<QString, VoiceChannelMember> *activeVoiceChannelMembers();
	const QSet<QString> &speakingVoiceChannelMembers() const;
	bool isDeafened() const;
	bool isMicrophoneMuted() const;

	void adjustVoiceChannelMemberVolume(VoiceChannelMember &vcm, float stepSize, int numSteps);

signals:
	/// Updates text & states of all user related buttons
	void buttonsUpdateRequested();
	void targetsChanged();
	void activeTargetChanged(const QString &targetId);

private:
	void syncTargetManagerSettingsFromGlobals();

private slots:
	void onInitialized();
	void onStreamDeckEventReceived(const QStreamDeckEvent &e);

private:
	DiscordTargetManager targetManager_;
	QTimer discordConnectTimeoutTimer_;
};
