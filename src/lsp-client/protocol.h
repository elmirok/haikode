//
// Created by Alex on 2020/1/28. (https://github.com/microsoft/language-server-protocol)
// Additional changes by Andrea Anzani
//
// Part of the LLVM Project, under the Apache License v2.0 with LLVM Exceptions.
// See https://llvm.org/LICENSE.txt for license information.
// SPDX-License-Identifier: Apache-2.0 WITH LLVM-exception
// Most of the code comes from clangd(Protocol.h)

#ifndef LSP_PROTOCOL_H
#define LSP_PROTOCOL_H

#include <string>
#include <vector>
#include <tuple>
#include <map>
#include <memory>
#include <optional>

#include <json.hpp>

#include "LSPProjectWrapper.h"
#include "uri.h"

#define MAP_JSON(...) {j = {__VA_ARGS__};}
#define MAP_KEY(KEY) {#KEY, value.KEY}
#define MAP_TO(KEY, TO) {KEY, value.TO}
#define MAP_KV(K, ...) {K, {__VA_ARGS__}}
#define FROM_KEY(KEY) if (j.contains(#KEY)) j.at(#KEY).get_to(value.KEY);
#define JSON_SERIALIZE(Type, TO, FROM) \
    namespace nlohmann { \
        template <> struct adl_serializer<Type> { \
            static void to_json(json& j, const Type& value) TO \
            static void from_json(const json& j, Type& value) FROM \
        }; \
    }

namespace nlohmann {
    template <typename T>
    struct adl_serializer<std::optional<T>> {
        static void to_json(json& j, const std::optional<T>& opt) {
            if (opt.has_value()) {
                j = opt.value();
            } else {
                j = nullptr;
            }
        }
        static void from_json(const json& j, std::optional<T>& opt) {
            if (j.is_null()) {
                opt = std::nullopt;
            } else {
                opt = j.get<T>();
            }
        }
    };

    template <>
    struct adl_serializer<lsp::DocumentUri> {
        static void to_json(json& j, const lsp::DocumentUri& uri) {
            j = uri.toString();
        }
        static void from_json(const json& j, lsp::DocumentUri& uri) {
            uri = lsp::Uri::parse(j.get<std::string>());
        }
    };
}

using TextType = string_ref;
enum class ErrorCode {
    // Defined by JSON RPC.
    ParseError = -32700,
    InvalidRequest = -32600,
    MethodNotFound = -32601,
    InvalidParams = -32602,
    InternalError = -32603,
    ServerNotInitialized = -32002,
    UnknownErrorCode = -32001,
    // Defined by the protocol.
    RequestCancelled = -32800,
};
class LSPError {
public:
    std::string Message;
    ErrorCode Code;
    static char ID;
    LSPError(std::string Message, ErrorCode Code)
            : Message(std::move(Message)), Code(Code) {}
};

struct TextDocumentIdentifier {
    /// The text document's URI.
    DocumentUri uri;
};
JSON_SERIALIZE(TextDocumentIdentifier, MAP_JSON(MAP_KEY(uri)), {});

struct VersionedTextDocumentIdentifier : public TextDocumentIdentifier {
    int version = 0;
};
JSON_SERIALIZE(VersionedTextDocumentIdentifier, MAP_JSON(MAP_KEY(uri), MAP_KEY(version)), {});

#include "protocol_objects.h"
// struct Position (now lsp::Position with uint fields)
JSON_SERIALIZE(Position,
    MAP_JSON(MAP_KEY(line), MAP_KEY(character)),
    {FROM_KEY(line);FROM_KEY(character)});
// struct Range (now lsp::Range)
JSON_SERIALIZE(Range,
    MAP_JSON(MAP_KEY(start), MAP_KEY(end)),
    {FROM_KEY(start);FROM_KEY(end)});

//struct Location
JSON_SERIALIZE(Location, MAP_JSON(MAP_KEY(uri), MAP_KEY(range)), {FROM_KEY(uri);FROM_KEY(range)});
// struct TextEdit
JSON_SERIALIZE(TextEdit, MAP_JSON(MAP_KEY(range), MAP_KEY(newText)), {FROM_KEY(range);FROM_KEY(newText);});

