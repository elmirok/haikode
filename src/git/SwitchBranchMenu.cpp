/*
 * Copyright The Genio Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * Parts are taken from the TemplatesMenu class from Haiku (Tracker) under the
 * Open Tracker Licence
 * Copyright (c) 1991-2000, Be Incorporated. All rights reserved.
 */

#include "SwitchBranchMenu.h"

#include <MenuItem.h>

#include "GenioApp.h"
#include "GenioWindow.h"
#include "GitRepository.h"
#include "ProjectFolder.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "SwitchBranchMenu"


SwitchBranchMenu::SwitchBranchMenu(BHandler *target, const char* label,
									BMessage *message, const char *projectPath)
	:
	BMenu(label),
	fTarget(target),
	fMessage(message),
	fProjectPath(projectPath)
{
	SetRadioMode(true);
}


SwitchBranchMenu::~SwitchBranchMenu()
{
	delete fMessage;
}


void
SwitchBranchMenu::AttachedToWindow()
{
	BString projectPath = fProjectPath;
	if (projectPath.IsEmpty()) {
		auto activeProject = gMainWindow->GetActiveProject();
		if (activeProject != nullptr)
			projectPath = activeProject->Path().String();
	}
	_BuildMenu(projectPath);

	// This has to be done AFTER _BuildMenu, since
	// it layouts the menu and resizes the window.
	// So we need to have the menuitems already added.
	BMenu::AttachedToWindow();

	SetTargetForItems(fTarget);
}


bool
SwitchBranchMenu::_BuildMenu(const BString& projectPath)
{
	// clear everything...
	int32 count = CountItems();
	RemoveItems(0, count, true);

	if (!projectPath.IsEmpty()) {
		Genio::Git::GitRepository* repo = nullptr;
		try {
			repo = new Genio::Git::GitRepository(projectPath);
			auto branches = repo->GetBranches();
			auto currentBranch = repo->GetCurrentBranch();
			for (auto &branch : branches) {
				BMessage *message = new BMessage(fMessage->what);
				message->AddString("branch", branch);
				message->AddString("project_path", projectPath);
				auto item = new BMenuItem(branch, message);
				AddItem(item);
				if (branch == currentBranch)
					item->SetMarked(true);
			}
		} catch(...) {
		}
		delete repo;
	}
	return count > 0;
}
