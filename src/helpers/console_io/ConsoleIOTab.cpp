/*
 * Copyright 2025, Andrea Anzani
 * All rights reserved. Distributed under the terms of the MIT license.
 */


#include "ConsoleIOTab.h"
#include "ConsoleIOThread.h"
#include <Catalog.h>
#include <Looper.h>
#include <cstdio>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "ConsoleIOView"

ConsoleIOTab::ConsoleIOTab(const char* name, BMessenger messenger, BString theme):
	TerminalTab(), fMessenger(messenger)
{
	SetName(name);
	SetInitialCommand("/bin/sh -c \":\"");
	SetInitialTheme(theme);
}


void
ConsoleIOTab::Clear()
{
	BView*	target = _FindTarget();
	if (target == nullptr)
		return;
	BMessage msg('clea');
	Looper()->PostMessage(&msg, target, this);
};

void
ConsoleIOTab::SetTheme(BString theme)
{
	BView*	target = _FindTarget();
	if (target == nullptr)
		return;
	BMessage msg('teme');
	msg.AddString("theme", theme);
	Looper()->PostMessage(&msg, target, this);
};


status_t
ConsoleIOTab::Stop()
{
	BMessage stop;
	BString cmd(":");
	cmd << "\n" << _BannerCommand(fContextMessage.GetString("banner_claim", "command"), "STOPPED   ", true);
	stop.AddString("cmd", cmd);
	stop.AddBool("internalStop", true);
	return RunCommand(&stop, false, false);
};


status_t
ConsoleIOTab::RunCommand(BMessage* message, bool clean, bool notifyMessage)
{
	BString cmd;
	if (notifyMessage) {
		cmd << _BannerCommand(message->GetString("banner_claim", "command"), "started   ", false) << "\n";
	}
	cmd << message->GetString("cmd", "echo error");
	if (notifyMessage) {
		cmd << "\n" << _BannerCommand(message->GetString("banner_claim", "command"), "ended     ", true);
	}

	if (notifyMessage)
		fContextMessage = *message;

	return _RunCommand(cmd.String(), clean);
}

BString
ConsoleIOTab::_BannerCommand(BString claim, BString status, bool ending)
{
	BString banner("PRET=$?; echo '");
	banner  << "--------------------------------"
			<< "   "
			<< claim
			<< " "
			<< status
			<< "--------------------------------"
			<< "'";
	if (ending)
		banner << "; exit $PRET";
	return banner;
}

void
ConsoleIOTab::NotifyCommandQuit(bool exitNormal, int exitStatus)
{
	status_t status = exitNormal ? ( exitStatus == 0 ? B_OK : B_ERROR) : B_ERROR;

	if(fContextMessage.IsEmpty() == false) {
		BMessage notification = fContextMessage;
		fContextMessage.MakeEmpty();

		notification.what = CONSOLEIOTHREAD_EXIT;
		notification.AddInt32("status", status);
		notification.what = CONSOLEIOTHREAD_EXIT;
		notification.AddInt32("status", status);
		fMessenger.SendMessage(&notification);
	}
}

