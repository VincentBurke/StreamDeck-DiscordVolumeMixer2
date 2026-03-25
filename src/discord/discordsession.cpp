#include "discordsession.h"

#include <QJsonArray>

DiscordSession::DiscordSession(const QString &targetId, const QString &pipeName, QObject *parent) :
	QObject(parent),
	targetId_(targetId),
	pipeName_(pipeName) {
	connect(&discord_, &QDiscord::messageReceived, this, &DiscordSession::onDiscordMessageReceived);
	connect(&discord_, &QDiscord::avatarReady, this, &DiscordSession::stateChanged);
	connect(&discord_, &QDiscord::disconnected, this, [this] {
		currentVoiceChannelID_.clear();
		voiceChannelMembers_.clear();
		speakingVoiceChannelMembers_.clear();
		isDeafened_ = false;
		isMicrophoneMuted_ = false;
		emit stateChanged();
	});
}

bool DiscordSession::ensureConnected(const QString &clientID, const QString &clientSecret, const QString &oauthDataPath) {
	if(discord_.isConnected() || discord_.isProcessing())
		return discord_.isConnected();

	if(!discord_.connect(QDiscordConnectOptions{
		.clientID = clientID,
		.clientSecret = clientSecret,
		.pipeName = pipeName_,
		.oauthDataPath = oauthDataPath,
	})) {
		emit stateChanged();
		return false;
	}

	refreshUserSummary();

	discord_.sendCommand(+QDiscord::CommandType::subscribe, {}, QJsonObject{
		{"evt", "VOICE_CHANNEL_SELECT"},
	});

	if(QDiscordReply *reply = discord_.sendCommand(+QDiscord::CommandType::getVoiceSettings)) {
		connect(reply, &QDiscordReply::success, this, [this](const QDiscordMessage &msg) {
			isMicrophoneMuted_ = msg.data["mute"].toBool();
			isDeafened_ = msg.data["deaf"].toBool();
			emit stateChanged();
		});
	}

	updateChannelMembersData();
	emit stateChanged();
	return true;
}

void DiscordSession::disconnect() {
	discord_.disconnect();
}

void DiscordSession::updateChannelMembersData() {
	if(!discord_.isConnected())
		return;

	QDiscordReply *reply = discord_.sendCommand(+QDiscord::CommandType::getSelectedVoiceChannel);
	connect(reply, &QDiscordReply::success, this, [this](const QDiscordMessage &msg) {
		updateCurrentVoiceChannel(msg.data["id"].toString());

		voiceChannelMembers_.clear();

		const QJsonArray voiceStates = msg.data["voice_states"].toArray();
		for(const QJsonValue &voiceState : voiceStates) {
			const VoiceChannelMember member = VoiceChannelMember::fromJson(voiceState.toObject());
			if(member.userID != discord_.userID())
				voiceChannelMembers_.insert(member.userID, member);
		}

		emit stateChanged();
	});
}

void DiscordSession::adjustVoiceChannelMemberVolume(VoiceChannelMember &vcm, float stepSize, int numSteps) {
	float newVolume = vcm.volume + stepSize * numSteps;
	newVolume = qBound(QDiscord::minVoiceVolume, newVolume, QDiscord::maxVoiceVolume);
	newVolume = qRound(newVolume / stepSize) * stepSize;

	if(newVolume == vcm.volume && !vcm.isMuted)
		return;

	vcm.volume = newVolume;
	vcm.isMuted = false;

	discord_.sendCommand(+QDiscord::CommandType::setUserVoiceSettings, QJsonObject{
		{"user_id", vcm.userID},
		{"volume",  QDiscord::uiToIPCVolume(newVolume)},
		{"mute",    false},
	});
	emit stateChanged();
}

void DiscordSession::setVoiceChannelMemberMuted(VoiceChannelMember &vcm, bool muted) {
	vcm.isMuted = muted;
	discord_.sendCommand(+QDiscord::CommandType::setUserVoiceSettings, QJsonObject{
		{"user_id", vcm.userID},
		{"mute", muted},
	});
	emit stateChanged();
}

