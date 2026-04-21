/*
 * Copyright 2023-2026, Andrea Anzani
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include <Autolock.h>
#include <ToolTip.h>

#include <vector>
#include <lsp/types.h>

#include "CallTipContext.h"
#include "LSPCapabilities.h"
#include "LSPProjectWrapper.h"
#include "LSPTextDocument.h"
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
	lsp::Diagnostic diagnostic;
	std::string fixTitle;

	// Clangd extension fields (not part of the LSP spec, not in lsp::Diagnostic).
	std::optional<std::string> category;
	std::optional<lsp::json::Value> codeActions; // JSON array of code actions
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

				LSPEditorWrapper(const BPath& filenamePath, Editor* editor);
		virtual	~LSPEditorWrapper() {};
		void	ApplySettings();
		void	SetLSPServer(LSPProjectWrapper* cW);
		void	RegisterDocument();
		void	UnsetLSPServer();
		bool	HasLSPServer() const;
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
		void	Rename(const lsp::Position& position);
		void	Rename();
		void	StartHover(Sci_Position sci_position);
		void	EndHover();
		void	GetDiagnostics(std::vector<LSPDiagnostic>& diagnostics) const { diagnostics = fLastDiagnostics; }
		void	FindReferences();

		void	IndicatorClick(Sci_Position sci_position);
private:
		void	RequestDocumentSymbols();
public:
		void	CharAdded(const char ch /*utf-8?*/);

		void	NextCallTip();
		void	PrevCallTip();
		void	HideCallTip();

		void	MouseMoved(BMessage*);

public:

		void	OnFormat(ArrayTextEdit&& edits);
		void	OnRename(lsp::WorkspaceEdit&& edit); //use the result?
		void	OnHover(lsp::TextDocument_HoverResult&& result);
		void	OnGoTo(lsp::TextDocument_DefinitionResult&& result);
		void	OnFindReferences(lsp::TextDocument_ReferencesResult&& result, std::string symbolName);
		void	OnSignatureHelp(lsp::SignatureHelp&& signatureHelp); //use the result?
		void	OnCompletion(lsp::TextDocument_CompletionResult&& result);
		void	OnDiagnostics(lsp::json::Value&& params);
		void	OnDocumentLink(lsp::TextDocument_DocumentLinkResult&& links);
		void	OnDocumentSymbol(lsp::TextDocument_DocumentSymbolResult&& result);
		void	OnCodeActions(lsp::json::Value&& codeActionsJson);

		//	Still generic methods using json buffers..
		void	OnFileStatus(lsp::json::Value& params);
		void	OnInitialize(lsp::json::Value& params);

		//move to LSPProject?
		bool	LocationFromDefinition(lsp::TextDocument_DefinitionResult&& result, lsp::Location& retLoc);
		//move to LSPProject?
		std::string		ExtractSymbolFromFile(const lsp::Location& loc); //move?

public:

		int32	DiagnosticFromPosition(Sci_Position sci_position, LSPDiagnostic& dia);
		int32	DiagnosticFromRange(const lsp::Range& range, LSPDiagnostic& dia) const;

private:
		bool	IsInitialized() const;


		void	_ShowToolTip(const char* text);
		void	_RemoveAllDiagnostics();
		void	_RemoveAllDocumentLinks();
		void	_DoRecursiveDocumentSymbol(lsp::Array<lsp::DocumentSymbol>& vect, BMessage& msg);
		void	_DoLinearSymbolInformation(lsp::Array<lsp::SymbolInformation>& vect, BMessage& msg);

		//utils
		void			FromSciPositionToLSPPosition(const Sci_Position &pos, lsp::Position *lsp_position);
		Sci_Position 	FromLSPPositionToSciPosition(const lsp::Position* lsp_position);
		void 			GetCurrentLSPPosition(lsp::Position *lsp_position);
		void 			FromSciPositionToRange(Sci_Position s_start, Sci_Position s_end, lsp::Range *range);
		Sci_Position 	ApplyTextEdit(lsp::json::Value& textEditJson);
		Sci_Position 	ApplyTextEdit(lsp::TextEdit &textEdit);
		void			OpenFileURI(const lsp::FileUri& uri, int32 line = -1, int32 character = -1,
									ArrayTextEdit&& edits = {});

		std::string 	GetCurrentLine();


		Editor*				fEditor;
		lsp::CompletionList	fCurrentCompletion;
		Sci_Position		fCompletionPosition;
		BTextToolTip* 		fToolTip;
		LSPProjectWrapper*	fLSPProjectWrapper;
		BString				fFileStatus;
		CallTipContext		fCallTip;
		bool				fInitialized;
		Sci_Position		fLastWordStartPosition;
		Sci_Position		fLastWordEndPosition;

		std::vector<LSPDiagnostic>	fLastDiagnostics;
		std::vector<InfoRange>		fLastDocumentLinks;
		std::vector<lsp::TextDocumentContentChangeEvent> fChanges;
};
