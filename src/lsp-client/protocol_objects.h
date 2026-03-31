/*
 * Copyright 2023, Andrea Anzani
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef protocol_objects_H
#define protocol_objects_H

#include <map>
#include <optional>
#include <string>
#include <vector>
#include <tuple>

#include <json.hpp>

#include "uri.h"
#include "LSPCompat.h"

// TextDocumentContentChangeEvent — migrated to lsp-framework types.
// (lsp::TextDocumentContentChangeEvent = OneOf<..._Range_Text, ..._Text>)

// CompletionItemKind and InsertTextFormat — migrated to lsp-framework types.
// See LSPCompat.h for the type aliases.

// CompletionItem and CompletionList — migrated to lsp-framework types.
// See LSPCompat.h for the type aliases.

// SignatureHelp, SignatureInformation, ParameterInformation — migrated to lsp-framework types.
// See LSPCompat.h for the type aliases.

// Diagnostic and DiagnosticRelatedInformation — migrated to lsp-framework types.
// See LSPCompat.h for the type aliases.
// Clangd extension fields (category, codeActions) live in LSPDiagnostic (LSPEditorWrapper.h).

// CodeAction, WorkspaceEdit — migrated to lsp-framework types.
// See LSPCompat.h for the type aliases.
// TweakArgs, ExecuteCommandParams, LspCommand were clangd-specific structs
// only used inside CodeAction serialization — removed with the migration.

// SymbolKind — migrated to lsp::SymbolKind.
// See LSPCompat.h for the type alias (using SymbolKind = lsp::SymbolKind).

// DocumentSymbol and SymbolInformation — migrated to lsp-framework types.
// See LSPCompat.h for the type aliases.

enum class MessageType {
	Error = 1,
	Warning = 2,
	Info = 3,
	Log = 4,
	Debug = 5 /* since 3.18.0 (proposed)*/
};

struct LogMessageParams {

	MessageType type;

	/**
	 * The actual message
	 */
	std::string message;
};

#endif // protocol_objects_H
