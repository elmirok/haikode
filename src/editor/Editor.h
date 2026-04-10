/*
 * Copyright 2025 Andrea Anzani
 * Copyright 2017 A. Mosca
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include <Locker.h>
#include <MessageFilter.h>
#include <Messenger.h>
#include <MessageRunner.h>

#include <set>
#include <string>
#include <utility>

#include "EditorId.h"
#include "IEditor.h"
#include "LSPCapabilities.h"
#include "ScintillaView.h"

class LSPEditorWrapper;
class OverScrollBar;
class ProjectFolder;

namespace editor {
	class StatusView;
}

enum {
	EDITOR_POSITION_CHANGED			= 'Epch',
	EDITOR_UPDATE_SAVEPOINT			= 'EUSP',
	EDITOR_UPDATE_DIAGNOSTICS		= 'diag',
	EDITOR_UPDATE_SYMBOLS			= 'symb',
	EDITOR_MARKER_GOTO				= 'Emgo'
};

enum IndentStyle {
	Tab,
	Space
};

/*
 * Not very smart: NONE,SKIP,DONE are Status
 * while the others are Function placeholders
 *
 */
enum {
	REPLACE_NONE = -1,
	REPLACE_SKIP = 0,
	REPLACE_DONE = 1,
	REPLACE_ONE,
	REPLACE_NEXT,
	REPLACE_PREVIOUS,
	REPLACE_ALL
};

constexpr auto sci_NUMBER_MARGIN = 0;
constexpr auto sci_BOOKMARK_MARGIN = 1;
constexpr auto sci_FOLD_MARGIN = 2;
constexpr auto sci_COMMENT_MARGIN = 3;

constexpr auto sci_BOOKMARK = 0; //Marker

struct EditorConfig {
	enum IndentStyle	IndentStyle;
	int32				IndentSize;
	int32				EndOfLine;
	bool				TrimTrailingWhitespace;
	bool				InsertFinalNewline; // Not implemented
};



class Editor : public BScintillaView, public IEditor {
public:

								Editor(entry_ref* ref, const BMessenger& target);
								~Editor() override;
			BView*				View() override { return this; }
			status_t			PerformEditorAction(BMessage* msg) override;
			editor_id			Id() override { return fId; }
			BString				Name() const override { return fFileName; }
			void				SetProjectFolder(ProjectFolder*) override;
			ProjectFolder*		GetProjectFolder() const override { return fProjectFolder; }
			filter_result		BeforeKeyDown(BMessage*);
			filter_result		BeforeMouseMoved(BMessage* message);
			filter_result		BeforeModifiersChanged(BMessage* message);
			void				GrabFocus() override;

			void				AttachedToWindow() override;

			// Cut, Copy and Paste interface
			bool				CanCopy() override;
			void				Copy() override;
			bool				CanCut() override;
			void				Cut() override;
			bool				CanPaste() override;
			void				Paste() override;

			// Undo and Redo interface
			bool				CanRedo() override;
			void				Redo() override;
			bool				CanUndo() override;
			void				Undo() override;

			// File interface
		const BString			FilePath() const override;
			entry_ref *const	FileRef() override;
			status_t			SetFileRef(entry_ref* ref) override;
			node_ref *const		NodeRef() override;
			status_t			LoadFromFile() override;
			status_t			UnloadFile() override;
			status_t			SaveToFile() override;
			status_t			Reload() override;
			status_t			StartMonitoring() override;
			status_t			StopMonitoring() override;
			std::string			FileType() const override { return fFileType; }
			void				SetFileType(const std::string& fileType) override { fFileType = fileType; }
			status_t			SetSavedCaretPosition() override;

			//
			void				LoadEditorConfig() override;
			void				ApplySettings() override;
			void				ApplyEdit(const std::string& info) override;
			void				TrimTrailingWhitespace() override;

			void				GoToLine(int32 line) override;
			void				GoToLSPPosition(int32 line, int character) override;

			bool				IsFoldingAvailable() const override { return fFoldingAvailable; }
			bool				IsModified() const override { return fModified; }
			bool				IsTextSelected() override;
			bool				IsOverwrite() override;
			bool				IsReadOnly() override;
			void				SetReadOnly(bool readOnly = true) override;
			int32				EndOfLine() override;

			void				SetProblems() override;

			void				SetDocumentSymbols(const BMessage* symbols, IEditor::symbols_status status) override;
			void				GetDocumentSymbols(BMessage* symbols) const override;

			void				SetCommentLineToken(const std::string& commenter) { fCommenter = commenter; }
			void				SetCommentBlockTokens(const std::string& startBlock,
												const std::string& endBlock) ;

			LSPEditorWrapper*	GetLSPEditorWrapper() override { return fLSPEditorWrapper; }
			bool				HasLSPServer() const override;
			bool				HasLSPCapability(const LSPCapability cap) const override;

