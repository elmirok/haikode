/*
 * Copyright 2026, Andrea Anzani
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include <lsp/json/json.h>
#include <lsp/types.h>

// Type aliases formerly in MessageHandler.h, used throughout the LSP client.
using value = lsp::json::Value;
using RequestID = std::string;

// Comparison operators not provided by lsp-framework.
namespace lsp {
	inline bool operator==(const lsp::Position& a, const lsp::Position& b) {
		return a.line == b.line && a.character == b.character;
	}
	inline bool operator==(const lsp::Range& a, const lsp::Range& b) {
		return a.start == b.start && a.end == b.end;
	}
	inline bool operator!=(const lsp::Range& a, const lsp::Range& b) {
		return !(a == b);
	}
} // namespace lsp

// to simplify the lsp-framework migration.

using Position = lsp::Position;
using Range = lsp::Range;
using Location = lsp::Location;
using TextEdit = lsp::TextEdit;
using CompletionItem = lsp::CompletionItem;
using CompletionList = lsp::CompletionList;
using DiagnosticRelatedInformation = lsp::DiagnosticRelatedInformation;
using Diagnostic = lsp::Diagnostic;
using CodeAction = lsp::CodeAction;
using WorkspaceEdit = lsp::WorkspaceEdit;
using Command = lsp::Command;
using MarkupContent = lsp::MarkupContent;
using Hover = lsp::Hover;
using SignatureHelp = lsp::SignatureHelp;
using SignatureInformation = lsp::SignatureInformation;
using ParameterInformation = lsp::ParameterInformation;
using DocumentSymbol = lsp::DocumentSymbol;
using SymbolInformation = lsp::SymbolInformation;
using SymbolKind = lsp::SymbolKind;
