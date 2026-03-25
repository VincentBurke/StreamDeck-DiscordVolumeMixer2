#include "action_switchtarget.h"

#include <qtstreamdeck2/qstreamdeckpropertyinspectorbuilder.h>

#include "dvmplugin.h"

namespace {
int targetIndexForId(const QList<DiscordTarget> &targets, const QString &targetId) {
	for(int i = 0; i < targets.size(); i++) {
		if(targets[i].id == targetId)
			return i;
	}

	return -1;
}

QString targetLabel(const DiscordTarget &target) {
	QString label = target.displayName;
	if(!target.cachedUser.username.isEmpty() && target.cachedUser.username != target.displayName)
		label += QStringLiteral(" [%1]").arg(target.cachedUser.username);

	label += QStringLiteral(" (%1)").arg(target.pipeName);
	return label;
}
}

Action_SwitchTarget::Action_SwitchTarget() {
	connect(this, &QStreamDeckAction::initialized, this, &Action_SwitchTarget::onInitialized);
	connect(this, &QStreamDeckAction::keyDown, this, &Action_SwitchTarget::onPressed);
}

void Action_SwitchTarget::update() {
	const DiscordTarget *target = plugin()->target(plugin()->activeTargetId());

	QString newTitle;
	int newState = 1;
	if(!target) {
		newTitle = "NO\nTARGET";
	}
	else {
		newTitle = target->displayName;
		if(!target->cachedUser.username.isEmpty() && target->cachedUser.username != target->displayName)
			newTitle += QStringLiteral("\n%1").arg(target->cachedUser.username);

		newState = (!target->isAvailable || !target->lastError.isEmpty()) ? 1 : 0;
	}

	if(title_ != newTitle) {
		title_ = newTitle;
		setTitle(newTitle);
	}

	if(state_ != newState) {
		state_ = newState;
		setState(state_);
	}
}

void Action_SwitchTarget::buildPropertyInspector(QStreamDeckPropertyInspectorBuilder &b) {
	const QList<DiscordTarget> targets = plugin()->targets();

	b.addSection("Switch behavior");
	b.addComboBox("switchMode", "Mode", {
		"Next available target",
		"Previous available target",
		"Specific target",
	}).linkWithActionSetting();
	b.addMessage("Manual switching is temporary. Voice-chat selection wins immediately if another target is currently chosen by the automatic rules.");

	if(targets.isEmpty()) {
		b.addMessage("No Discord targets discovered yet. Open Discord or Discord Canary to make target switching available.");
		DVMAction::buildPropertyInspector(b);
		return;
	}

	QStringList targetItems;
	targetItems.reserve(targets.size());
	for(const DiscordTarget &target : targets)
		targetItems += targetLabel(target);

	if(setting("switchMode").toInt() == 2) {
		auto &targetCombo = b.addComboBox("switchTargetSelector", "Target", targetItems);
		targetCombo.setValue(qMax(targetIndexForId(targets, setting("switchTargetId").toString()), 0));
		targetCombo.addValueChangedCallback([action = this, targets](const QVariant &value) {
			const int index = value.toInt();
			if(index >= 0 && index < targets.size())
				action->setSetting("switchTargetId", targets[index].id);
		});
	}

	if(const DiscordTarget *activeTarget = plugin()->target(plugin()->activeTargetId())) {
		b.addMessage(QStringList{
			QStringLiteral("Current active target: %1").arg(activeTarget->displayName),
			QStringLiteral("Primary target: %1").arg(plugin()->targetDisplayName(plugin()->primaryTargetId())),
			QStringLiteral("Pipe: %1").arg(activeTarget->pipeName),
		});
	}

	DVMAction::buildPropertyInspector(b);
}

void Action_SwitchTarget::onInitialized() {
	setSettingDefault("switchMode", 0);

	connect(plugin(), &DVMPlugin::targetsChanged, this, &QStreamDeckAction::updatePropertyInspector);
	connect(plugin(), &DVMPlugin::activeTargetChanged, this, &QStreamDeckAction::updatePropertyInspector);
	connect(plugin(), &DVMPlugin::globalSettingsChanged, this, &QStreamDeckAction::updatePropertyInspector);
}

void Action_SwitchTarget::onPressed() {
	const int mode = setting("switchMode").toInt();
	const QList<DiscordTarget> targets = plugin()->targets();

	if(targets.isEmpty()) {
		plugin()->connectToDiscord();
		return;
	}

	QString nextTargetId;
	if(mode == 2) {
		nextTargetId = setting("switchTargetId").toString();
	}
	else {
		QList<DiscordTarget> availableTargets;
		for(const DiscordTarget &target : targets) {
			if(target.isAvailable)
				availableTargets += target;
		}

		if(availableTargets.isEmpty()) {
			plugin()->connectToDiscord();
			return;
		}

		int currentIndex = targetIndexForId(availableTargets, plugin()->activeTargetId());
		if(currentIndex == -1)
			currentIndex = mode == 1 ? 0 : -1;

		const int direction = mode == 1 ? -1 : 1;
		const int nextIndex = (currentIndex + direction + availableTargets.size()) % availableTargets.size();
		nextTargetId = availableTargets[nextIndex].id;
	}

	if(nextTargetId.isEmpty())
		return;

	plugin()->setActiveTarget(nextTargetId);
	plugin()->connectToDiscord();
	plugin()->updateChannelMembersData();
}
