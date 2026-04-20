/*
 * Copyright 2025 Andrea Anzani <andrea.anzani@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include <Bitmap.h>
#include <Entry.h>
#include <Messenger.h>
#include <String.h>
#include <View.h>

#include "EditorId.h"
#include "IEditor.h"

/**
 * ImageEditor - Image viewer implementation of IEditor
 *
 * Displays image files in common formats (PNG, JPEG, GIF, BMP, etc.)
 * Most IEditor methods are no-ops or stubs since image editing is not supported.
 */
class ImageEditor : public BView, public IEditor {
public:
								ImageEditor(entry_ref* ref, const BMessenger& target);
								~ImageEditor() override;

	 status_t					PerformEditorAction(BMessage* msg) override { return B_ERROR; }

	// IEditor interface - View
	BView*						View() override { return this; }

	// IEditor interface - Identity
	editor_id					Id() override { return fId; }
	BString						Name() const override { return fFileName; }
	void						SetProjectFolder(ProjectFolder*) override;
	ProjectFolder*				GetProjectFolder() const override { return fProjectFolder; }

	// IEditor interface - Focus
	void						GrabFocus() override;


	//The editor is now the selected editor.
	virtual void				Selected() override {};

	// IEditor interface - Clipboard (not supported for images)
	bool						CanCopy() override { return false; }
	void						Copy() override {}
	bool						CanCut() override { return false; }
	void						Cut() override {}
	bool						CanPaste() override { return false; }
	void						Paste() override {}

	// IEditor interface - Undo/Redo (not supported)
	bool						CanRedo() override { return false; }
	void						Redo() override {}
	bool						CanUndo() override { return false; }
	void						Undo() override {}

	// IEditor interface - File operations
	const BString				FilePath() const override;
	entry_ref *const			FileRef() override { return &fFileRef; }
	status_t					SetFileRef(entry_ref* ref) override;
	node_ref *const				NodeRef() override { return &fNodeRef; }
	status_t					LoadFromFile() override;
	status_t					UnloadFile() override;
	status_t					SaveToFile() override { return B_NOT_ALLOWED; }
	status_t					Reload() override;
	status_t					StartMonitoring() override;
	status_t					StopMonitoring() override;
	std::string					FileType() const override { return fFileType; }
	void						SetFileType(const std::string& fileType) override { fFileType = fileType; }
	status_t					SetSavedCaretPosition() override { return B_OK; }

	// IEditor interface - Configuration (no-ops)
	void						LoadEditorConfig() override {}
	void						ApplySettings() override {}
	void						ApplyEdit(const std::string& info) override {}
	void						TrimTrailingWhitespace() override {}

	// IEditor interface - Navigation (no-ops)
	void						GoToLine(int32 line) override {}
	void						GoToLSPPosition(int32 line, int character) override {}

	// IEditor interface - State queries
	bool						IsFoldingAvailable() const override { return false; }
	bool						IsModified() const override { return false; }
	bool						IsTextSelected() override { return false; }
	bool						IsOverwrite() override { return false; }
	bool						IsReadOnly() override { return true; }
	void						SetReadOnly(bool readOnly = true) override {}
	int32						EndOfLine() override { return 0; }

	// IEditor interface - Problems (not applicable)
	void						SetProblems() override {}

	// IEditor interface - Symbols (not applicable)
	void						SetDocumentSymbols(const BMessage* symbols, IEditor::symbols_status status) override {}
	void						GetDocumentSymbols(BMessage* symbols) const override {}


	// IEditor interface - LSP (not supported)
	LSPEditorWrapper*			GetLSPEditorWrapper() override { return nullptr; }
	bool						HasLSPServer() const override { return false; }
	bool						HasLSPCapability(const LSPCapability cap) const override { return false; }

	// IEditor interface - Scripting (minimal support)
	const BString				Selection() override { return BString(""); }
	const BString				GetSymbol() override { return BString(""); }
	void						Insert(BString text, int32 start = -1) override {}
	void						Append(BString text) override {}
	const BString				GetLine(int32 lineNumber) override { return BString(""); }
	void						InsertLine(BString text, int32 lineNumber) override {}
	int32						CountLines() override { return 0; }
	int32						GetCurrentColumnNumber() override { return 0; }
	int32						GetCurrentLineNumber() override { return 0; }
	int32						GetCurrentPosition() override { return 0; }
	BMessage					GetCaretPositionInfo() override { return BMessage(); }
	BMessage					GetSelectionRange() override { return BMessage(); }
	void						SetSelectionRange(int32 start, int32 end) override {}
	BMessage					GetVisibleLines() override { return BMessage(); }
	BMessage					GetScrollPosition() override { return BMessage(); }
	void						SetScrollPosition(int32 line) override {}
	BMessage					GetModifiedState() override;
	BMessage					GetDocumentInfo() override;


protected:
	// BView overrides
	void						Draw(BRect updateRect) override;
	void						FrameResized(float width, float height) override;
	void						MessageReceived(BMessage* message) override;
	void						MouseDown(BPoint where) override;
	void						MouseUp(BPoint where) override;
	void						MouseMoved(BPoint where, uint32 code, const BMessage* dragMessage) override;
	void						AttachedToWindow() override;

private:
	void						_UpdateBitmapPosition();
	void						_ZoomIn(BPoint center);
	void						_ZoomOut(BPoint center);
	void						_ZoomToFit();
	void						_ZoomToActual();
	status_t					_LoadImage();

	editor_id					fId;
	entry_ref					fFileRef;
	node_ref					fNodeRef;
	BString						fFileName;
	std::string					fFileType;
	BMessenger					fTarget;
	ProjectFolder*				fProjectFolder;

	BBitmap*					fBitmap;
	BRect						fBitmapRect;
	float						fScale;
	BPoint						fOffset;
	bool						fDragging;
	BPoint						fDragStart;
	BPoint						fOffsetStart;
};
