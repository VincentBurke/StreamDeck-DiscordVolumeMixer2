#pragma once

#include <QString>

struct DiscordUserSummary {
	QString userID;
	QString username;
	QString avatarID;

	inline bool isValid() const {
		return !userID.isEmpty();
	}

	bool operator==(const DiscordUserSummary &) const = default;
};

struct DiscordTarget {
	QString id;
	QString pipeName;
	QString displayName;
	DiscordUserSummary cachedUser;
	bool isAvailable = false;
	bool isInVoiceChannel = false;
	QString lastError;

	bool operator==(const DiscordTarget &) const = default;
};
