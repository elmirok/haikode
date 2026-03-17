/*
 * Copyright 2025 Andrea Anzani <andrea.anzani@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include <String.h>
#include <Message.h>

#include <string>

#include "EditorId.h"
#include "LSPCapabilities.h"

class LSPEditorWrapper;
class ProjectFolder;
struct entry_ref;
struct node_ref;
class BView;

/**
 * IEditor - Interface for text editor functionality
 *
 * This interface defines the contract for editor implementations,
 * providing text editing, file operations, LSP integration, and scripting capabilities.
 */
class IEditor {
public:

	enum symbols_status {
		STATUS_UNKNOWN			= 0, // "<empty string>"
		STATUS_NO_CAPABILITY	= 1, // "No outline available"
		STATUS_REQUESTED		= 2, // "Creating outline"
		STATUS_HAS_SYMBOLS		= 3, // <list of symbols (if any)>
	};


	virtual						~IEditor() {}

	virtual BView*				View() = 0;

	virtual status_t			PerformEditorAction(BMessage* msg) = 0;

	// Identity and properties
	virtual editor_id			Id() = 0;
	virtual BString				Name() const = 0;
	virtual void				SetProjectFolder(ProjectFolder*) = 0;
	virtual ProjectFolder*		GetProjectFolder() const = 0;

	// Focus management
	virtual void				GrabFocus() = 0;

	// Clipboard operations
	virtual bool				CanCopy() = 0;
	virtual void				Copy() = 0;
	virtual bool				CanCut() = 0;
	virtual void				Cut() = 0;
	virtual bool				CanPaste() = 0;
	virtual void				Paste() = 0;

	// Undo/Redo operations
	virtual bool				CanRedo() = 0;
	virtual void				Redo() = 0;
	virtual bool				CanUndo() = 0;
	virtual void				Undo() = 0;

	// File operations
	virtual const BString		FilePath() const = 0;
	virtual entry_ref *const	FileRef() = 0;
	virtual status_t			SetFileRef(entry_ref* ref) = 0;
	virtual node_ref *const		NodeRef() = 0;
	virtual status_t			LoadFromFile() = 0;
	virtual	status_t			UnloadFile() = 0; //dual of LoadFromFile
	virtual status_t			SaveToFile() = 0;
	virtual status_t			Reload() = 0;
	virtual status_t			StartMonitoring() = 0;
	virtual status_t			StopMonitoring() = 0;
	virtual std::string			FileType() const = 0;
	virtual void				SetFileType(const std::string& fileType) = 0;
	virtual status_t			SetSavedCaretPosition() = 0;

	// Configuration and settings
	virtual void				LoadEditorConfig() = 0;
	virtual void				ApplySettings() = 0;
	virtual void				ApplyEdit(const std::string& info) = 0;
	virtual void				TrimTrailingWhitespace() = 0;

	// Navigation
	virtual void				GoToLine(int32 line) = 0;
	virtual void				GoToLSPPosition(int32 line, int character) = 0;

	// Editor state queries
	virtual bool				IsFoldingAvailable() const = 0;
	virtual bool				IsModified() const = 0;
	virtual bool				IsTextSelected() = 0;
	virtual bool				IsOverwrite() = 0;
	virtual bool				IsReadOnly() = 0;
	virtual void				SetReadOnly(bool readOnly = true) = 0;
	virtual int32				EndOfLine() = 0;

	// Problems and diagnostics
	virtual void				SetProblems() = 0;

	// Symbol management
	virtual void				SetDocumentSymbols(const BMessage* symbols, IEditor::symbols_status status) = 0;
	virtual void				GetDocumentSymbols(BMessage* symbols) const = 0;

	// LSP integration
	virtual LSPEditorWrapper*	GetLSPEditorWrapper() = 0;
	virtual bool				HasLSPServer() const = 0;
	virtual bool				HasLSPCapability(const LSPCapability cap) const = 0;

	// Scripting interface
	virtual const BString		Selection() = 0;
	virtual const BString		GetSymbol() = 0;
	virtual void				Insert(BString text, int32 start = -1) = 0;
	virtual void				Append(BString text) = 0;
	virtual const BString		GetLine(int32 lineNumber) = 0;
	virtual void				InsertLine(BString text, int32 lineNumber) = 0;
	virtual int32				CountLines() = 0;
	virtual int32				GetCurrentColumnNumber() = 0;
	virtual int32				GetCurrentLineNumber() = 0;
	virtual int32				GetCurrentPosition() = 0;
	virtual BMessage			GetCaretPositionInfo() = 0;
	virtual BMessage			GetSelectionRange() = 0;
	virtual void				SetSelectionRange(int32 start, int32 end) = 0;
	virtual BMessage			GetVisibleLines() = 0;
	virtual BMessage			GetScrollPosition() = 0;
	virtual void				SetScrollPosition(int32 line) = 0;
	virtual BMessage			GetModifiedState() = 0;
	virtual BMessage			GetDocumentInfo() = 0;
};