			// Scripting methods
			const 	BString		Selection() override;
			const 	BString		GetSymbol() override;
			void				Insert(BString text, int32 start = -1) override;
			void				Append(BString text) override;
			const 	BString		GetLine(int32 lineNumber) override;
			void				InsertLine(BString text, int32 lineNumber) override;
			int32				CountLines() override;
			int32				GetCurrentColumnNumber() override;
			int32				GetCurrentLineNumber() override;
			int32				GetCurrentPosition() override;
			BMessage			GetCaretPositionInfo() override;
			BMessage			GetSelectionRange() override;
			void				SetSelectionRange(int32 start, int32 end) override;
			BMessage			GetVisibleLines() override;
			BMessage			GetScrollPosition() override;
			void				SetScrollPosition(int32 line) override;
			BMessage			GetModifiedState() override;
			BMessage			GetDocumentInfo() override;

protected:
			virtual	void 		MessageReceived(BMessage* message) override;
			void				SetTarget(const BMessenger& target);
			void				NotificationReceived(SCNotification* n) override;

private:

			int					ReplaceAndFindNext(const BString& selection,
									const BString& replacement, int flags, bool wrap);
			int					ReplaceAndFindPrevious(const BString& selection,
									const BString& replacement, int flags, bool wrap);
			int32				ReplaceAll(const BString& selection,
									const BString& replacement, int flags);
			int					ReplaceOne(const BString& selection,
									const BString& replacement);
			int					SetSearchFlags(bool matchCase, bool wholeWord,
									bool wordStart,	bool regExp, bool posix,
									const BString& text = BString());
			int32				FindMarkAll(const BString& text, int flags);
			int					FindNext(const BString& search, int flags, bool wrap);
			int					FindPrevious(const BString& search, int flags, bool wrap);
			int					FindInTarget(const BString& search, int flags, int startPosition, int endPosition);
			int32				Find(const BString&  text, int flags, bool backwards, bool onWrap);
			filter_result		OnArrowKey(int8 ch);
			void				SetZoom(int32 zoom);
			void				Completion();
			void				Format();
			void				GoToDefinition();
			void				GoToDeclaration();
			void				GoToImplementation();
			void				FindReferences();
			void				Rename();
			void				SwitchSourceHeader();
			void				UncommentSelection();

			void 				ContextMenu(BPoint point) override;
			void				ToggleFolding();
			void				ShowLineEndings(bool show);
			void				ShowWhiteSpaces(bool show);
			bool				LineEndingsVisible();
			bool				WhiteSpacesVisible();

			void				ScrollCaret();
			void				SelectAll();
	const 	BRect				GetSymbolSurroundingRect();
			void				SendPositionChanges();
			BString const		ModeString();
			void				OverwriteToggle();
			BString const		IsOverwriteString();
			bool				IsSearchSelected(const BString& search, int flags);
			void				CommentSelectedLines();

			void				DuplicateCurrentLine();
			void				DeleteSelectedLines();
			void				EndOfLineConvert(int32 eolMode);
			void				EnsureVisiblePolicy();
			bool				CanClear();
			void				Clear();
			void				BookmarkClearAll(int marker);
			bool				BookmarkGoToNext();
			bool				BookmarkGoToPrevious();
			void				BookmarkToggle(int position);
			BString	const		_EndOfLineString();
			void				UpdateStatusBar();
			void				_ApplyExtensionSettings();
			void 				_LoadResources(BMessage *message);
			void				_MaintainIndentation(char c);
			void				_SetLineIndentation(int line, int indent);
			void				_BraceHighlight();
			bool				_BraceMatch(int pos);
			void				_CommentLine(int32 position);
			void				_EndOfLineAssign(char *buffer, int32 size);
			void				_HighlightBraces();
			void				_RedrawNumberMargin(bool forced = false);
			void				_SetFoldMargin(bool enabled);
			void				_UpdateSavePoint(bool modified);
			void				_NotifyFindStatus(const char* status);

			template<typename T>
			typename T::type	Get() { return T::Get(this); }
			template<typename T>
			void				Set(typename T::type value) { T::Set(this, value); }

			void				EvaluateIdleTime();
			bool				HasValidFileRef() const;
			void				_UpdateOverScrollBarSciMarkers();
			void				_HandleDoubleClik();
			void				_UpdateHighlight();
			void				_ClearHighlight();

private:
			editor_id			fId;
			entry_ref			fFileRef;
			bool				fModified;
			BString				fFileName;
			node_ref			fNodeRef;
			BMessenger			fTarget;

			bool				fBracingAvailable;
			std::string			fFileType;
			bool				fFoldingAvailable;
			std::string			fCommenter;
			int					fLinesLog10;

			int					fCurrentLine;
			int					fCurrentColumn;

			LSPEditorWrapper*	fLSPEditorWrapper;
			ProjectFolder*		fProjectFolder;
			editor::StatusView*	fStatusView;

			BMessage			fDocumentSymbols;
			std::set<std::pair<std::string, int32> > fCollapsedSymbols;

	static	bool				sAutoIndent;

			// editorconfig
			bool				fHasEditorConfig;
			EditorConfig		fEditorConfig;

			BMessageRunner*		fIdleHandler;

			Sci_Position		fLastWordStartPosition = -1;
			Sci_Position		fLastWordEndPosition = -1;
			OverScrollBar*		fOverScrollBar = nullptr;
			BString				fLastHighlight = "";
};