struct TextDocumentItem {
    /// The text document's URI.
    DocumentUri uri;

    /// The text document's language identifier.
    string_ref languageId;

    /// The version number of this document (it will strictly increase after each
    int version = 0;

    /// The content of the opened text document.
    string_ref text;
};
JSON_SERIALIZE(TextDocumentItem, MAP_JSON(
                MAP_KEY(uri), MAP_KEY(languageId), MAP_KEY(version), MAP_KEY(text)), {});

enum class TraceLevel {
    Off = 0,
    Messages = 1,
    Verbose = 2,
};
enum class TextDocumentSyncKind {
    /// Documents should not be synced at all.
    None = 0,
    /// Documents are synced by always sending the full content of the document.
    Full = 1,
    /// Documents are synced by sending the full content on open.  After that
    /// only incremental updates to the document are send.
    Incremental = 2,
};

// OffsetEncoding, MarkupKind, ResourceOperationKind, FailureHandlingKind —
// only used by the old ClientCapabilities struct (now deleted).
// Position encoding is now lsp::PositionEncodingKindEnum, markup kind is
// lsp::MarkupKindEnum (both in lsp-framework types.h).


namespace CodeActionKind {

    /**
     * Empty kind.
     */
    const std::string Empty("");

    /**
     * Base kind for quickfix actions: 'quickfix'.
     */
    const std::string QuickFix("quickfix");

    /**
     * Base kind for refactoring actions: 'refactor'.
     */
    const std::string Refactor("refactor");

    /**
     * Base kind for refactoring extraction actions: 'refactor.extract'.
     *
     * Example extract actions:
     *
     * - Extract method
     * - Extract function
     * - Extract variable
     * - Extract interface from class
     * - ...
     */
    const std::string RefactorExtract("refactor.extract");

    /**
     * Base kind for refactoring inline actions: 'refactor.inline'.
     *
     * Example inline actions:
     *
     * - Inline function
     * - Inline variable
     * - Inline constant
     * - ...
     */
    const std::string RefactorInline("refactor.inline");

    /**
     * Base kind for refactoring rewrite actions: 'refactor.rewrite'.
     *
     * Example rewrite actions:
     *
     * - Convert JavaScript function to class
     * - Add or remove parameter
     * - Encapsulate field
     * - Make method static
     * - Move method to base class
     * - ...
     */
    const std::string RefactorRewrite("refactor.rewrite");

    /**
     * Base kind for source actions: `source`.
     *
     * Source code actions apply to the entire file.
     */
    const std::string Source("source");

    /**
     * Base kind for an organize imports source action:
     * `source.organizeImports`.
     */
    const std::string SourceOrganizeImports("source.organizeImports");

    /**
     * Base kind for a 'fix all' source action: `source.fixAll`.
     *
     * 'Fix all' actions automatically fix errors that have a clear fix that
     * do not require user input. They should not suppress errors or perform
     * unsafe fixes such as generating new types or classes.
     *
     * @since 3.17.0
     */
    const std::string SourceFixAll("source.fixAll");
}

// ClientCapabilities — migrated to lsp::ClientCapabilities.
// Built programmatically in LSPProjectWrapper.cpp (BuildClientCapabilities()).
// Clangd extension fields are patched into the serialized JSON in Initialize().

struct ServerCapabilities {
    json capabilities;
    /**
     * Defines how text documents are synced. Is either a detailed structure defining each notification or
     * for backwards compatibility the TextDocumentSyncKind number. If omitted it defaults to `TextDocumentSyncKind.None`.
     */
    TextDocumentSyncKind textDocumentSync = TextDocumentSyncKind::None;
    bool resolveProvider = false;
    std::vector<std::string> executeCommands;
    std::vector<std::string> signatureHelpTrigger;
    std::vector<std::string> formattingTrigger;
    std::vector<std::string> completionTrigger;
    bool hasProvider(std::string &name) {
        if (capabilities.contains(name)) {
            if (capabilities[name].type() == json::value_t::boolean) {
                return capabilities["name"];
            }
        }
        return false;
    }
};
JSON_SERIALIZE(ServerCapabilities, {}, {
    value.capabilities = j;
    FROM_KEY(textDocumentSync);
    j["documentOnTypeFormattingProvider"]["firstTriggerCharacter"].get_to(value.formattingTrigger);
    j["completionProvider"]["resolveProvider"].get_to(value.resolveProvider);
    j["completionProvider"]["triggerCharacters"].get_to(value.completionTrigger);
    j["executeCommandProvider"]["commands"].get_to(value.executeCommands);
});

