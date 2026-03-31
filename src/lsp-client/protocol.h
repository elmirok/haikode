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

// Param structs — migrated to lsp-framework types.
// DidOpenTextDocumentParams, DidCloseTextDocumentParams, DidSaveTextDocumentParams,
// DidChangeTextDocumentParams, TextDocumentContentChangeEvent,
// DocumentRangeFormattingParams, DocumentOnTypeFormattingParams,
// FoldingRangeParams, SelectionRangeParams, DocumentFormattingParams,
// DocumentSymbolParams, CompletionParams, CompletionContext,
// RenameParams, ReferenceParams, CodeActionParams, CodeActionContext,
// PublishDiagnosticsParams, TextDocumentPositionParams, DocumentLinkParams.
// All constructed as lsp::* types in LSPProjectWrapper.cpp.

// Diagnostic, CodeActionContext, CodeActionParams, PublishDiagnosticsParams —
// migrated to lsp-framework types. Deserialized via LSPBridge::fromNlohmann<>().
// WorkspaceEdit, CodeAction — migrated to lsp-framework (see LSPCompat.h).

// Bridge serializers for lsp::WorkspaceEdit and lsp::CodeAction
// (used when nlohmann needs to serialize/deserialize these lsp types directly).
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


JSON_SERIALIZE(LogMessageParams, {}, {FROM_KEY(type); FROM_KEY(message)});



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
