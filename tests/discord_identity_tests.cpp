#include <QtTest>

#include <QFileInfo>
#include <QSet>
#include <QTemporaryDir>

#include "discord/discordauthstorage.h"
#include "discord/discordtargetstate.h"

namespace {
DiscordSessionSnapshot authenticatedSession(
	const QString &pipeName,
	const QString &userId,
	const QString &username,
	bool inVoiceChannel = false) {
	DiscordSessionSnapshot snapshot;
	snapshot.pipeName = pipeName;
	snapshot.user = DiscordUserSummary{
		.userID = userId,
		.username = username,
	};
	snapshot.isAvailable = true;
	snapshot.isConnected = true;
	snapshot.isInVoiceChannel = inVoiceChannel;
	return snapshot;
}

DiscordSessionSnapshot unauthenticatedSession(const QString &pipeName) {
	DiscordSessionSnapshot snapshot;
	snapshot.pipeName = pipeName;
	snapshot.isAvailable = true;
	snapshot.lastError = "AUTH REQUIRED";
	return snapshot;
}
}

class DiscordIdentityTests : public QObject {
Q_OBJECT

private slots:
	void usesStableAccountIdsForAuthenticatedTargets();
	void preservesLabelsAndSelectionAcrossPipeSwaps();
	void ignoresMissingSavedTargetsInsteadOfCreatingGhosts();
	void usesTemporaryIdsForUnauthenticatedTargets();
	void storesAuthPerAccount();
};

void DiscordIdentityTests::usesStableAccountIdsForAuthenticatedTargets() {
	const DiscordTargetState state = buildDiscordTargetState({
		authenticatedSession("discord-ipc-4", "111", "Main"),
	}, {}, "111", "111");

	QCOMPARE(state.targets.size(), 1);
	QVERIFY(state.targets.contains("111"));
	QCOMPARE(state.targets.value("111").pipeName, QString("discord-ipc-4"));
	QCOMPARE(state.targets.value("111").displayName, QString("Main"));
	QCOMPARE(state.activeTargetId, QString("111"));
	QCOMPARE(state.primaryTargetId, QString("111"));
}

void DiscordIdentityTests::preservesLabelsAndSelectionAcrossPipeSwaps() {
	const QHash<QString, QString> labels{
		{"111", "Main / Stable"},
		{"222", "Alt / Canary"},
	};

	const DiscordTargetState state = buildDiscordTargetState({
		authenticatedSession("discord-ipc-1", "222", "Alt"),
		authenticatedSession("discord-ipc-0", "111", "Main"),
	}, labels, "222", "111");

	QCOMPARE(state.targets.value("111").displayName, QString("Main / Stable"));
	QCOMPARE(state.targets.value("222").displayName, QString("Alt / Canary"));
	QCOMPARE(state.targets.value("111").pipeName, QString("discord-ipc-0"));
	QCOMPARE(state.targets.value("222").pipeName, QString("discord-ipc-1"));
	QCOMPARE(state.activeTargetId, QString("222"));
	QCOMPARE(state.primaryTargetId, QString("111"));
}

void DiscordIdentityTests::ignoresMissingSavedTargetsInsteadOfCreatingGhosts() {
	const DiscordTargetState state = buildDiscordTargetState({
		authenticatedSession("discord-ipc-3", "555", "Only Account"),
	}, {}, "discord-ipc-0", "discord-ipc-1");

	QCOMPARE(state.targets.size(), 1);
	QVERIFY(state.targets.contains("555"));
	QCOMPARE(state.activeTargetId, QString("555"));
	QCOMPARE(state.primaryTargetId, QString("555"));
}

void DiscordIdentityTests::usesTemporaryIdsForUnauthenticatedTargets() {
	const DiscordTargetState state = buildDiscordTargetState({
		unauthenticatedSession("discord-ipc-0"),
	}, {
		{temporaryDiscordTargetId("discord-ipc-0"), "Should not apply"},
	}, {}, {});

	QCOMPARE(state.targets.size(), 1);
	const DiscordTarget target = state.targets.first();
	QVERIFY(isTemporaryDiscordTargetId(target.id));
	QCOMPARE(target.id, QString("pipe.discord-ipc-0"));
	QCOMPARE(target.displayName, QString("discord-ipc-0"));
	QCOMPARE(target.lastError, QString("AUTH REQUIRED"));
}

void DiscordIdentityTests::storesAuthPerAccount() {
	QTemporaryDir tempDir;
	QVERIFY(tempDir.isValid());

	const DiscordAuthStorage storage(tempDir.path());
	storage.saveAuthData("111", QJsonObject{
		{"access_token", "token-a"},
		{"refresh_token", "refresh-a"},
	});
	storage.saveAuthData("222", QJsonObject{
		{"access_token", "token-b"},
	});

	const QList<QJsonObject> authData = storage.loadAllAuthData();
	QCOMPARE(authData.size(), 2);
	QFileInfo authPath(storage.authPathForAccount("111"));
	QVERIFY(authPath.exists());

	QSet<QString> accessTokens;
	for(const QJsonObject &entry : authData)
		accessTokens.insert(entry.value("access_token").toString());

	QVERIFY(accessTokens == QSet<QString>{QString("token-a"), QString("token-b")});
}

QTEST_APPLESS_MAIN(DiscordIdentityTests)

#include "discord_identity_tests.moc"