struct ClangdCompileCommand {
    TextType workingDirectory;
    std::vector<TextType> compilationCommand;
};
JSON_SERIALIZE(ClangdCompileCommand,MAP_JSON(
        MAP_KEY(workingDirectory), MAP_KEY(compilationCommand)), {});

struct ConfigurationSettings {
    // Changes to the in-memory compilation database.
    // The key of the map is a file name.
    std::map<std::string, ClangdCompileCommand> compilationDatabaseChanges;
};
JSON_SERIALIZE(ConfigurationSettings, MAP_JSON(MAP_KEY(compilationDatabaseChanges)), {});

struct InitializationOptions {
    // What we can change throught the didChangeConfiguration request, we can
    // also set through the initialize request (initializationOptions field).
    ConfigurationSettings configSettings;

    std::optional<TextType> compilationDatabasePath;
    // Additional flags to be included in the "fallback command" used when
    // the compilation database doesn't describe an opened file.
    // The command used will be approximately `clang $FILE $fallbackFlags`.
    std::vector<TextType> fallbackFlags;

    /// Clients supports show file status for textDocument/clangd.fileStatus.
    bool clangdFileStatus = true;
};
JSON_SERIALIZE(InitializationOptions, MAP_JSON(
                MAP_KEY(configSettings),
                MAP_KEY(compilationDatabasePath),
                MAP_KEY(fallbackFlags),
                MAP_KEY(clangdFileStatus)), {});

// InitializeParams — migrated to lsp::InitializeParams.
// Built in LSPProjectWrapper::Initialize() using lsp types + LSPBridge.

struct ShowMessageParams {
    /// The message type.
    MessageType type = MessageType::Info;
    /// The actual message.
    std::string message;
};
JSON_SERIALIZE(ShowMessageParams, {}, {FROM_KEY(type); FROM_KEY(message)});

struct Registration {
    /**
     * The id used to register the request. The id can be used to deregister
     * the request again.
     */
    TextType id;
    /**
     * The method / capability to register for.
     */
    TextType method;
};
JSON_SERIALIZE(Registration, MAP_JSON(MAP_KEY(id), MAP_KEY(method)), {});

struct RegistrationParams {
    std::vector<Registration> registrations;
};
JSON_SERIALIZE(RegistrationParams, MAP_JSON(MAP_KEY(registrations)), {});

struct UnregistrationParams {
    std::vector<Registration> unregisterations;
};
JSON_SERIALIZE(UnregistrationParams, MAP_JSON(MAP_KEY(unregisterations)), {});

// DidOpenTextDocumentParams — migrated to lsp-framework types.
// Constructed as lsp::DidOpenTextDocumentParams in LSPProjectWrapper.cpp.
struct DidOpenTextDocumentParams {
/// The document that was opened.
    TextDocumentItem textDocument;
};
JSON_SERIALIZE(DidOpenTextDocumentParams, MAP_JSON(MAP_KEY(textDocument)), {});

// DidCloseTextDocumentParams — migrated to lsp-framework types.
struct DidCloseTextDocumentParams {
    /// The document that was closed.
    TextDocumentIdentifier textDocument;
};
JSON_SERIALIZE(DidCloseTextDocumentParams, MAP_JSON(MAP_KEY(textDocument)), {});

// DidSaveTextDocumentParams — migrated to lsp-framework types.
struct DidSaveTextDocumentParams {
  /// The document that was saved.
  TextDocumentIdentifier textDocument;
};
JSON_SERIALIZE(DidSaveTextDocumentParams, MAP_JSON(MAP_KEY(textDocument)), {});

// TextDocumentContentChangeEvent — migrated to lsp-framework types.
// DidChangeTextDocumentParams — migrated to lsp-framework types.

