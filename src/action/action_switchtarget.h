#pragma once

#include "dvmaction.h"

class Action_SwitchTarget : public DVMAction {
Q_OBJECT

public:
	Action_SwitchTarget();

public:
	virtual void update() override;

protected:
	virtual void buildPropertyInspector(QStreamDeckPropertyInspectorBuilder &b) override;

private slots:
	void onInitialized();
	void onPressed();

private:
	int state_ = -1;
	QString title_;
};
