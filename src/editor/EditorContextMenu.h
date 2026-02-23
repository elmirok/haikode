/*
 * Copyright 2023, Andrea Anzani 
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include <Point.h>

class BMenuItem;
class BPopUpMenu;
class Editor;
struct LSPDiagnostic;
class LSPEditorWrapper;

class EditorContextMenu {
public:
	static void Show(Editor*, BPoint point);

private:
		   EditorContextMenu();

	static BPopUpMenu*	sMenu;
	static BPopUpMenu*	sFixMenu;

	static void _PopulateFixMenu(BPopUpMenu* fixMenu, LSPEditorWrapper* lsp, 
								Editor* editor, LSPDiagnostic& dia, int32 index);
	static BPopUpMenu* _GetCodeActionsMenu(Editor* editor, BPoint screenPoint,
								LSPEditorWrapper*& outLsp);
	static BPopUpMenu* _GetStandardMenu(Editor* editor, BPoint screenPoint);
};