enum class FileChangeType {
    /// The file got created.
            Created = 1,
    /// The file got changed.
            Changed = 2,
    /// The file got deleted.
            Deleted = 3
};
struct FileEvent {
    /// The file's URI.
    DocumentUri uri;
    /// The change type.
    FileChangeType type = FileChangeType::Created;
};
JSON_SERIALIZE(FileEvent, MAP_JSON(MAP_KEY(uri), MAP_KEY(type)), {});

struct DidChangeWatchedFilesParams {
    /// The actual file events.
    std::vector<FileEvent> changes;
};
JSON_SERIALIZE(DidChangeWatchedFilesParams, MAP_JSON(MAP_KEY(changes)), {});

struct DidChangeConfigurationParams {
    ConfigurationSettings settings;
};
JSON_SERIALIZE(DidChangeConfigurationParams, MAP_JSON(MAP_KEY(settings)), {});

// DocumentRangeFormattingParams — migrated to lsp-framework types.
struct DocumentRangeFormattingParams {
    /// The document to format.
    TextDocumentIdentifier textDocument;

    /// The range to format
    Range range;
};
JSON_SERIALIZE(DocumentRangeFormattingParams, MAP_JSON(MAP_KEY(textDocument), MAP_KEY(range)), {});

// DocumentOnTypeFormattingParams — migrated to lsp-framework types.
struct DocumentOnTypeFormattingParams {
    /// The document to format.
    TextDocumentIdentifier textDocument;

    /// The position at which this request was sent.
    Position position;

    /// The character that has been typed.
    TextType ch;
};
JSON_SERIALIZE(DocumentOnTypeFormattingParams, MAP_JSON(MAP_KEY(textDocument), MAP_KEY(position), MAP_KEY(ch)), {});

// FoldingRangeParams — migrated to lsp-framework types.
struct FoldingRangeParams {
    /// The document to format.
    TextDocumentIdentifier textDocument;
};
JSON_SERIALIZE(FoldingRangeParams, MAP_JSON(MAP_KEY(textDocument)), {});

enum class FoldingRangeKind {
    Comment,
    Imports,
    Region,
};
NLOHMANN_JSON_SERIALIZE_ENUM(FoldingRangeKind, {
    {FoldingRangeKind::Comment, "comment"},
    {FoldingRangeKind::Imports, "imports"},
    {FoldingRangeKind::Region, "region"}
})

struct FoldingRange {
    /**
     * The zero-based line number from where the folded range starts.
     */
    int startLine;
    /**
     * The zero-based character offset from where the folded range starts.
     * If not defined, defaults to the length of the start line.
     */
    int startCharacter;
    /**
     * The zero-based line number where the folded range ends.
     */
    int endLine;
    /**
     * The zero-based character offset before the folded range ends.
     * If not defined, defaults to the length of the end line.
     */
    int endCharacter;

    FoldingRangeKind kind;
};
JSON_SERIALIZE(FoldingRange, {}, {
    FROM_KEY(startLine);
    FROM_KEY(startCharacter);
    FROM_KEY(endLine);
    FROM_KEY(endCharacter);
    FROM_KEY(kind);
});

// SelectionRangeParams — migrated to lsp-framework types.
struct SelectionRangeParams {
    /// The document to format.
    TextDocumentIdentifier textDocument;
    std::vector<Position> positions;
};
JSON_SERIALIZE(SelectionRangeParams, MAP_JSON(MAP_KEY(textDocument), MAP_KEY(positions)), {});

struct SelectionRange {
    Range range;
    std::unique_ptr<SelectionRange> parent;
};
JSON_SERIALIZE(SelectionRange, {}, {
    FROM_KEY(range);
    if (j.contains("parent")) {
        value.parent = std::make_unique<SelectionRange>();
        j.at("parent").get_to(*value.parent);
    }
});

// DocumentFormattingParams — migrated to lsp-framework types.
struct DocumentFormattingParams {
    /// The document to format.
    TextDocumentIdentifier textDocument;
};
JSON_SERIALIZE(DocumentFormattingParams, MAP_JSON(MAP_KEY(textDocument)), {});

