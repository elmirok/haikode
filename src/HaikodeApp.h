#ifndef HAIKODE_APP_H
#define HAIKODE_APP_H

#include <Application.h>

class MainWindow;

class HaikodeApp final : public BApplication {
public:
	HaikodeApp();
	void ReadyToRun() override;

private:
	MainWindow* fMainWindow;
};

#endif

