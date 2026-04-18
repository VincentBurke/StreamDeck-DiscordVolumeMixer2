#include "discordauthstorage.h"

#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QStandardPaths>

DiscordAuthStorage::DiscordAuthStorage(const QString &basePath) :
	basePath_(basePath) {
}

QList<QJsonObject> DiscordAuthStorage::loadAllAuthData() const {
	QList<QJsonObject> authDataList;
	QDir accountsDir(accountsPath());
	if(!accountsDir.exists())
		return authDataList;

	const QStringList entries = accountsDir.entryList(QStringList{"*.json"}, QDir::Files, QDir::Name);
	authDataList.reserve(entries.size());
	for(const QString &entry : entries) {
		QFile authFile(accountsDir.filePath(entry));
		if(!authFile.open(QIODevice::ReadOnly))
			continue;

		const QJsonObject authData = QJsonDocument::fromJson(authFile.readAll()).object();
		if(!authData.isEmpty())
			authDataList += authData;
	}

	return authDataList;
}

void DiscordAuthStorage::saveAuthData(const QString &accountId, const QJsonObject &oauthData) const {
	if(accountId.isEmpty() || oauthData.isEmpty())
		return;

	QDir accountsDir(accountsPath());
	if(!accountsDir.exists())
		accountsDir.mkpath(".");

	QFile authFile(authPathForAccount(accountId));
	if(!authFile.open(QIODevice::WriteOnly))
		return;

	authFile.write(QJsonDocument(oauthData).toJson(QJsonDocument::Compact));
}

QString DiscordAuthStorage::accountsPath() const {
	return QDir(resolvedBasePath()).filePath("oauth/accounts");
}

QString DiscordAuthStorage::authPathForAccount(const QString &accountId) const {
	return QDir(accountsPath()).filePath(QStringLiteral("%1.json").arg(accountId));
}

QString DiscordAuthStorage::resolvedBasePath() const {
	if(!basePath_.isEmpty())
		return basePath_;

	return QStandardPaths::writableLocation(QStandardPaths::AppDataLocation);
}
