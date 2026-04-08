/*
 * Copyright 2023, Andrea Anzani
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef LSPEditorWrapper_H
#define LSPEditorWrapper_H

#include <Autolock.h>
#include <ToolTip.h>

#include <vector>

#include "CallTipContext.h"
#include "LSPCapabilities.h"
#include "LSPProjectWrapper.h"
#include "LSPTextDocument.h"
#include "LSPCompat.h"
#include "Sci_Position.h"

enum IndicatorIndex {
	IND_DIAG = INDICATOR_CONTAINER + 1, //Style for Problems
	IND_LINK = INDICATOR_CONTAINER + 2, //Style for Links
	IND_OVER = INDICATOR_CONTAINER + 3, //Style for mouse hover
	IND_HIGHLIGHT = INDICATOR_CONTAINER + 4, //Style for word highlight
};

//#define DOCUMENT_LINK

struct InfoRange {
	Sci_Position	from;
	Sci_Position	to;
	std::string		info;
};

struct LSPDiagnostic {
	InfoRange range;
	Diagnostic diagnostic;
	std::string fixTitle;

	// Clangd extension fields (not part of the LSP spec, not in lsp::Diagnostic).
	std::optional<std::string> category;
	std::optional<std::vector<CodeAction>> codeActions;
};

class Editor;
class LSPProjectWrapper;

using ArrayTextEdit = lsp::Array<lsp::TextEdit>;

class LSPEditorWrapper : public LSPTextDocument {
public:
	enum GoToType {
		GOTO_DEFINITION,
		GOTO_DECLARATION,
		GOTO_IMPLEMENTATION
	};

				LSPEditorWrapper(BPath filenamePath, Editor* fEditor);
		virtual	~LSPEditorWrapper() {};
		void	ApplySettings();
		void	SetLSPServer(LSPProjectWrapper* cW);
		void	UnsetLSPServer();
		bool	HasLSPServer();
		bool	HasLSPServerCapability(const LSPCapability cap);
		void	ApplyFix(BMessage* info);
		void 	ApplyEdit(std::string info);
		void	GoTo(LSPEditorWrapper::GoToType type);

private:
		void	didOpen();
public:
		void	didClose();
		void	didChange(const char* text, long len, Sci_Position start_pos, Sci_Position poslength);
		void	flushChanges();
		void	didSave();

		void	StartCompletion();
		void	SelectedCompletion(const char* text);
		void	Format();
		void	Rename(std::string newName);
		void	StartHover(Sci_Position sci_position);
		void	EndHover();
		void	GetDiagnostics(std::vector<LSPDiagnostic>& diagnostics) { diagnostics = fLastDiagnostics; }
		void	RequestCodeActions(Diagnostic& diagnostic);
		void	CodeActionResolve(lsp::CodeAction &params);

		void	IndicatorClick(Sci_Position position);
private:
		void	RequestDocumentSymbols();
public:
		void	CharAdded(const char ch /*utf-8?*/);

		void	NextCallTip();
		void	PrevCallTip();
		void	HideCallTip();

		void	MouseMoved(BMessage*);

public:

	int32 DiagnosticFromPosition(Sci_Position p, LSPDiagnostic& dia);
	int32 DiagnosticFromRange(Range& range, LSPDiagnostic& dia);

	Editor*				fEditor;
	CompletionList		fCurrentCompletion;
	Sci_Position		fCompletionPosition;
	BTextToolTip* 		fToolTip;
	LSPProjectWrapper*	fLSPProjectWrapper;
	BString				fFileStatus;
	CallTipContext		fCallTip;
	bool				fInitialized;
	Sci_Position		fLastWordStartPosition;
	Sci_Position		fLastWordEndPosition;

private:
	bool	IsInitialized();
	std::vector<LSPDiagnostic>	fLastDiagnostics;
	std::vector<InfoRange>		fLastDocumentLinks;

	void				_ShowToolTip(const char* text);
	void				_RemoveAllDiagnostics();
	void				_RemoveAllDocumentLinks();

public:
			//FIXME: rename
	void	_DoFormat(ArrayTextEdit&& edits);
	void	_DoRename(lsp::WorkspaceEdit&& edit); //use the result?
	void	_DoHover(lsp::TextDocument_HoverResult&& result);
	void	_DoGoTo(lsp::TextDocument_DefinitionResult&& result);
	void	_DoSignatureHelp(lsp::SignatureHelp&& signatureHelp); //use the result?
	void	_DoCompletion(lsp::TextDocument_CompletionResult&& result);
	void	_DoDiagnostics(lsp::PublishDiagnosticsParams&& params);
	void	_DoDocumentLink(lsp::TextDocument_DocumentLinkResult&& links);
	void	_DoDocumentSymbol(lsp::TextDocument_DocumentSymbolResult&& result);
	void	_DoCodeActions(lsp::TextDocument_CodeActionResult&& codeAction);
	void	_DoCodeActionResolve(CodeAction&& params);

	//Still generic methods
	void	_DoFileStatus(value& params);
	void	_DoInitialize(value& params);

private:

	void	_DoRecursiveDocumentSymbol(lsp::Array<DocumentSymbol>& v, BMessage& msg);
	void	_DoLinearSymbolInformation(lsp::Array<SymbolInformation>& v, BMessage& msg);

	//utils
	void 			FromSciPositionToLSPPosition(const Sci_Position &pos, Position *lsp_position);
	Sci_Position 	FromLSPPositionToSciPosition(const Position* lsp_position);
	void 			GetCurrentLSPPosition(Position *lsp_position);
	void 			FromSciPositionToRange(Sci_Position s_start, Sci_Position s_end, Range *range);
	Sci_Position 	ApplyTextEdit(value &textEdit);
	Sci_Position 	ApplyTextEdit(TextEdit &textEdit);
	void			OpenFileURI(std::string uri, int32 line = -1, int32 character = -1,
								ArrayTextEdit&& edits = {});
	std::string 	GetCurrentLine();
	bool			IsStatusValid();
	std::vector<lsp::TextDocumentContentChangeEvent> fChanges;
};

#endif // LSPEditorWrapper_H
