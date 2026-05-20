/*
 * Copyright 2023, Andrea Anzani
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "EditorContextMenu.h"

#include <Autolock.h>
#include <Debug.h>
#include <Catalog.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <Window.h>

#include "ActionManager.h"
#include "Editor.h"
#include "EditorMessages.h"
#include "GenioWindowMessages.h"
#include "GMessage.h"
#include "LSPEditorWrapper.h"
#include "ProjectBrowser.h"

#undef  B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "GenioWindow"

BPopUpMenu* EditorContextMenu::sMenu = nullptr;
BPopUpMenu* EditorContextMenu::sFixMenu = nullptr;


EditorContextMenu::EditorContextMenu()
{
}


/* static */
void
EditorContextMenu::Show(Editor* editor, BPoint point)
{
	ASSERT(editor != nullptr);

	BAutolock l(editor->Looper());

	bool isFindInBrowserEnabled = ActionManager::IsEnabled(MSG_FIND_IN_BROWSER);
	bool isShowInTrackerEnabled = ActionManager::IsEnabled(MSG_PROJECT_MENU_SHOW_IN_TRACKER);
	bool isOpenInTerminalEnabled = ActionManager::IsEnabled(MSG_PROJECT_MENU_OPEN_TERMINAL);

	LSPEditorWrapper* outLsp = nullptr;
	BPopUpMenu* menu = _GetCodeActionsMenu(editor, point, outLsp);
	if (menu == nullptr) {
		// If no action menu, just show the regular menu
		menu = _GetStandardMenu(editor, point);
	}
	ASSERT(menu != nullptr);
	menu->Go(point, true, false, false);

	ActionManager::SetEnabled(MSG_PROJECT_MENU_SHOW_IN_TRACKER, isShowInTrackerEnabled);
	ActionManager::SetEnabled(MSG_PROJECT_MENU_OPEN_TERMINAL, isOpenInTerminalEnabled);
	ActionManager::SetEnabled(MSG_FIND_IN_BROWSER, isFindInBrowserEnabled);
}


/* static */
void
EditorContextMenu::_PopulateFixMenu(BPopUpMenu* fixMenu, LSPEditorWrapper* lsp,
								Editor* editor, LSPDiagnostic& dia, int32 index)
{
	fixMenu->RemoveItems(0, fixMenu->CountItems(), true);
	auto& actionsArray = dia.codeActions.value().array();
	for (int i = 0; i < static_cast<int>(actionsArray.size()); i++) {
		std::string title = actionsArray[i].object().get("title").string();
		auto item = new BMenuItem(title.c_str(),
			new GMessage({{"what", kApplyFix},
					{"index", index}, {"action", i}, {"quickFix", true}}));
		fixMenu->AddItem(item);
		fixMenu->SetTargetForItems(editor);
	}
	lsp->EndHover();
}


/* static */
BPopUpMenu*
EditorContextMenu::_GetCodeActionsMenu(Editor* editor, BPoint screenPoint, LSPEditorWrapper*& outLsp)
{
	outLsp = editor->GetLSPEditorWrapper();
	if (outLsp == nullptr)
		return nullptr;

	LSPDiagnostic dia;
	BPoint p = editor->ConvertFromScreen(screenPoint);
	Sci_Position sci_position = editor->SendMessage(SCI_POSITIONFROMPOINT, uptr_t(p.x), sptr_t(p.y));
	int32 index = outLsp->DiagnosticFromPosition(sci_position, dia);
	if (index > -1 && dia.codeActions.has_value()
		&& dia.codeActions->isArray() && !dia.codeActions->array().empty()) {
		if (!sFixMenu) {
			sFixMenu = new BPopUpMenu("FixEditorContextMenu", false, false);
			sFixMenu->AddItem(new BMenuItem("Fix!", nullptr));
		}

		_PopulateFixMenu(sFixMenu, outLsp, editor, dia, index);
		return sFixMenu;
	}

	return nullptr;
}


/* static */
BPopUpMenu*
EditorContextMenu::_GetStandardMenu(Editor* editor, BPoint screenPoint)
{
	if (sMenu == nullptr) {
		sMenu = new BPopUpMenu("EditorContextMenu", false, false);

		ActionManager::AddItem(B_UNDO, sMenu);
		ActionManager::AddItem(B_REDO, sMenu);

		sMenu->AddSeparatorItem();

		ActionManager::AddItem(B_CUT, sMenu);
		ActionManager::AddItem(B_COPY, sMenu);
		ActionManager::AddItem(B_PASTE, sMenu);
		ActionManager::AddItem(MSG_TEXT_DELETE, sMenu);

		sMenu->AddSeparatorItem();

		ActionManager::AddItem(B_SELECT_ALL, sMenu);

		sMenu->AddSeparatorItem();

		ActionManager::AddItem(MSG_GOTODEFINITION, sMenu);
		ActionManager::AddItem(MSG_GOTODECLARATION, sMenu);
		ActionManager::AddItem(MSG_GOTOIMPLEMENTATION, sMenu);
		ActionManager::AddItem(MSG_FIND_REFERENCES, sMenu);

		sMenu->AddSeparatorItem();

		ActionManager::AddItem(MSG_RENAME, sMenu);

		sMenu->AddSeparatorItem();

		ActionManager::AddItem(MSG_FIND_IN_BROWSER, sMenu);

		entry_ref ref;
		BMessage* refMessage = new BMessage();
		refMessage->AddRef("ref", &ref);
		ActionManager::AddItem(MSG_PROJECT_MENU_SHOW_IN_TRACKER, sMenu, refMessage);

		refMessage = new BMessage(*refMessage);
		ActionManager::AddItem(MSG_PROJECT_MENU_OPEN_TERMINAL, sMenu, refMessage);
	}

	ActionManager::SetEnabled(B_CUT,   editor->CanCut());
	ActionManager::SetEnabled(B_COPY,  editor->CanCopy());
	ActionManager::SetEnabled(B_PASTE, editor->CanPaste());

	// TODO: duplicated code between here and EditorTabView::ShowTabMenu()
	if (editor->FileRef() != nullptr) {
		ActionManager::GetMessage(MSG_PROJECT_MENU_SHOW_IN_TRACKER, sMenu)->ReplaceRef("ref", editor->FileRef());
		ActionManager::GetMessage(MSG_PROJECT_MENU_OPEN_TERMINAL, sMenu)->ReplaceRef("ref", editor->FileRef());
		ActionManager::SetEnabled(MSG_FIND_IN_BROWSER, (editor->GetProjectFolder() != nullptr));
	} else {
		ActionManager::SetEnabled(MSG_PROJECT_MENU_SHOW_IN_TRACKER, false);
		ActionManager::SetEnabled(MSG_PROJECT_MENU_OPEN_TERMINAL, false);
		ActionManager::SetEnabled(MSG_FIND_IN_BROWSER, false);
	}
	// NOTE: the target should always be editor (for all messages) but we need fist to move them
	// from the window.
	sMenu->SetTargetForItems(editor->Window());

	return sMenu;
}
