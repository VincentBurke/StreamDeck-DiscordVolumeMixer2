#pragma once

#include <QObject>
#include <QMap>
#include <QSet>

#include <qtdiscordipc/qdiscord.h>

#include "discordtypes.h"
#include "voicechannelmember.h"

class DiscordSession : public QObject {
Q_OBJECT

public:
	DiscordSession(const QString &targetId, const QString &pipeName, QObject *parent = nullptr);

public:
	bool ensureConnected(const QString &clientID, const QString &clientSecret, const QString &oauthDataPath);
	void disconnect();

	inline const QString &targetId() const {
		return targetId_;
	}

	inline const QString &pipeName() const {
		return pipeName_;
	}

	inline bool isConnected() const {
		return discord_.isConnected();
	}

	inline const QString &connectionError() const {
		return discord_.connectionError();
	}

	inline const DiscordUserSummary &userSummary() const {
		return userSummary_;
	}

	inline const QString &currentVoiceChannelID() const {
		return currentVoiceChannelID_;
	}

	inline bool isInVoiceChannel() const {
		return !currentVoiceChannelID_.isEmpty();
	}

	inline const QMap<QString, VoiceChannelMember> &voiceChannelMembers() const {
		return voiceChannelMembers_;
	}

	inline QMap<QString, VoiceChannelMember> &voiceChannelMembers() {
		return voiceChannelMembers_;
	}

	inline const QSet<QString> &speakingVoiceChannelMembers() const {
		return speakingVoiceChannelMembers_;
	}

	inline bool isDeafened() const {
		return isDeafened_;
	}

	inline bool isMicrophoneMuted() const {
		return isMicrophoneMuted_;
	}

public:
	void updateChannelMembersData();
	void adjustVoiceChannelMemberVolume(VoiceChannelMember &vcm, float stepSize, int numSteps);
	void setVoiceChannelMemberMuted(VoiceChannelMember &vcm, bool muted);
	void setMicrophoneMuted(bool muted);
	void setDeafened(bool deafened);
	QImage userAvatar(const QString &userId, const QString &avatarId);

signals:
	void stateChanged();

private:
	void updateCurrentVoiceChannel(const QString &newVoiceChannel);
	void updateSelfVoiceState(const QDiscordMessage &msg);
	void refreshUserSummary();

private slots:
	void onDiscordMessageReceived(const QDiscordMessage &msg);

private:
	const QString targetId_;
	const QString pipeName_;
	QDiscord discord_;
	DiscordUserSummary userSummary_;
	QString currentVoiceChannelID_;
	QMap<QString, VoiceChannelMember> voiceChannelMembers_;
	QSet<QString> speakingVoiceChannelMembers_;
	bool isDeafened_ = false;
	bool isMicrophoneMuted_ = false;
};
