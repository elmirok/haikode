/*
 * Copyright 2025, Stefano Ceccherini
 * All rights reserved. Distributed under the terms of the MIT license.
 */


#include "ProjectMenuField.h"

#include <Catalog.h>
#include <NaturalCompare.h>
#include <Window.h>

#include "NoticeMessages.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "ProjectMenuField"

using namespace Genio::UI;

ProjectMenuField::ProjectMenuField(const char* name, int32 what, uint32 flags)
	:
	OptionList(name, B_TRANSLATE("Project:"),
		B_TRANSLATE("Choose project" B_UTF8_ELLIPSIS), true, flags),
	fWhat(what)
{
}


/* virtual */
void
ProjectMenuField::AttachedToWindow()
{
	OptionList::AttachedToWindow();
	if (Window()->LockLooper()) {
		Window()->StartWatching(this, MSG_NOTIFY_PROJECT_LIST_CHANGED);
		Window()->StartWatching(this, MSG_NOTIFY_PROJECT_SET_ACTIVE);
		Window()->UnlockLooper();
	}
}


/* virtual */
void
ProjectMenuField::DetachedFromWindow()
{
	OptionList::DetachedFromWindow();
	if (Window()->LockLooper()) {
		Window()->StopWatching(this, MSG_NOTIFY_PROJECT_LIST_CHANGED);
		Window()->StopWatching(this, MSG_NOTIFY_PROJECT_SET_ACTIVE);
		Window()->UnlockLooper();
	}
}


/* virtual */
void
ProjectMenuField::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case B_OBSERVER_NOTICE_CHANGE:
		{
			int32 code;
			message->FindInt32(B_OBSERVE_WHAT_CHANGE, &code);
			switch (code) {
				case MSG_NOTIFY_PROJECT_LIST_CHANGED:
					_HandleProjectListChanged(message);
					break;
				case MSG_NOTIFY_PROJECT_SET_ACTIVE:
					_HandleActiveProjectChanged(message);
					break;
				default:
					OptionList::MessageReceived(message);
					break;
			}
			break;
		}
		default:
			OptionList::MessageReceived(message);
			break;
	}
}


void
ProjectMenuField::_HandleProjectListChanged(const BMessage* message)
{
	// The logic here: save the currently selected project, empty the list
	// then rebuild the list and try to reselect the previously selected project.
	// otherwise select the active project.
	BMenu* projectMenu = Menu();

	BString selectedProject;
	BMenuItem* selectedItem = projectMenu->FindMarked();
	if (selectedItem != nullptr) {
		selectedProject = selectedItem->Label();
	}

	Window()->BeginViewTransaction();

	MakeEmpty();

	int32 index = 0;
	for (;;) {
		BString projectPath;
		BString projectName;
		if (message->FindString("project_path", index, &projectPath) != B_OK)
			break;
		if (message->FindString("project_name", index, &projectName) != B_OK)
			break;
		bool marked = projectName == selectedProject;
		AddItem(projectName, projectPath, fWhat, true, marked);

		index++;
	}

	if (projectMenu->FindMarked() == nullptr) {
		BMenuItem* item = nullptr;
		BString activeProjectName;
		if (message->FindString("active_project_name", &activeProjectName) == B_OK)
			item = projectMenu->FindItem(activeProjectName);
		else
			item = projectMenu->ItemAt(0);
		if (item != nullptr) {
			item->SetMarked(true);
			item->Messenger().SendMessage(item->Message());
		}
	}

	Window()->EndViewTransaction();
}


void
ProjectMenuField::_HandleActiveProjectChanged(const BMessage* message)
{
	BString selectedProjectName;
	BMenuItem* item = Menu()->FindMarked();
	if (item != nullptr) {
		selectedProjectName = item->Label();
	}

	// TODO: use project path, since name could be duplicated
	const BString activeProjectName = message->GetString("active_project_name");
	bool changed = false;
	if (activeProjectName != selectedProjectName) {
		item = Menu()->FindItem(activeProjectName);
		if (item == nullptr) {
			// The active project is not present in the menu.
			// That means we have not yet received the "project list changed"
			// message. Just add the active project here.
			BString projectPath;
			BString projectName;
			if (message->FindString("active_project_path", &projectPath) == B_OK &&
					message->FindString("active_project_name", &projectName) == B_OK) {
				AddItem(projectName, projectPath, fWhat, true, true);
				item = Menu()->FindItem(activeProjectName);
			}
		}
		if (item != nullptr) {
			changed = !item->IsMarked();
			item->SetMarked(true);
		}
	}
	if (changed) {
		if (item != nullptr)
			item->Messenger().SendMessage(item->Message());
	}
}
