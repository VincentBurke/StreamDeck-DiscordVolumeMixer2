#include "action_microphone.h"

#include "dvmplugin.h"

Action_Microphone::Action_Microphone() {
	connect(this, &QStreamDeckAction::keyDown, this, &Action_Microphone::onPressed);
	connect(this, &QStreamDeckAction::keyUp, this, &Action_Microphone::onReleased);
}

void Action_Microphone::update() {
	if(const int newState = plugin()->isMicrophoneMuted(); state_ != newState) {
		state_ = newState;
		setState(newState);
	}
}

void Action_Microphone::onPressed() {
	if(DiscordSession *session = plugin()->activeSession())
		session->setMicrophoneMuted(!session->isMicrophoneMuted());
}

void Action_Microphone::onReleased() {
	setState(state_);
}