// DocumentSymbolParams — migrated to lsp-framework types.
struct DocumentSymbolParams {
    // The text document to find symbols in.
    TextDocumentIdentifier textDocument;
};
JSON_SERIALIZE(DocumentSymbolParams, MAP_JSON(MAP_KEY(textDocument)), {});

// Diagnostic and DiagnosticRelatedInformation — migrated to lsp-framework types.
// Bridge serializer so nlohmann can serialize/deserialize lsp::Diagnostic
// (needed by CodeActionContext and PublishDiagnosticsParams).
namespace nlohmann {
    template <>
    struct adl_serializer<lsp::Diagnostic> {
        static void to_json(json& j, const lsp::Diagnostic& value) {
            lsp::Diagnostic copy = value;
            j = json::parse(lsp::json::stringify(lsp::toJson(std::move(copy))));
        }
        static void from_json(const json& j, lsp::Diagnostic& value) {
            lsp::fromJson(lsp::json::parse(j.dump()), value);
        }
    };
}

// PublishDiagnosticsParams — migrated to lsp-framework types (was already unused).
struct PublishDiagnosticsParams {
    /**
     * The URI for which diagnostic information is reported.
     */
    std::string uri;
    /**
     * An array of diagnostic information items.
     */
    std::vector<Diagnostic> diagnostics;
};
JSON_SERIALIZE(PublishDiagnosticsParams, {}, {FROM_KEY(uri);FROM_KEY(diagnostics);});

// CodeActionContext — migrated to lsp::CodeActionContext.
struct CodeActionContext {
    /// An array of diagnostics.
    std::vector<Diagnostic> diagnostics;
};
JSON_SERIALIZE(CodeActionContext, MAP_JSON(MAP_KEY(diagnostics)), {});

// CodeActionParams — migrated to lsp::CodeActionParams.
struct CodeActionParams {
    /// The document in which the command was invoked.
    TextDocumentIdentifier textDocument;

    /// The range for which the command was invoked.
    Range range;

    /// Context carrying additional information.
    CodeActionContext context;
};
JSON_SERIALIZE(CodeActionParams, MAP_JSON(MAP_KEY(textDocument), MAP_KEY(range), MAP_KEY(context)), {});

//struct WorkspaceEdit — migrated to lsp-framework (see LSPCompat.h)
//struct TweakArgs — removed (clangd-specific, only used in old CodeAction serialization)
//struct ExecuteCommandParams — removed (clangd-specific)
//struct LspCommand — removed (clangd-specific, replaced by lsp::Command)
//struct CodeAction — migrated to lsp-framework (see LSPCompat.h)

// Bridge serializers for lsp::WorkspaceEdit and lsp::CodeAction
// (needed by ApplyWorkspaceEditParams and CodeActionResolve).
namespace nlohmann {
    template <>
    struct adl_serializer<lsp::WorkspaceEdit> {
        static void to_json(json& j, const lsp::WorkspaceEdit& value) {
            lsp::WorkspaceEdit copy = value;
            j = json::parse(lsp::json::stringify(lsp::toJson(std::move(copy))));
        }
        static void from_json(const json& j, lsp::WorkspaceEdit& value) {
            lsp::fromJson(lsp::json::parse(j.dump()), value);
        }
    };

    template <>
    struct adl_serializer<lsp::CodeAction> {
        static void to_json(json& j, const lsp::CodeAction& value) {
            lsp::CodeAction copy = value;
            j = json::parse(lsp::json::stringify(lsp::toJson(std::move(copy))));
        }
        static void from_json(const json& j, lsp::CodeAction& value) {
            lsp::fromJson(lsp::json::parse(j.dump()), value);
        }
    };
}


// DocumentSymbol and SymbolInformation — migrated to lsp-framework types.
// Deserialized via LSPBridge::fromNlohmann<>() instead of nlohmann.

//struct LogMessageParams (same as ShowMessageParams?)
JSON_SERIALIZE(LogMessageParams, {}, {FROM_KEY(type); FROM_KEY(message)});

