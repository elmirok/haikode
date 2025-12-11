/*
 * Copyright 2025 Andrea Anzani <andrea.anzani@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include <Entry.h>
#include <Messenger.h>
#include <String.h>
#include <TextView.h>

#include "EditorId.h"
#include "IEditor.h"

class ProjectFolder;

/**
 * EmptyEditor - Minimal BTextView implementation of IEditor
 *
 * A simple BTextView without any functionality implemented.
 * All IEditor methods are no-ops or return default values.
 * No capabilities are enabled.
 */
class EmptyEditor : public BTextView, public IEditor {
public:
								EmptyEditor();
								~EmptyEditor() override;

	// IEditor interface - View
	BView*						View() override { return this; }

	// IEditor interface - Identity
	editor_id					Id() override { return fId; }
	BString						Name() const override { return fName; }
	void						SetProjectFolder(ProjectFolder*) override {}
	ProjectFolder*				GetProjectFolder() const override { return nullptr; }

	// IEditor interface - Focus
	void						GrabFocus() override {}

	// IEditor interface - Clipboard
	bool						CanCopy() override { return false; }
	void						Copy() override {}
	bool						CanCut() override { return false; }
	void						Cut() override {}
	bool						CanPaste() override { return false; }
	void						Paste() override {}

	// IEditor interface - Undo/Redo
	bool						CanRedo() override { return false; }
	void						Redo() override {}
	bool						CanUndo() override { return false; }
	void						Undo() override {}

	// IEditor interface - File operations
	const BString				FilePath() const override { return ""; }
	entry_ref *const			FileRef() override { return nullptr; }
	status_t					SetFileRef(entry_ref* ref) override { return B_ERROR; }
	node_ref *const				NodeRef() override { return nullptr; }
	status_t					LoadFromFile() override { return B_ERROR; }
	status_t					SaveToFile() override { return B_ERROR; }
	status_t					Reload() override { return B_ERROR; }
	status_t					StartMonitoring() override { return B_ERROR; }
	status_t					StopMonitoring() override { return B_ERROR; }
	std::string					FileType() const override { return ""; }
	void						SetFileType(const std::string& fileType) override {}
	status_t					SetSavedCaretPosition() override { return B_ERROR; }

	// IEditor interface - Configuration
	void						LoadEditorConfig() override {}
	void						ApplySettings() override {}
	void						ApplyEdit(const std::string& info) override {}
	void						TrimTrailingWhitespace() override {}

	// IEditor interface - Navigation
	void						GoToLine(int32 line) override {}
	void						GoToLSPPosition(int32 line, int character) override {}

	// IEditor interface - State queries
	bool						IsFoldingAvailable() const override { return false; }
	bool						IsModified() const override { return false; }
	bool						IsTextSelected() override { return false; }
	bool						IsOverwrite() override { return false; }
	bool						IsReadOnly() override { return false; }
	void						SetReadOnly(bool readOnly = true) override {}
	int32						EndOfLine() override { return 0; }

	// IEditor interface - Problems
	void						SetProblems() override {}

	// IEditor interface - Symbols
	void						SetDocumentSymbols(const BMessage* symbols, IEditor::symbols_status status) override {}
	void						GetDocumentSymbols(BMessage* symbols) const override {}

	// IEditor interface - LSP
	LSPEditorWrapper*			GetLSPEditorWrapper() override { return nullptr; }
	bool						HasLSPServer() const override { return false; }
	bool						HasLSPCapability(const LSPCapability cap) const override { return false; }

	// IEditor interface - Scripting
	const BString				Selection() override { return ""; }
	void						SetSelection(int32 start, int32 end) override {}
	const BString				GetSymbol() override { return ""; }
	void						Insert(BString text, int32 start = -1) override {}
	void						Append(BString text) override {}
	const BString				GetLine(int32 lineNumber) override { return ""; }
	void						InsertLine(BString text, int32 lineNumber) override {}
	int32						CountLines() override { return 0; }
	int32						GetCurrentColumnNumber() override { return 0; }
	int32						GetCurrentLineNumber() override { return 0; }
	int32						GetCurrentPosition() override { return 0; }
	BMessage					GetCaretPositionInfo() override { return BMessage(); }
	BMessage					GetSelectionRange() override { return BMessage(); }
	BMessage					GetVisibleLines() override { return BMessage(); }
	BMessage					GetScrollPosition() override { return BMessage(); }
	void						SetScrollPosition(int32 line) override {}
	BMessage					GetModifiedState() override { return BMessage(); }
	BMessage					GetDocumentInfo() override { return BMessage(); }


private:
	editor_id					fId;
	BString						fName;
};
