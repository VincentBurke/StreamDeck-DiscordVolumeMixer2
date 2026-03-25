#pragma once

#include "dvmaction.h"

class Action_OpenMixer : public DVMAction {
Q_OBJECT

public:
	Action_OpenMixer();

public slots:
	virtual void update() override;

protected:
	virtual void buildPropertyInspector(QStreamDeckPropertyInspectorBuilder &b) override;

private slots:
	void onInitialized();
	void onPressed();

};