struct SymbolDetails {
    TextType name;
    TextType containerName;
    /// Unified Symbol Resolution identifier
    /// This is an opaque string uniquely identifying a symbol.
    /// Unlike SymbolID, it is variable-length and somewhat human-readable.
    /// It is a common representation across several clang tools.
    /// (See USRGeneration.h)
    TextType USR;
    std::optional<TextType> ID;
};

struct WorkspaceSymbolParams {
    /// A non-empty query string
    TextType query;
};
JSON_SERIALIZE(WorkspaceSymbolParams, MAP_JSON(MAP_KEY(query)), {});

struct ApplyWorkspaceEditParams {
    WorkspaceEdit edit;
};
JSON_SERIALIZE(ApplyWorkspaceEditParams, MAP_JSON(MAP_KEY(edit)), {});

// TextDocumentPositionParams — migrated to lsp::TextDocumentPositionParams.
struct TextDocumentPositionParams {
    /// The text document.
    TextDocumentIdentifier textDocument;

    /// The position inside the text document.
    Position position;
};
JSON_SERIALIZE(TextDocumentPositionParams, MAP_JSON(MAP_KEY(textDocument), MAP_KEY(position)), {});

enum class CompletionTriggerKind {
    /// Completion was triggered by typing an identifier (24x7 code
    /// complete), manual invocation (e.g Ctrl+Space) or via API.
            Invoked = 1,
    /// Completion was triggered by a trigger character specified by
    /// the `triggerCharacters` properties of the `CompletionRegistrationOptions`.
            TriggerCharacter = 2,
    /// Completion was re-triggered as the current completion list is incomplete.
            TriggerTriggerForIncompleteCompletions = 3
};
// CompletionContext — migrated to lsp::CompletionContext.
struct CompletionContext {
    /// How the completion was triggered.
    CompletionTriggerKind triggerKind = CompletionTriggerKind::Invoked;
    /// The trigger character (a single character) that has trigger code complete.
    /// Is undefined if `triggerKind !== CompletionTriggerKind.TriggerCharacter`
    std::optional<TextType> triggerCharacter;
};
JSON_SERIALIZE(CompletionContext, MAP_JSON(MAP_KEY(triggerKind), MAP_KEY(triggerCharacter)), {});

// CompletionParams — migrated to lsp::CompletionParams.
struct CompletionParams : TextDocumentPositionParams {
    std::optional<CompletionContext> context;
};
JSON_SERIALIZE(CompletionParams, MAP_JSON(MAP_KEY(context), MAP_KEY(textDocument), MAP_KEY(position)), {});

// MarkupContent and Hover — migrated to lsp-framework types.
// See LSPCompat.h for the type aliases.
// CompletionItem and CompletionList — migrated to lsp-framework types.
// Deserialized via LSPBridge::fromNlohmann<lsp::CompletionList>() instead of nlohmann.
// SignatureHelp, SignatureInformation, ParameterInformation — migrated to lsp-framework types.
// Deserialized via LSPBridge::fromNlohmann<lsp::SignatureHelp>() instead of nlohmann.

// RenameParams — migrated to lsp::RenameParams.
struct RenameParams {
    /// The document that was opened.
    TextDocumentIdentifier textDocument;

    /// The position at which this request was sent.
    Position position;

    /// The new name of the symbol.
    std::string newName;
};
JSON_SERIALIZE(RenameParams, MAP_JSON(MAP_KEY(textDocument), MAP_KEY(position), MAP_KEY(newName)), {});

enum class DocumentHighlightKind { Text = 1, Read = 2, Write = 3 };

struct DocumentHighlight {
    /// The range this highlight applies to.
    Range range;
    /// The highlight kind, default is DocumentHighlightKind.Text.
    DocumentHighlightKind kind = DocumentHighlightKind::Text;
    friend bool operator<(const DocumentHighlight &LHS,
                          const DocumentHighlight &RHS) {
        int LHSKind = static_cast<int>(LHS.kind);
        int RHSKind = static_cast<int>(RHS.kind);
        return std::tie(LHS.range, LHSKind) < std::tie(RHS.range, RHSKind);
    }
    friend bool operator==(const DocumentHighlight &LHS,
                           const DocumentHighlight &RHS) {
        return LHS.kind == RHS.kind && LHS.range == RHS.range;
    }
};
enum class TypeHierarchyDirection: int { Children = 0, Parents = 1, Both = 2 };

