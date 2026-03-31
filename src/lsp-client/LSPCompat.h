/*
 * Copyright 2026, Andrea Anzani
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

/*
 * Compatibility layer for migrating from Genio's hand-rolled LSP protocol types
 * to leon-bckl/lsp-framework auto-generated types.
 *
 * Each alias below is activated by removing the corresponding definition from
 * protocol_objects.h / protocol.h and uncommenting here.
 *
 * This file is temporary scaffolding — it will be removed once all types
 * have been migrated and the old headers are deleted.
 */

#include <lsp/types.h>

// --- Phase: Position, Range ---
using Position = lsp::Position;
using Range = lsp::Range;

// Comparison operators not provided by lsp-framework.
// Must live in namespace lsp so ADL finds them (required by std::tuple comparison in C++20).
namespace lsp {

inline bool operator==(const Position& a, const Position& b) {
    return a.line == b.line && a.character == b.character;
}
inline bool operator!=(const Position& a, const Position& b) {
    return !(a == b);
}
inline bool operator<(const Position& a, const Position& b) {
    return std::tie(a.line, a.character) < std::tie(b.line, b.character);
}
inline bool operator<=(const Position& a, const Position& b) {
    return std::tie(a.line, a.character) <= std::tie(b.line, b.character);
}

inline bool operator==(const Range& a, const Range& b) {
    return a.start == b.start && a.end == b.end;
}
inline bool operator!=(const Range& a, const Range& b) {
    return !(a == b);
}
inline bool operator<(const Range& a, const Range& b) {
    return std::tie(a.start, a.end) < std::tie(b.start, b.end);
}

} // namespace lsp

// --- Phase: Location, TextEdit ---
using Location = lsp::Location;
using TextEdit = lsp::TextEdit;

// --- Phase: Completion ---
using CompletionItem = lsp::CompletionItem;
using CompletionList = lsp::CompletionList;
// CompletionItemKind and InsertTextFormat aliases NOT activated yet —
// ClientCapabilities still uses the Genio enums (migrated in step 2.13).
// using CompletionItemKind = lsp::CompletionItemKind;
// using InsertTextFormat = lsp::InsertTextFormat;

// --- Phase: Diagnostics ---
using DiagnosticRelatedInformation = lsp::DiagnosticRelatedInformation;
using Diagnostic = lsp::Diagnostic;
// Note: lsp::Diagnostic does NOT have 'category' or 'codeActions' (clangd extensions).
// Those fields live in LSPDiagnostic (LSPEditorWrapper.h).

// --- Phase: CodeAction, WorkspaceEdit ---
using CodeAction = lsp::CodeAction;
using WorkspaceEdit = lsp::WorkspaceEdit;
using Command = lsp::Command;

// --- Phase: Hover ---
using MarkupContent = lsp::MarkupContent;
using Hover = lsp::Hover;

// --- Phase: Signatures ---
// using SignatureHelp = lsp::SignatureHelp;
// using SignatureInformation = lsp::SignatureInformation;
// using ParameterInformation = lsp::ParameterInformation;

// --- Phase: Document Symbols ---
// using DocumentSymbol = lsp::DocumentSymbol;
// using SymbolInformation = lsp::SymbolInformation;
// using SymbolKind = lsp::SymbolKind;
