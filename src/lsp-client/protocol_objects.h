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

struct TextDocumentContentChangeEvent {
    /// The range of the document that changed.
    std::optional<Range> range;

    /// The length of the range that got replaced.
    //xed std::optional<int> rangeLength;
    /// The new text of the range/document.
    std::string text;
};

enum class CompletionItemKind {
    Missing = 0,
    Text = 1,
    Method = 2,
    Function = 3,
    Constructor = 4,
    Field = 5,
    Variable = 6,
    Class = 7,
    Interface = 8,
    Module = 9,
    Property = 10,
    Unit = 11,
    Value = 12,
    Enum = 13,
    Keyword = 14,
    Snippet = 15,
    Color = 16,
    File = 17,
    Reference = 18,
    Folder = 19,
    EnumMember = 20,
    Constant = 21,
    Struct = 22,
    Event = 23,
    Operator = 24,
    TypeParameter = 25,
};

enum class InsertTextFormat {
    Missing = 0,
    /// The primary text to be inserted is treated as a plain string.
            PlainText = 1,
    /// The primary text to be inserted is treated as a snippet.
    ///
    /// A snippet can define tab stops and placeholders with `$1`, `$2`
    /// and `${3:foo}`. `$0` defines the final tab stop, it defaults to the end
    /// of the snippet. Placeholders with equal identifiers are linked, that is
    /// typing in one will update others too.
    ///
    /// See also:
    /// https//github.com/Microsoft/vscode/blob/master/src/vs/editor/contrib/snippet/common/snippet.md
            Snippet = 2,
};
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

// enum class CompletionItemKind
enum class SymbolKind {
    File = 1,
    Module = 2,
    Namespace = 3,
    Package = 4,
    Class = 5,
    Method = 6,
    Property = 7,
    Field = 8,
    Constructor = 9,
    Enum = 10,
    Interface = 11,
    Function = 12,
    Variable = 13,
    Constant = 14,
    String = 15,
    Number = 16,
    Boolean = 17,
    Array = 18,
    Object = 19,
    Key = 20,
    Null = 21,
    EnumMember = 22,
    Struct = 23,
    Event = 24,
    Operator = 25,
    TypeParameter = 26
};

/// Represents programming constructs like variables, classes, interfaces etc.
/// that appear in a document. Document symbols can be hierarchical and they
/// have two ranges: one that encloses its definition and one that points to its
/// most interesting range, e.g. the range of an identifier.
struct DocumentSymbol {
  /// The name of this symbol.
  std::string name;

  /// More detail for this symbol, e.g the signature of a function.
  std::string detail;

  /// The kind of this symbol.
  SymbolKind kind;

  /// Indicates if this symbol is deprecated.
  bool deprecated = false;

  /// The range enclosing this symbol not including leading/trailing whitespace
  /// but everything else like comments. This information is typically used to
  /// determine if the clients cursor is inside the symbol to reveal in the
  /// symbol in the UI.
  Range range;

  /// The range that should be selected and revealed when this symbol is being
  /// picked, e.g the name of a function. Must be contained by the `range`.
  Range selectionRange;

  /// Children of this symbol, e.g. properties of a class.
  std::vector<DocumentSymbol> children;
};

struct SymbolInformation {
    /// The name of this symbol.
    std::string name;
    /// The kind of this symbol.
    SymbolKind kind = SymbolKind::Class;
    /// The location of this symbol.
    Location location;
    /// The name of the symbol containing this symbol.
    std::optional<std::string> containerName;
};


// enum class CompletionItemKind
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
