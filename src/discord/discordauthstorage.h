#pragma once

#include <QJsonObject>
#include <QList>
#include <QString>

struct DiscordStoredAuth {
	QString accountId;
	QJsonObject oauthData;
};

class DiscordAuthStorage {
public:
	explicit DiscordAuthStorage(const QString &basePath = {});

public:
	QList<QJsonObject> loadAllAuthData() const;
	void saveAuthData(const QString &accountId, const QJsonObject &oauthData) const;
	QString accountsPath() const;
	QString authPathForAccount(const QString &accountId) const;

private:
	QString resolvedBasePath() const;

private:
	QString basePath_;
};