struct TypeHierarchyParams : public TextDocumentPositionParams {
    /// The hierarchy levels to resolve. `0` indicates no level.
    int resolve = 0;

    /// The direction of the hierarchy levels to resolve.
    TypeHierarchyDirection direction = TypeHierarchyDirection::Parents;
};
JSON_SERIALIZE(TypeHierarchyParams, MAP_JSON(MAP_KEY(resolve), MAP_KEY(direction), MAP_KEY(textDocument), MAP_KEY(position)), {});

struct TypeHierarchyItem {
    /// The human readable name of the hierarchy item.
    std::string name;

    /// Optional detail for the hierarchy item. It can be, for instance, the
    /// signature of a function or method.
    std::optional<std::string> detail;

    /// The kind of the hierarchy item. For instance, class or interface.
    SymbolKind kind;

    /// `true` if the hierarchy item is deprecated. Otherwise, `false`.
    bool deprecated;

    /// The URI of the text document where this type hierarchy item belongs to.
    DocumentUri uri;

    /// The range enclosing this type hierarchy item not including
    /// leading/trailing whitespace but everything else like comments. This
    /// information is typically used to determine if the client's cursor is
    /// inside the type hierarch item to reveal in the symbol in the UI.
    Range range;

    /// The range that should be selected and revealed when this type hierarchy
    /// item is being picked, e.g. the name of a function. Must be contained by
    /// the `range`.
    Range selectionRange;

    /// If this type hierarchy item is resolved, it contains the direct parents.
    /// Could be empty if the item does not have direct parents. If not defined,
    /// the parents have not been resolved yet.
    std::optional<std::vector<TypeHierarchyItem>> parents;

    /// If this type hierarchy item is resolved, it contains the direct children
    /// of the current item. Could be empty if the item does not have any
    /// descendants. If not defined, the children have not been resolved.
    std::optional<std::vector<TypeHierarchyItem>> children;

    /// The protocol has a slot here for an optional 'data' filed, which can
    /// be used to identify a type hierarchy item in a resolve request. We don't
    /// need this (the item itself is sufficient to identify what to resolve)
    /// so don't declare it.
};

// ReferenceParams — migrated to lsp::ReferenceParams.
struct ReferenceParams : public TextDocumentPositionParams {
    // For now, no options like context.includeDeclaration are supported.
};
JSON_SERIALIZE(ReferenceParams, MAP_JSON(MAP_KEY(textDocument), MAP_KEY(position)), {});
struct FileStatus {
    /// The text document's URI.
    DocumentUri uri;
    /// The human-readable string presents the current state of the file, can be
    /// shown in the UI (e.g. status bar).
    TextType state;
    // FIXME: add detail messages.
};

// DocumentLinkParams — migrated to lsp::DocumentLinkParams.
struct DocumentLinkParams {
  /// The document to provide document links for.
  TextDocumentIdentifier textDocument;
};
JSON_SERIALIZE(DocumentLinkParams, MAP_JSON(MAP_KEY(textDocument)), {});



// A range in a text document that links to an internal or external resource,
// like another text document or a web site.
struct DocumentLink {
  // The range this link applies to.
  Range range;

  // The uri this link points to. If missing a resolve request is sent later.
  std::string target;

  // TODO(forster): The following optional fields defined by the language
  // server protocol are unsupported:
  //
  // data?: any - A data entry field that is preserved on a document link
  //              between a DocumentLinkRequest and a
  //              DocumentLinkResolveRequest.
  friend bool operator==(const DocumentLink &LHS, const DocumentLink &RHS) {
    return LHS.range == RHS.range && LHS.target == RHS.target;
  }

  friend bool operator!=(const DocumentLink &LHS, const DocumentLink &RHS) {
    return !(LHS == RHS);
  }
};

JSON_SERIALIZE(DocumentLink, MAP_JSON(MAP_KEY(range), MAP_KEY(target)), {FROM_KEY(range);FROM_KEY(target);});

#endif //LSP_PROTOCOL_H
