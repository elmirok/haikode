/*
 * Copyright 2025, Andrea Anzani
 * All rights reserved. Distributed under the terms of the MIT license.
 */


#include "TerminalTab.h"

#include <Looper.h>
#include <Messenger.h>

#include <cstdio>
#include <sys/wait.h>

#include "Log.h"
#include "TerminalManager.h"


TerminalTab::TerminalTab(bool autorestart)
	:
	BView("Terminal", B_FRAME_EVENTS),
	fTermView(nullptr),
	fCommand(""),
	fThemeName(""),
	fAutorestart(autorestart)
{
	SetInitialCommand("/bin/bash -l");
	SetResizingMode(B_FOLLOW_ALL);
}


void
TerminalTab::FrameResized(float w, float h)
{
	if (fTermView) {
		fTermView->ResizeTo(w, h);
	}
	BView::FrameResized(w, h);
}


void
TerminalTab::AttachedToWindow()
{
	BView::AttachedToWindow();
	if (fTermView == nullptr) {
		fTermView = TerminalManager::CreateNewTerminal(BRect(0, 0, 100,100), BMessenger(this), fCommand, fThemeName);
		fTermView->SetResizingMode(B_FOLLOW_NONE);
		fTermView->SetExplicitMinSize(BSize(100, 100));
		fTermView->SetExplicitPreferredSize(BSize(100, 100));
		fTermView->SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNLIMITED));
		AddChild(fTermView);
	}
}


void
TerminalTab::MessageReceived(BMessage* msg)
{
	if (msg->what == 'NOTM') {
		int status = -1;
		pid_t pid = msg->GetInt32("pid", -1);
		if (waitpid(pid, &status, WNOHANG) > 0) {
			NotifyCommandQuit(WIFEXITED(status), WEXITSTATUS(status));
		} else {
			NotifyCommandQuit(false, 255);
		}
		return;
	}
	BView::MessageReceived(msg);
}


void
TerminalTab::NotifyCommandQuit(bool exitNormal, int exitStatus)
{
	if (exitNormal && exitStatus == 0) {
		LogDebug("/* the program terminated normally and executed successfully */\n");
	} else if (exitNormal && exitStatus != 0) {
		LogDebug("/* the program terminated normally, but returned a non-zero status */\n");
	} else {
		LogDebug("/* the program didn't terminate normally */\n");
	}

	if (fAutorestart)
		_RunCommand(fCommand, true);
}


void
TerminalTab::SetInitialCommand(const char* command)
{
	fCommand = command;
}


void
TerminalTab::SetInitialTheme(const char* themeName)
{
	fThemeName = themeName;
}


status_t
TerminalTab::_RunCommand(const char* cmd, bool clean)
{
	//temporary big hack!
	BView* target = _FindTarget();
	if (target == nullptr)
		return B_ERROR;

	BMessage exec(B_EXECUTE_PROPERTY);
	exec.AddSpecifier("command");
	exec.AddString("argv", "/bin/sh");
	exec.AddString("argv", "-c");
	exec.AddString("argv", cmd);
	exec.AddBool("clear", clean);

	return Looper()->PostMessage(&exec, target);
}


BView*
TerminalTab::_FindTarget() //ugly hack
{
	//We need a more deterministic way to find the view!
	if (fTermView && fTermView->ChildAt(0) && fTermView->ChildAt(0)->ChildAt(0))
		return fTermView->ChildAt(0)->ChildAt(0);

	return nullptr;
}
