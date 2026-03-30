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

// --- Phase: Position, Range, Location, TextEdit ---
// using Position = lsp::Position;
// using Range = lsp::Range;
// using Location = lsp::Location;
// using TextEdit = lsp::TextEdit;

// Comparison operators not provided by lsp-framework.
// Uncomment when Position/Range are migrated.
//
// inline bool operator==(const lsp::Position& a, const lsp::Position& b) {
//     return a.line == b.line && a.character == b.character;
// }
// inline bool operator!=(const lsp::Position& a, const lsp::Position& b) {
//     return !(a == b);
// }
// inline bool operator<(const lsp::Position& a, const lsp::Position& b) {
//     return std::tie(a.line, a.character) < std::tie(b.line, b.character);
// }
// inline bool operator<=(const lsp::Position& a, const lsp::Position& b) {
//     return std::tie(a.line, a.character) <= std::tie(b.line, b.character);
// }
//
// inline bool operator==(const lsp::Range& a, const lsp::Range& b) {
//     return a.start == b.start && a.end == b.end;
// }
// inline bool operator!=(const lsp::Range& a, const lsp::Range& b) {
//     return !(a == b);
// }
// inline bool operator<(const lsp::Range& a, const lsp::Range& b) {
//     return std::tie(a.start, a.end) < std::tie(b.start, b.end);
// }

// --- Phase: Completion ---
// using CompletionItem = lsp::CompletionItem;
// using CompletionList = lsp::CompletionList;
// using CompletionItemKind = lsp::CompletionItemKind;
// using InsertTextFormat = lsp::InsertTextFormat;

// --- Phase: Diagnostics ---
// using DiagnosticRelatedInformation = lsp::DiagnosticRelatedInformation;
// Note: lsp::Diagnostic does NOT have 'category' or 'codeActions' (clangd extensions).
// A Genio wrapper struct will be needed — see step 2.9 of the migration plan.

// --- Phase: CodeAction, WorkspaceEdit ---
// using CodeAction = lsp::CodeAction;
// using WorkspaceEdit = lsp::WorkspaceEdit;

// --- Phase: Hover ---
// using MarkupContent = lsp::MarkupContent;
// using Hover = lsp::Hover;

// --- Phase: Signatures ---
// using SignatureHelp = lsp::SignatureHelp;
// using SignatureInformation = lsp::SignatureInformation;
// using ParameterInformation = lsp::ParameterInformation;

// --- Phase: Document Symbols ---
// using DocumentSymbol = lsp::DocumentSymbol;
// using SymbolInformation = lsp::SymbolInformation;
// using SymbolKind = lsp::SymbolKind;
