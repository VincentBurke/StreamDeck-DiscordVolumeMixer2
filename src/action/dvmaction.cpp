#include "dvmaction.h"

#include <qtstreamdeck2/qstreamdeckpropertyinspectorbuilder.h>

#include "dvmplugin.h"

DVMAction::DVMAction() {
	connect(this, &QStreamDeckAction::initialized, this, &DVMAction::onInitialized);
	connect(this, &QStreamDeckAction::settingsChanged, this, &DVMAction::update);
	connect(this, &QStreamDeckAction::settingsChanged, this, &QStreamDeckAction::updatePropertyInspector);
}

void DVMAction::buildPropertyInspector(QStreamDeckPropertyInspectorBuilder &b) {
	Q_UNUSED(b);
}

void DVMAction::onInitialized() {
	connect(plugin(), &DVMPlugin::buttonsUpdateRequested, this, &DVMAction::update);
	connect(plugin(), &DVMPlugin::globalSettingsChanged, this, &DVMAction::update);

	QTimer::singleShot(0, this, &DVMAction::update);
}
