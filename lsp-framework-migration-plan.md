# LSP-Framework Migration Plan

Target library: [leon-bckl/lsp-framework](https://github.com/leon-bckl/lsp-framework) (v1.3.1, C++20, MIT)

## Goal

Replace Genio's hand-rolled protocol types and JSON serialization (`protocol.h`, `protocol_objects.h`, `uri.h`) with lsp-framework's auto-generated, type-safe LSP types and messages. The connection/transport layer stays untouched for now — only the **protocol** layer migrates.

---

## What lsp-framework provides (relevant to this plan)

| Component | Header | What it gives us |
|---|---|---|
| **LSP types** | `<lsp/types.h>` | All LSP structs (`Position`, `Range`, `CompletionItem`, `Diagnostic`, …) auto-generated from the official meta model. Replaces `protocol_objects.h`. |
| **LSP messages** | `<lsp/messages.h>` | Typed request/notification definitions in `lsp::requests::*` and `lsp::notifications::*` with `::Params` and `::Result` inner types. Replaces the param structs in `protocol.h`. |
| **Serialization** | `<lsp/serialization.h>` | `lsp::toJson()` / `lsp::fromJson()` using lsp-framework's own `lsp::json::Any`. Replaces the `JSON_SERIALIZE` / `MAP_JSON` / `FROM_KEY` macro system and the nlohmann/json dependency for protocol types. |
| **URI** | `<lsp/uri.h>`, `<lsp/fileuri.h>` | `lsp::DocumentUri`, `lsp::URI`. Replaces `uri.h` and the `string_ref`/`DocumentUri` aliases. |
| **Enumerations** | `<lsp/enumeration.h>` | Proper C++ enums for `CompletionItemKind`, `SymbolKind`, `DiagnosticSeverity`, etc. Replaces `NLOHMANN_JSON_SERIALIZE_ENUM` blocks. |
| **MessageHandler** | `<lsp/messagehandler.h>` | `sendRequest` / `sendNotification` with typed params + callback-based or future-based response routing. Replaces `SendRequest`/`SendNotify` + manual ID routing in `LSPProjectWrapper`. |
| **Connection** | `<lsp/connection.h>` | `lsp::Connection` over `lsp::io::Stream`. **Not used in this plan** — Genio keeps its `AsyncJsonTransport` / `LSPPipeClient`. |

## What lsp-framework does NOT provide (stays in Genio)

- `AsyncJsonTransport`, `LSPPipeClient`, `LSPReaderThread` — Haiku-specific pipe/BLooper transport
- `LSPEditorWrapper`, `CallTipContext` — Scintilla editor integration
- `LSPServersManager` — server config and lifecycle
- `LSPTextDocument` — per-file state tracking
- Capability bitmask (`LSPCapabilities.h`) — could later be replaced by checking `InitializeResult.capabilities` fields directly

---

## Current Architecture

```
┌─────────────────────────────────────────────────────────┐
│  LSPEditorWrapper / CallTipContext                       │  Scintilla integration
│  (response callbacks, position conversion, UI)          │
├─────────────────────────────────────────────────────────┤
│  LSPProjectWrapper                                      │  Per-project server session
│  - outbound methods: Initialize, DidOpen, Completion…   │
│  - response routing by RequestID "docKey_methodName"    │
│  - capability negotiation                               │
├─────────────────────────────────────────────────────────┤
│  protocol.h / protocol_objects.h / uri.h                │  ← REPLACE WITH lsp-framework
│  - LSP structs + nlohmann/json serialization macros     │
│  - ~50 structs, ~30 JSON_SERIALIZE blocks               │
├─────────────────────────────────────────────────────────┤
│  MessageHandler.h                                       │  Abstract callback interface
├─────────────────────────────────────────────────────────┤
│  Transport.h/cpp, LSPPipeClient, LSPReaderThread        │  Pipe I/O, JSON-RPC framing
└─────────────────────────────────────────────────────────┘
```

---

## Migration Plan — Protocol Types Only

### Phase 1 — Build lsp-framework and integrate into the build system

**Risk: Low | Files changed: `Makefile`**

1. Add lsp-framework as a git submodule (or vendored copy) under `libs/lsp-framework/`.
2. Build it as a static library (`liblsp.a`) — it uses CMake, so either invoke CMake from the Makefile or build it separately and link the archive.
3. Add `-I libs/lsp-framework/lsp` (or the installed include path) to CFLAGS.
4. Link against `liblsp.a`.
5. Verify the existing code still compiles unchanged — lsp-framework headers shouldn't conflict yet since nothing includes them.

### Phase 2 — Create a type-mapping header (`LSPCompat.h`)

**Risk: Low | Files changed: 1 new file**

Create `src/lsp-client/LSPCompat.h` with:
- Conversion functions between Genio's current types and lsp-framework types.
- Example: `lsp::Position toLspPosition(const Position& p)` and `Position fromLspPosition(const lsp::Position& p)`.
- Cover: `Position`, `Range`, `Location`, `TextEdit`, `DocumentUri`.

This lets us migrate **one method at a time** without a big-bang replacement. The compat layer is temporary scaffolding removed at the end.

### Phase 3 — Replace protocol_objects.h types one-by-one

**Risk: Medium | Files changed: `protocol_objects.h`, `LSPEditorWrapper.cpp`, `LSPProjectWrapper.cpp`**

Migrate structs from `protocol_objects.h` to their lsp-framework equivalents, one group at a time. After each group, compile and test.

**Order** (dependencies flow downward):

| Step | Genio type | lsp-framework type | Notes |
|---|---|---|---|
| 3a | `Position`, `Range` | `lsp::Position`, `lsp::Range` | Used everywhere — most impactful. Use `using Position = lsp::Position;` in a compat header to minimize churn. |
| 3b | `Location`, `TextEdit` | `lsp::Location`, `lsp::TextEdit` | Used in GoTo responses, formatting, rename. |
| 3c | `CompletionItem`, `CompletionList`, `CompletionItemKind`, `InsertTextFormat` | `lsp::CompletionItem`, `lsp::CompletionList`, etc. | `_DoCompletion()` in `LSPEditorWrapper` will need field name adjustments since lsp-framework uses the spec names. |
| 3d | `Diagnostic`, `DiagnosticRelatedInformation`, `CodeAction`, `WorkspaceEdit` | `lsp::Diagnostic`, `lsp::CodeAction`, `lsp::WorkspaceEdit` | Complex — `CodeAction` has `option<>` fields. lsp-framework uses `std::optional` instead of Genio's `option<>`. |
| 3e | `SignatureHelp`, `SignatureInformation`, `ParameterInformation` | `lsp::SignatureHelp`, etc. | `CallTipContext` needs updating. |
| 3f | `DocumentSymbol`, `SymbolInformation`, `SymbolKind` | `lsp::DocumentSymbol`, etc. | Used in `_DoDocumentSymbol()`. |
| 3g | `Hover`, `MarkupContent` | `lsp::Hover`, `lsp::MarkupContent` | Simple, few usages. |

**Strategy for each step:**
1. Add `#include <lsp/types.h>` where needed.
2. Replace the Genio struct with a `using` alias (e.g., `using Position = lsp::Position;`) or direct usage.
3. Remove the corresponding `JSON_SERIALIZE(...)` macro block — lsp-framework handles serialization.
4. Fix any field name mismatches (e.g., Genio's `option<T>` → `std::optional<T>`).
5. Compile. Fix. Test with clangd.

### Phase 4 — Replace protocol.h param/result structs

**Risk: Medium | Files changed: `protocol.h`, `LSPProjectWrapper.cpp`**

Replace the request/notification parameter structs with lsp-framework message types. Same incremental approach:

| Step | Genio param struct | lsp-framework message | Used in |
|---|---|---|---|
| 4a | `InitializeParams`, `ClientCapabilities` | `lsp::requests::Initialize::Params` | `LSPProjectWrapper::Initialize()` |
| 4b | `DidOpenTextDocumentParams`, `DidCloseTextDocumentParams`, `DidChangeTextDocumentParams`, `DidSaveTextDocumentParams` | `lsp::notifications::TextDocument_DidOpen::Params`, etc. | `DidOpen()`, `DidClose()`, `DidChange()`, `DidSave()` |
| 4c | `CompletionParams`, `CompletionContext` | `lsp::requests::TextDocument_Completion::Params` | `Completion()` |
| 4d | `TextDocumentPositionParams` | `lsp::requests::TextDocument_Hover::Params`, `::Definition::Params`, etc. | `Hover()`, `GoToDefinition()`, etc. |
| 4e | `DocumentFormattingParams`, `DocumentRangeFormattingParams`, `DocumentOnTypeFormattingParams` | `lsp::requests::TextDocument_Formatting::Params`, etc. | `Formatting()`, `RangeFomatting()` |
| 4f | `RenameParams`, `CodeActionParams`, `CodeActionContext` | `lsp::requests::TextDocument_Rename::Params`, etc. | `Rename()`, `CodeAction()` |
| 4g | `DocumentSymbolParams`, `ReferenceParams` | `lsp::requests::TextDocument_DocumentSymbol::Params`, etc. | `DocumentSymbol()`, `References()` |

**Key change in each method:**
```cpp
// BEFORE (Genio today):
void LSPProjectWrapper::DidOpen(LSPTextDocument* doc, string_ref text, string_ref langId) {
    DidOpenTextDocumentParams params;
    params.textDocument.uri = doc->GetFilenameURI().String();
    params.textDocument.text = text;
    params.textDocument.languageId = langId;
    SendNotify("textDocument/didOpen", params);  // nlohmann::json serialization
}

// AFTER (with lsp-framework types):
void LSPProjectWrapper::DidOpen(LSPTextDocument* doc, string_ref text, string_ref langId) {
    auto params = lsp::notifications::TextDocument_DidOpen::Params{
        .textDocument = {
            .uri        = lsp::DocumentUri::fromPath(doc->GetFilenameURI().String()),
            .languageId = langId.str(),
            .version    = 0,
            .text       = text.str()
        }
    };
    // Still use existing SendNotify — but serialize via lsp::toJson(params) instead of nlohmann
    SendNotify("textDocument/didOpen", lsp::toJson(std::move(params)));
}
```

### Phase 5 — Adapt response handling in LSPEditorWrapper

**Risk: Medium | Files changed: `LSPEditorWrapper.cpp`**

The `_Do*` callback methods currently receive `nlohmann::json&` and deserialize manually. After phases 3–4, they should deserialize using lsp-framework:

```cpp
// BEFORE:
void LSPEditorWrapper::_DoHover(nlohmann::json& params) {
    Hover hover = params.get<Hover>();  // nlohmann JSON_SERIALIZE
    ...
}

// AFTER:
void LSPEditorWrapper::_DoHover(nlohmann::json& params) {
    // Convert nlohmann::json to lsp::json::Any, then deserialize
    auto hover = lsp::fromJson<lsp::Hover>(convertToLspJson(params));
    ...
}
```

**Important**: Since the transport layer still delivers `nlohmann::json` (via `AsyncJsonTransport`), we need a thin `nlohmann::json` ↔ `lsp::json::Any` conversion at the boundary. This is the main integration point — one function that converts between JSON representations. Alternatively, we can deserialize from the raw JSON string directly using lsp-framework's JSON parser, bypassing nlohmann entirely at the deserialization step.

### Phase 6 — Remove dead code

**Risk: Low | Files changed: `protocol.h`, `protocol_objects.h`, `uri.h`**

1. Delete all replaced struct definitions and their `JSON_SERIALIZE` blocks from `protocol.h` and `protocol_objects.h`.
2. Delete `uri.h` (replaced by `<lsp/uri.h>` / `<lsp/fileuri.h>`).
3. Remove the `option<T>` template if fully replaced by `std::optional<T>`.
4. Remove the `MAP_JSON`/`MAP_KEY`/`FROM_KEY`/`JSON_SERIALIZE` macro definitions once no structs use them.
5. If `protocol.h` and `protocol_objects.h` are now empty or near-empty, delete them entirely.
6. Remove the `LSPCompat.h` conversion functions if no longer needed.

---

## What stays unchanged (connection layer — future work)

These files are **not touched** in this plan:

| File | Reason to keep |
|---|---|
| `Transport.h/cpp` | Haiku BLooper-based async JSON-RPC transport |
| `LSPPipeClient.h/cpp` | Pipe I/O using Haiku's `PipeImage` |
| `LSPReaderThread.h/cpp` | Reader thread using Haiku's `GenericThread` |
| `MessageHandler.h` | Currently the base for `LSPTextDocument` response dispatch |

A **future Phase 7** could replace `Transport` + `LSPPipeClient` with `lsp::Connection` over a custom `lsp::io::Stream` subclass wrapping `PipeImage`. And `MessageHandler` could be replaced by `lsp::MessageHandler` (which provides typed callback routing, eliminating the manual `IF_ID` dispatch and `"docKey_methodName"` ID scheme). But that's a separate, larger change.

---

## Key principles

1. **Protocol first, connection later.** The biggest maintenance burden is the hand-written type definitions and JSON serialization — ~800 lines of boilerplate in `protocol.h` + `protocol_objects.h`. Replacing those with auto-generated types is the highest-value change.
2. **One struct group at a time.** Each step produces a compilable, testable state.
3. **Compat layer as scaffolding.** `LSPCompat.h` bridges old ↔ new during the transition; it gets deleted at the end.
4. **JSON boundary adapter.** The only tricky integration point is `nlohmann::json` ↔ `lsp::json::Any` at the transport/protocol boundary. Keep this to one conversion function.
5. **Test with clangd after each step.** Verify: completion, hover, go-to-definition, diagnostics, formatting, rename, document symbols.
6. **No wire-protocol changes.** The JSON-RPC messages must be identical before and after each step.
