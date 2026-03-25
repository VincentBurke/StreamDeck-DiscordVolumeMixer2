#pragma once

#include <QString>

struct DiscordUserSummary {
	QString userID;
	QString username;
	QString avatarID;

	inline bool isValid() const {
		return !userID.isEmpty();
	}
};

struct DiscordTarget {
	QString id;
	QString pipeName;
	QString displayName;
	DiscordUserSummary cachedUser;
	bool isAvailable = false;
	QString lastError;
};
