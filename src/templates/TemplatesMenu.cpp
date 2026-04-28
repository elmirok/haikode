/*
 * Copyright The Genio Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * Parts are taken from the TemplatesMenu class from Haiku (Tracker) under the
 * Open Tracker Licence
 * Copyright (c) 1991-2000, Be Incorporated. All rights reserved.
 */

#include "TemplatesMenu.h"

#include <Catalog.h>
#include <Directory.h>
#include <MenuItem.h>
#include <MimeTypes.h>

#include "IconMenuItem.h"
#include "TemplateManager.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "TemplatesMenu"


const char* kNewFolderLabel = "New folder";

TemplatesMenu::TemplatesMenu(BHandler *target, const char* label,
								BMessage *message, BMessage *showTemplateMessage,
								ViewMode mode,
								bool showNewFolder)
	:
	BMenu(label),
	fTarget(target),
	fOpenItem(NULL),
	fMessage(message),
	fShowTemplateMessage(showTemplateMessage),
	fViewMode(mode),
	fShowNewFolder(showNewFolder),
	fEnableNewFolder(true),
	fShowTemplatesDirectory(fShowTemplateMessage != nullptr ? true : false)
{
}


TemplatesMenu::~TemplatesMenu()
{
}


void
TemplatesMenu::AttachedToWindow()
{
	_BuildMenu();
	BMenu::AttachedToWindow();
	SetTargetForItems(fTarget);
}


status_t
TemplatesMenu::SetTargetForItems(BHandler* target)
{
	status_t result = BMenu::SetTargetForItems(target);
	if (fOpenItem)
		fOpenItem->SetTarget(target);

	return result;
}


void
TemplatesMenu::UpdateMenuState()
{
	_BuildMenu();
}


void
TemplatesMenu::SetViewMode(ViewMode mode, bool enableNewFolder)
{
	fViewMode = mode;
	fEnableNewFolder = enableNewFolder;
}


void
TemplatesMenu::SetSender(const void* sender, const entry_ref* ref)
{
	fMessage->RemoveName("sender");
	fMessage->AddPointer("sender", sender);
	fMessage->RemoveName("sender_ref");
	fMessage->AddRef("sender_ref", ref);
}


// BMessages are passed in the constructor for each of the events:
// * Create new item
// * Show user template folder
// WARNING: Only the what field of the passed message is used to always generate a new message
//
// * Create new item -> New (empty) folder
// what: specified by the caller in the passed BMessage
// type (String): "new_folder"
//
// * Create new item -> New folder template (typically used for project templates)
// what: specified by the caller in the passed BMessage
// type (String): "new_folder_template"
// refs (entry_ref): the entry_ref of the folder
//
// * Create new item -> New folder template (typically used for file templates)
// what: specified by the caller in the passed BMessage
// type (String): "new_file_template"
// refs (entry_ref): the entry_ref of the folder
//
// * Show user template folder -> Send a message to the target to show the user template folder
// (typically /boot/home/config/settings/Genio/templates)
// what: specified by the caller in the passed BMessage
// refs (entry_ref): the entry_ref of the folder
//
bool
TemplatesMenu::_BuildMenu()
{
	// clear everything...
	fOpenItem = nullptr;

	RemoveItems(0, CountItems(), true);

	if (fShowNewFolder) {
		// Always create a new message
		// TODO: Why ?

		void* sender = nullptr;
		fMessage->FindPointer("sender", &sender);
		entry_ref senderRef;
		fMessage->FindRef("sender_ref", &senderRef);
		BMessage *message = new BMessage(fMessage->what);
		message->AddString("type","new_folder");
		message->AddPointer("sender", sender);
		message->AddRef("sender_ref", &senderRef);

		// add the folder
		IconMenuItem* menuItem = new IconMenuItem(B_TRANSLATE(kNewFolderLabel),
			message, B_DIRECTORY_MIME_TYPE, B_MINI_ICON);
		menuItem->SetEnabled(fEnableNewFolder);
		AddItem(menuItem);
		menuItem->SetShortcut('N', 0);
		AddSeparatorItem();
	}

	entry_list entryList;
	TemplateManager::Get()->GetTemplatesList(entryList);
	for (const entry_ref& entry : entryList) {
		_BuildTemplateItem(entry);
	}

	int32 templatesCount = entryList.size();
	if (templatesCount > 0)
		AddSeparatorItem();

	entry_list userEntryList;
	TemplateManager::Get()->GetUserTemplatesList(userEntryList);
	for (const entry_ref& userEntry : userEntryList) {
		_BuildTemplateItem(userEntry);
	}

	int32 userTemplatesCount = userEntryList.size();
	if (fShowTemplatesDirectory) {
		if (userTemplatesCount > 0)
			AddSeparatorItem();

		// this is the message sent to open the templates folder
		BMessage *templateMessage = new BMessage(fShowTemplateMessage->what);
		entry_ref dirRef;
		BDirectory userTemplateDirectory(TemplateManager::GetUserTemplateDirectory());
		BEntry entry;
		if (userTemplateDirectory.GetEntry(&entry) == B_OK)
			entry.GetRef(&dirRef);
		templateMessage->AddRef("refs", &dirRef);

		// add item to show templates folder
		fOpenItem = new BMenuItem(B_TRANSLATE("Edit user templates" B_UTF8_ELLIPSIS),
			templateMessage);
		AddItem(fOpenItem);
		if (dirRef == entry_ref())
			fOpenItem->SetEnabled(false);
	}

	return templatesCount + userTemplatesCount > 0;
}


status_t
TemplatesMenu::_BuildTemplateItem(const entry_ref& itemRef)
{
	bool itemEnabled = true;

	BEntry entry(&itemRef);
	BNode node(&entry);
	BNodeInfo nodeInfo(&node);

	// ViewMode can be changed at any time before the menu is invoked to show/hide folder or
	// directories or both depending on the context and the item selected to perform the
	// operation on
	if (fViewMode == DIRECTORY_VIEW_MODE && !entry.IsDirectory())
		return B_ERROR;
	if (fViewMode == FILE_VIEW_MODE && entry.IsDirectory())
		return B_ERROR;
	if (fViewMode == DISABLE_FILES_VIEW_MODE && !entry.IsDirectory())
		itemEnabled = false;
	if (fViewMode == DISABLE_DIRECTORIES_VIEW_MODE && entry.IsDirectory())
		itemEnabled = false;

	char fileName[B_FILE_NAME_LENGTH];
	entry.GetName(fileName);
	if (nodeInfo.InitCheck() == B_OK) {
		char mimeType[B_MIME_TYPE_LENGTH];
		nodeInfo.GetType(mimeType);

		BMimeType mime(mimeType);
		if (mime.IsValid()) {
			entry_ref ref;
			entry.GetRef(&ref);
			void* sender = nullptr;
			fMessage->FindPointer("sender", &sender);
			entry_ref senderRef;
			fMessage->FindRef("sender_ref", &senderRef);
			BMessage *message = new BMessage(fMessage->what);
			message->AddRef("refs", &ref);
			message->AddPointer("sender", sender);
			message->AddRef("sender_ref", &senderRef);
			if (entry.IsDirectory())
				message->AddString("type", "new_folder_template");
			else
				message->AddString("type", "new_file_template");
			auto item = new IconMenuItem(fileName, message, &nodeInfo, B_MINI_ICON);
			item->SetEnabled(itemEnabled);
			AddItem(item);
		}
	}

	return B_OK;
}
