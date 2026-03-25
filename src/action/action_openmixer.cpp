#include "action_openmixer.h"

#include <qtstreamdeck2/qstreamdeckpropertyinspectorbuilder.h>

#include "dvmplugin.h"

namespace {
QString targetStateLabel(const DiscordTarget &target) {
	if(!target.isAvailable)
		return "Closed";

	if(target.isInVoiceChannel)
		return "In VC";

	return "Idle";
}

QString targetStatusLabel(const DiscordTarget &target) {
	const QString stateLabel = targetStateLabel(target);
	if(target.isAvailable && !target.lastError.isEmpty())
		return QStringLiteral("%1 (%2)").arg(stateLabel, target.lastError);

	return stateLabel;
}

QString targetComboLabel(const DiscordTarget &target, const QString &activeTargetId, const QString &primaryTargetId) {
	QString label = target.displayName;
	if(!target.cachedUser.username.isEmpty() && target.cachedUser.username != target.displayName)
		label += QStringLiteral(" [%1]").arg(target.cachedUser.username);

	label += QStringLiteral(" (%1)").arg(target.pipeName);
	if(target.id == primaryTargetId)
		label += " - Primary";
	if(target.id == activeTargetId)
		label += " - Active";

	label += QStringLiteral(" - %1").arg(targetStatusLabel(target));
	return label;
}

int targetIndexForId(const QList<DiscordTarget> &targets, const QString &targetId) {
	for(int i = 0; i < targets.size(); i++) {
		if(targets[i].id == targetId)
			return i;
	}

	return targets.isEmpty() ? 0 : -1;
}
}

Action_OpenMixer::Action_OpenMixer() {
	connect(this, &QStreamDeckAction::initialized, this, &Action_OpenMixer::onInitialized);
	connect(this, &QStreamDeckAction::keyDown, this, &Action_OpenMixer::onPressed);
}

void Action_OpenMixer::update() {
}

void Action_OpenMixer::buildPropertyInspector(QStreamDeckPropertyInspectorBuilder &b) {
	const QList<DiscordTarget> targets = plugin()->targets();
	const QString activeTargetId = plugin()->activeTargetId();
	const QString primaryTargetId = plugin()->primaryTargetId();

	b.addSection("Discord credentials");
	b.addLineEdit("client_id", "Client ID").linkWithGlobalSetting();
	b.addLineEdit("client_secret", "Client secret").linkWithGlobalSetting();
	b.addMessage("Shared credentials are reused for every discovered Discord target.");
	b.addMessage("For setup instructions, see the <a href=\"javascript: openUrl('https://github.com/CZDanol/StreamDeck-DiscordVolumeMixer2');\">GitHub page</a>.");

	b.addSection("Automatic selection");
	b.addMessage("The plugin follows whichever account is currently in voice chat. If multiple accounts are in voice chat, the primary target wins.");

	b.addSection("Mixer target");
	b.addComboBox("targetRoutingMode", "Routing", {
		"Follow active target",
		"Pinned target",
	}).linkWithActionSetting();

	if(targets.isEmpty()) {
		b.addMessage("No Discord targets discovered yet. Open Discord or Discord Canary and this list will populate automatically.");
		DVMAction::buildPropertyInspector(b);
		return;
	}

	QStringList targetItems;
	targetItems.reserve(targets.size());
	for(const DiscordTarget &target : targets)
		targetItems += targetComboLabel(target, activeTargetId, primaryTargetId);

	{
		auto &primaryCombo = b.addComboBox("primaryTargetSelector", "Primary target", targetItems);
		primaryCombo.setValue(qMax(targetIndexForId(targets, primaryTargetId), 0));
		primaryCombo.addValueChangedCallback([plugin = plugin(), targets](const QVariant &value) {
			const int index = value.toInt();
			if(index >= 0 && index < targets.size())
				plugin->setPrimaryTarget(targets[index].id);
		});
	}

	if(setting("targetRoutingMode").toInt() == 1) {
		auto &pinnedCombo = b.addComboBox("pinnedTargetSelector", "Pinned target", targetItems);
		pinnedCombo.setValue(qMax(targetIndexForId(targets, setting("pinnedTargetId").toString()), 0));
		pinnedCombo.addValueChangedCallback([action = this, targets](const QVariant &value) {
			const int index = value.toInt();
			if(index >= 0 && index < targets.size())
				action->setSetting("pinnedTargetId", targets[index].id);
		});
	}

	if(const DiscordTarget *activeTarget = plugin()->target(activeTargetId)) {
		QStringList summary{
			QStringLiteral("Active target: %1").arg(activeTarget->displayName),
			QStringLiteral("Primary target: %1").arg(plugin()->targetDisplayName(primaryTargetId)),
			QStringLiteral("Pipe: %1").arg(activeTarget->pipeName),
			QStringLiteral("Status: %1").arg(targetStatusLabel(*activeTarget)),
		};
		if(!activeTarget->cachedUser.username.isEmpty())
			summary += QStringLiteral("Account: %1").arg(activeTarget->cachedUser.username);

		b.addMessage(summary);
	}

	b.addSection("Target labels");
	for(const DiscordTarget &target : targets) {
		auto &labelInput = b.addLineEdit(QStringLiteral("targetLabel_%1").arg(target.id), target.pipeName);
		labelInput.setValue(plugin()->targetLabel(target.id));
		labelInput.addValueChangedCallback([plugin = plugin(), targetId = target.id](const QVariant &value) {
			plugin->setTargetDisplayName(targetId, value.toString());
		});

		QStringList targetInfo{
			QStringLiteral("Shown as: %1").arg(target.displayName),
			QStringLiteral("State: %1").arg(targetStatusLabel(target)),
		};
		if(!target.cachedUser.username.isEmpty())
			targetInfo += QStringLiteral("Authenticated account: %1").arg(target.cachedUser.username);

		b.addMessage(targetInfo);
	}

	DVMAction::buildPropertyInspector(b);
}

void Action_OpenMixer::onInitialized() {
	setSettingDefault("targetRoutingMode", 0);

	connect(plugin(), &DVMPlugin::targetsChanged, this, &QStreamDeckAction::updatePropertyInspector);
	connect(plugin(), &DVMPlugin::activeTargetChanged, this, &QStreamDeckAction::updatePropertyInspector);
	connect(plugin(), &DVMPlugin::globalSettingsChanged, this, &QStreamDeckAction::updatePropertyInspector);
}

void Action_OpenMixer::onPressed() {
	using DT = QStreamDeckDevice::DeviceType;
	static const QMap<int, QString> profileNameByDeviceType{
		{+DT::streamDeck,       "Discord Volume Mixer"},
		{+DT::streamDeckMini,   "Discord Volume Mixer Mini"},
		{+DT::streamDeckXL,     "Discord Volume Mixer XL"},
		{+DT::streamDeckMobile, "Discord Volume Mixer"},
		{+DT::streamDeckPlus,   "Discord Volume Mixer+"},
	};

	plugin()->resolveMixerTarget(setting("pinnedTargetId").toString(), setting("targetRoutingMode").toInt() == 1);
	plugin()->connectToDiscord();

	device()->switchToProfile(profileNameByDeviceType.value(+device()->deviceType(), "Discord Volume Mixer"));
	plugin()->updateChannelMembersData();
}
