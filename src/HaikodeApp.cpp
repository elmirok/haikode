#include "HaikodeApp.h"

#include "MainWindow.h"

static const char* kAppSignature = "application/x-vnd.haikode";

HaikodeApp::HaikodeApp()
	:
	BApplication(kAppSignature),
	fMainWindow(nullptr)
{
}

void
HaikodeApp::ReadyToRun()
{
	fMainWindow = new MainWindow();
	fMainWindow->Show();
}