void DiscordSession::setMicrophoneMuted(bool muted) {
	isMicrophoneMuted_ = muted;
	discord_.sendCommand(+QDiscord::CommandType::setVoiceSettings, {{"mute", muted}});
	emit stateChanged();
}

void DiscordSession::setDeafened(bool deafened) {
	isDeafened_ = deafened;
	isMicrophoneMuted_ |= deafened;
	discord_.sendCommand(+QDiscord::CommandType::setVoiceSettings, {{"deaf", deafened}});
	emit stateChanged();
}

QImage DiscordSession::userAvatar(const QString &userId, const QString &avatarId) {
	return discord_.getUserAvatar(userId, avatarId);
}

void DiscordSession::updateCurrentVoiceChannel(const QString &newVoiceChannel) {
	if(newVoiceChannel == currentVoiceChannelID_)
		return;

	static const QStringList events{
		"VOICE_STATE_UPDATE", "VOICE_STATE_CREATE", "VOICE_STATE_DELETE", "SPEAKING_START", "SPEAKING_STOP"
	};
	const auto updateSubscriptions = [&](const QString &command) {
		const QJsonObject args{{"channel_id", currentVoiceChannelID_}};
		for(const QString &event : events)
			discord_.sendCommand(command, args, {{"evt", event}});
	};

	if(!currentVoiceChannelID_.isEmpty())
		updateSubscriptions(+QDiscord::CommandType::unsubscribe);

	currentVoiceChannelID_ = newVoiceChannel;

	if(!currentVoiceChannelID_.isEmpty())
		updateSubscriptions(+QDiscord::CommandType::subscribe);
}

void DiscordSession::updateSelfVoiceState(const QDiscordMessage &msg) {
	const QJsonObject voiceState = msg.data["voice_state"].toObject();

	if(auto mute = voiceState["self_mute"]; !mute.isUndefined())
		isMicrophoneMuted_ = mute.toBool();

	if(auto deaf = voiceState["self_deaf"]; !deaf.isUndefined())
		isDeafened_ = deaf.toBool();
}

void DiscordSession::refreshUserSummary() {
	const QDiscordUserSummary discordUser = discord_.userSummary();
	if(!discordUser.isValid())
		return;

	userSummary_ = DiscordUserSummary{
		.userID = discordUser.userID,
		.username = discordUser.username,
		.avatarID = discordUser.avatarID,
	};
}

void DiscordSession::onDiscordMessageReceived(const QDiscordMessage &msg) {
	using ET = QDiscordMessage::EventType;

	switch(msg.event) {
		case ET::voiceChannelSelect:
			updateCurrentVoiceChannel(msg.data["channel_id"].toString());
			updateChannelMembersData();
			break;

		case ET::voiceStateCreate: {
			const VoiceChannelMember member = VoiceChannelMember::fromJson(msg.data);
			if(member.userID == discord_.userID())
				updateSelfVoiceState(msg);
			else
				voiceChannelMembers_.insert(member.userID, member);
			break;
		}

		case ET::voiceStateUpdate: {
			const VoiceChannelMember member = VoiceChannelMember::fromJson(msg.data);
			if(member.userID == discord_.userID())
				updateSelfVoiceState(msg);
			else if(voiceChannelMembers_.contains(member.userID))
				voiceChannelMembers_.insert(member.userID, member);
			break;
		}

		case ET::voiceStateDelete: {
			const VoiceChannelMember voiceData = VoiceChannelMember::fromJson(msg.data);
			voiceChannelMembers_.remove(voiceData.userID);

			// When Discord moves the current user between channels, this can be the only event.
			if(voiceData.userID == discord_.userID())
				updateChannelMembersData();
			break;
		}

		case ET::speakingStart:
			speakingVoiceChannelMembers_.insert(msg.data["user_id"].toString());
			break;

		case ET::speakingStop:
			speakingVoiceChannelMembers_.remove(msg.data["user_id"].toString());
			break;

		case ET::voiceSettingsUpdate:
			isMicrophoneMuted_ = msg.data["mute"].toBool();
			isDeafened_ = msg.data["deaf"].toBool();
			break;

		default:
			return;
	}

	emit stateChanged();
}
