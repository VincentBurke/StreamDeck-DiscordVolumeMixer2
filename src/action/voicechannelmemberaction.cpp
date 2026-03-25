#include "voicechannelmemberaction.h"

#include <qtstreamdeck2/qstreamdeckpropertyinspectorbuilder.h>

#include "dvmplugin.h"
#include "dvmdevice.h"

VoiceChannelMemberAction::VoiceChannelMemberAction() {
	connect(this, &QStreamDeckAction::initialized, this, &VoiceChannelMemberAction::onInitialized);
}

void VoiceChannelMemberAction::onInitialized() {
	// Convert from string to int (legacy reasons)
	if(const auto v = setting("user_ix"); v.isDouble())
		setSetting("user_ix", QString::number(v.toInt()));
}

VoiceChannelMemberAction::VoiceChannelMemberResult VoiceChannelMemberAction::voiceChannelMember() {
	QMap<QString, VoiceChannelMember> *members = plugin()->activeVoiceChannelMembers();
	if(!members)
		return {};

	auto &lst = *members;
	if(lst.isEmpty()) {
		device()->voiceChannelMemberIndexOffset = 0;
		return {};
	}

	if(device()->voiceChannelMemberIndexOffset >= lst.size())
		device()->voiceChannelMemberIndexOffset = 0;

	QString userID = setting("user_ix").toString();

	VoiceChannelMemberResult r;

	// If the ID length is too low, that means that it's index in the list, not the actual user ID
	if(userID.length() < 4) {
		r.userIndex = userID.toInt() + device()->voiceChannelMemberIndexOffset;
		userID = lst.keys().value(r.userIndex);
	}

	auto it = lst.find(userID);
	r.mem = it != lst.end() ? &*it : nullptr;
	return r;
}

void VoiceChannelMemberAction::buildPropertyInspector(QStreamDeckPropertyInspectorBuilder &b) {
	b.addLineEdit("user_ix", "User index or ID").linkWithActionSetting();
	b.addMessage(QStringList{
		"Index in the channel users list, indexed from 0.",
		"Alternatively, you can also use User ID (right click user -> copy ID) to set it to a specific user."
	});

	DVMAction::buildPropertyInspector(b);
}
