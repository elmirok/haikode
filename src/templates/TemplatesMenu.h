/*
 * Copyright The Genio Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 *
 * Parts are taken from the TemplatesMenu class from Haiku source code (Tracker) under the
 * Open Tracker Licence
 * Copyright (c) 1991-2000, Be Incorporated. All rights reserved.
 */

#ifndef _TEMPLATES_MENU_H
#define _TEMPLATES_MENU_H


#include <Menu.h>


class TemplatesMenu : public BMenu {
public:

	enum ViewMode {
		FILE_VIEW_MODE, // hide directories
		DIRECTORY_VIEW_MODE, // hide files and symlinks
		SHOW_ALL_VIEW_MODE, // show all items
		DISABLE_FILES_VIEW_MODE, // show all items, files are visible and disabled
		DISABLE_DIRECTORIES_VIEW_MODE // show all items, directories are visible and disabled
	};

							TemplatesMenu(BHandler *target, const char* label,
											BMessage *message, BMessage *showTemplateMessage,
											ViewMode mode = FILE_VIEW_MODE,
											bool showNewFolder = true);
	virtual 				~TemplatesMenu();

	virtual void 			AttachedToWindow();

	virtual status_t 		SetTargetForItems(BHandler* target);

	void 					UpdateMenuState();

	void					SetViewMode(ViewMode mode, bool enableNewFolder = true);
	void 					ShowNewFolder(bool show) { fShowNewFolder = show; }
	void 					EnableNewFolder(bool enable) { fEnableNewFolder = enable; }
	void					SetSender(const void* sender, const entry_ref* ref);

private:
	bool 					_BuildMenu();
	status_t				_BuildTemplateItem(const entry_ref& ref);

	BHandler* 				fTarget;
	BMenuItem* 				fOpenItem;
	BMessage*				fMessage;
	BMessage*				fShowTemplateMessage;
	ViewMode				fViewMode;
	bool					fShowNewFolder;
	bool					fEnableNewFolder;
	bool					fShowTemplatesDirectory;
};

#endif	// _TEMPLATES_MENU_H
