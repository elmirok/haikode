# Phase 2 — Detailed Plan: Create Type Compatibility Layer (`LSPCompat.h`)

## Objective

Create a compatibility header that makes lsp-framework types available under
Genio's existing names, so the rest of the codebase (LSPEditorWrapper,
LSPProjectWrapper, CallTipContext, FunctionsOutlineView) can be migrated
**one type at a time** without breaking compilation.

---

## Pre-requisites

- lsp-framework is built and `liblsp.a` is linked (Phase 1 — done).
- `<lsp/types.h>` and `<lsp/messages.h>` are reachable from the include path.

---

## Key Differences Between Genio Types and lsp-framework Types

### 1. `option<T>` vs `std::optional<T>`

| Aspect | Genio (`uri.h`) | lsp-framework |
|---|---|---|
| Type | `option<T>` (custom class) | `Opt<T>` = `std::optional<T>` |
| Check | `.has()` | `.has_value()` |
| Access | `.value()` | `.value()` or `*opt` |
| Bool | `explicit operator bool` | `explicit operator bool` |
| Pointer | `.ptr()`, `->` | `->` via std::optional |

**Used in ~40 places.** Every `option<T>` in struct fields and function signatures must migrate to `std::optional<T>`. Call sites using `.has()` must change to `.has_value()`.

### 2. `Position` — `int` vs `uint`

| | Genio | lsp-framework |
|---|---|---|
| `line` | `int` | `unsigned int` |
| `character` | `int` | `unsigned int` |
| Comparison operators | `==`, `!=`, `<`, `<=` | none provided |

**Breaking issue:** `LSPEditorWrapper.cpp:710` uses `position.character = -1` as a sentinel value. This must be refactored to use a different sentinel (e.g. `UINT_MAX` or a separate bool) before switching to lsp-framework's `Position`.

### 3. `DocumentUri` / `string_ref`

| | Genio | lsp-framework |
|---|---|---|
| `DocumentUri` | `string_ref` (non-owning `const char*` + length) | `lsp::DocumentUri` = `lsp::FileUri` (owns string, has `fromPath()`, `path()`) |
| `string_ref` | Custom class in `uri.h` | No equivalent — use `std::string` or `std::string_view` |
| `TextType` | `string_ref` | `lsp::String` = `std::string` |

**string_ref is used in:** `LSPProjectWrapper.h` (method signatures: `DidOpen`, `OnTypeFormatting`, `Rename`, `SendRequest`, `SendNotify`), `Transport.h/cpp` (`notify`, `request`), `protocol.h` (`TextType`, `TextDocumentItem` fields).

### 4. Struct Field Differences

#### `CompletionItem`

| Field | Genio | lsp-framework |
|---|---|---|
| `kind` | `CompletionItemKind` (enum class, direct) | `Opt<CompletionItemKindEnum>` (enumeration wrapper) |
| `detail` | `std::string` | `Opt<String>` |
| `sortText` | `std::string` | `Opt<String>` |
| `filterText` | `std::string` | `Opt<String>` |
| `insertText` | `std::string` | `Opt<String>` |
| `insertTextFormat` | `InsertTextFormat` (direct) | `Opt<InsertTextFormatEnum>` |
| `textEdit` | `TextEdit` (direct) | `Opt<OneOf<TextEdit, InsertReplaceEdit>>` (variant!) |
| `additionalTextEdits` | `std::vector<TextEdit>` | `Opt<Array<TextEdit>>` |
| `deprecated` | `bool` | `Opt<bool>` |
| `documentation` | removed | `Opt<OneOf<String, MarkupContent>>` |

**Impact:** `_DoCompletion()` accesses `item.textEdit.newText` directly — with lsp-framework this requires unwrapping the `Opt<OneOf<...>>`. Same for `item.insertText`, `item.kind`, etc. This is the most complex migration.

#### `Diagnostic`

| Field | Genio | lsp-framework |
|---|---|---|
| `severity` | `int` | `Opt<DiagnosticSeverityEnum>` |
| `source` | `std::string` | `Opt<String>` |
| `message` | `std::string` | `String` |
| `code` | commented out | `Opt<OneOf<int, String>>` |
| `relatedInformation` | `option<vector<...>>` | `Opt<Array<...>>` |
| `category` | `option<std::string>` (clangd extension) | **NOT PRESENT** |
| `codeActions` | `option<vector<CodeAction>>` (clangd extension) | **NOT PRESENT** |

**Impact:** `category` and `codeActions` are clangd-specific extensions not in the LSP spec. They cannot be represented by `lsp::Diagnostic` directly. Genio must keep a wrapper struct or store these separately.

#### `CodeAction`

| Field | Genio | lsp-framework |
|---|---|---|
| `kind` | `option<std::string>` | `Opt<CodeActionKindEnum>` (enumeration, not string) |
| `diagnostics` | `option<vector<Diagnostic>>` | `Opt<Array<Diagnostic>>` |
| `edit` | `option<WorkspaceEdit>` | `Opt<WorkspaceEdit>` |
| `command` | `option<LspCommand>` | `Opt<Command>` |
| `data` | `option<nlohmann::json>` | `Opt<LSPAny>` = `Opt<json::Value>` |

#### `WorkspaceEdit`

| Field | Genio | lsp-framework |
|---|---|---|
| `changes` | `option<map<string, vector<TextEdit>>>` | `Opt<Map<DocumentUri, Array<TextEdit>>>` |
| `documentChanges` | not present | `Opt<Array<OneOf<TextDocumentEdit, CreateFile, RenameFile, DeleteFile>>>` |

#### `Hover`

| Field | Genio | lsp-framework |
|---|---|---|
| `contents` | `MarkupContent` (single type) | `OneOf<MarkupContent, MarkedString, Array<MarkedString>>` (variant!) |
| `range` | `option<Range>` | `Opt<Range>` |

#### `SignatureInformation` / `ParameterInformation`

| Field | Genio | lsp-framework |
|---|---|---|
| `ParameterInformation.labelString` | `std::string` (custom field) | n/a |
| `ParameterInformation.labelOffsets` | `std::pair<uint, uint>` (custom field) | n/a |
| `ParameterInformation.label` | n/a (split into above) | `OneOf<String, Tuple<uint, uint>>` |
| `ParameterInformation.documentation` | `std::string` | `Opt<OneOf<String, MarkupContent>>` |

#### `DocumentSymbol` / `SymbolInformation`

| | Genio | lsp-framework |
|---|---|---|
| `SymbolKind` | plain `enum class` | `SymbolKindEnum` (Enumeration<> wrapper) |
| `kind` field | `SymbolKind` (direct int cast works) | `SymbolKindEnum` (has `.value()` to get underlying int) |
| `DocumentSymbol.detail` | `std::string` | `Opt<String>` |
| `DocumentSymbol.deprecated` | `bool` | `Opt<bool>` |
| `DocumentSymbol.children` | `std::vector<DocumentSymbol>` | `Opt<Array<DocumentSymbol>>` |
| `SymbolInformation` | standalone struct | inherits `BaseSymbolInformation` |

### 5. JSON Library

| | Genio | lsp-framework |
|---|---|---|
| JSON library | `nlohmann::json` | `lsp::json::Value` (custom) |
| Serialization | `JSON_SERIALIZE` macros + `nlohmann::adl_serializer` | `lsp::toJson()` / `lsp::fromJson()` |
| Parse | `nlohmann::json::parse(string)` | `lsp::json::parse(string_view)` |
| Stringify | `.dump()` | `lsp::json::stringify(value)` |

**The transport layer (`Transport.cpp`, `LSPProjectWrapper::MessageReceived`) still uses `nlohmann::json::parse`.** The JSON boundary will need a bridge.

---

## Implementation Steps

### Step 2.1 — Create `LSPCompat.h` with type aliases for basic types

**File:** `src/lsp-client/LSPCompat.h`

```cpp
#pragma once

#include <lsp/types.h>

// Alias lsp-framework types into Genio's existing names.
// Each alias is activated by removing the old definition from
// protocol_objects.h / protocol.h and uncommenting here.

// Basic types:
// using Position = lsp::Position;
// using Range = lsp::Range;
// using Location = lsp::Location;
// using TextEdit = lsp::TextEdit;

// Replace option<T> with std::optional<T>:
// template<typename T>
// using option = std::optional<T>;
```

This file starts empty (all aliases commented out). Each subsequent step uncomments one group and removes the corresponding Genio definition.

### Step 2.2 — Add JSON bridge function

**File:** `src/lsp-client/LSPJsonBridge.h`

Create a two-way conversion between `nlohmann::json` and `lsp::json::Value` using the raw JSON string as the interchange format:

```cpp
#pragma once
#include <json.hpp>
#include <lsp/json/json.h>

namespace LSPBridge {

// nlohmann -> lsp-framework
inline lsp::json::Value toLspJson(const nlohmann::json& nj) {
    return lsp::json::parse(nj.dump());
}

// lsp-framework -> nlohmann
inline nlohmann::json toNlohmannJson(const lsp::json::Value& lj) {
    return nlohmann::json::parse(lsp::json::stringify(lj));
}

// Deserialize an lsp-framework type from nlohmann::json
template<typename T>
T fromNlohmann(const nlohmann::json& nj) {
    T result;
    lsp::fromJson(toLspJson(nj), result);
    return result;
}

// Serialize an lsp-framework type to nlohmann::json
template<typename T>
nlohmann::json toNlohmann(T&& value) {
    return toNlohmannJson(lsp::toJson(std::forward<T>(value)));
}

} // namespace LSPBridge
```

This is the temporary glue between the old transport (nlohmann) and the new types (lsp-framework). It uses `stringify → parse` round-tripping, which is simple and correct. Performance is acceptable since LSP messages are small. This bridge is removed later when the transport layer itself migrates.

### Step 2.3 — Migrate `option<T>` → `std::optional<T>`

**Scope:** All files in `src/lsp-client/`

This is a prerequisite for adopting lsp-framework types, because all their optional fields use `std::optional<T>`.

1. In `uri.h`, keep the `option` class but add a deprecation note.
2. In each struct in `protocol_objects.h` and `protocol.h`, replace `option<T>` with `std::optional<T>`.
3. In `protocol.h`, update the `nlohmann::adl_serializer<option<T>>` to `adl_serializer<std::optional<T>>`.
4. Find-and-replace across all `.cpp` files:
   - `.has()` → `.has_value()`
   - Access patterns remain the same (`.value()`, `*opt`, `->` all work).
5. In `LSPProjectWrapper.h`, update function signatures:
   - `option<DocumentUri>` → `std::optional<DocumentUri>`
   - `option<bool>` → `std::optional<bool>`

**~40 call sites.** This is a mechanical change that can be done in one pass.

### Step 2.4 — Fix the `position.character = -1` sentinel

**File:** `src/lsp-client/LSPEditorWrapper.cpp` (around line 710)

Before `Position` can use `uint`, the `-1` sentinel must be replaced. The code uses it to detect whether a position has been set:

```cpp
// BEFORE:
position.character = -1;
// ... later:
if (position.character == -1) { ... }

// AFTER: use a separate bool
bool positionSet = false;
// ... later:
if (!positionSet) { ... positionSet = true; }
```

This is a small, isolated change.

### Step 2.5 — Migrate `Position` and `Range`

**Scope:** `protocol_objects.h`, all consuming files

1. Remove `struct Position` and `struct Range` from `protocol_objects.h`.
2. Add `using Position = lsp::Position; using Range = lsp::Range;` in `LSPCompat.h` (or directly in `protocol_objects.h`).
3. Update comparison operators: lsp-framework doesn't provide `==`, `!=`, `<`, `<=` for `Position`/`Range`. Add free-function overloads in `LSPCompat.h`:
   ```cpp
   inline bool operator==(const lsp::Position& a, const lsp::Position& b) {
       return a.line == b.line && a.character == b.character;
   }
   // ... same for !=, <, <= on Position and Range
   ```
4. Handle `int` → `uint` type change — audit all arithmetic on `.line` and `.character`:
   - `+ 1` operations (line 261, 683, 991, etc.) — safe with uint.
   - `- 1` (line 737) — needs cast or guard.
   - `AddInt32(... .line + 1)` — safe, implicit conversion.
5. Compile and test.

### Step 2.6 — Migrate `Location` and `TextEdit`

1. Remove `struct Location`, `struct TextEdit` from `protocol_objects.h`.
2. Add aliases in `LSPCompat.h`.
3. `Location.uri`: changes from `std::string` to `lsp::DocumentUri` — impacts `_DoGoTo()` which does `std::string uri = location.uri;`. Need to convert: `location.uri.toString()` or similar.
4. `TextEdit` fields are identical (`range`, `newText`) — this should be seamless.
5. Add comparison operators for `Location` if needed.

### Step 2.7 — Migrate `MarkupContent` and `Hover`

1. Remove Genio's `MarkupContent` and `Hover` from `protocol.h`.
2. Import `lsp::MarkupContent` and `lsp::Hover`.
3. **Breaking change in `Hover::contents`:** Genio uses `MarkupContent` (single struct); lsp-framework uses `OneOf<MarkupContent, MarkedString, Array<MarkedString>>`.
4. In `_DoHover()`, the deserialization with `LSPBridge::fromNlohmann<lsp::Hover>(params)` will now handle all variants. The display code must use `std::visit` or `std::get_if` to extract the markup content.

### Step 2.8 — Migrate `CompletionItem`, `CompletionList`, `CompletionItemKind`, `InsertTextFormat`

This is the **most complex step** due to field type differences.

1. Remove Genio structs from `protocol_objects.h`.
2. Import lsp-framework types.
3. Update `_DoCompletion()`:
   - `item.kind` → `item.kind.value()` (unwrap `Opt<CompletionItemKindEnum>`), cast int values.
   - `item.textEdit.newText` → Must unwrap `Opt<OneOf<TextEdit, InsertReplaceEdit>>`. Use a helper:
     ```cpp
     const TextEdit* getTextEdit(const lsp::CompletionItem& item) {
         if (!item.textEdit) return nullptr;
         if (auto* te = std::get_if<lsp::TextEdit>(&*item.textEdit)) return te;
         return nullptr; // InsertReplaceEdit not used currently
     }
     ```
   - `item.insertText` → `item.insertText.value_or("")`
   - `item.detail`, `item.sortText`, `item.filterText` → same, unwrap `Opt<String>`.
   - `item.deprecated` → `item.deprecated.value_or(false)`
   - `item.additionalTextEdits` → unwrap `Opt<Array<TextEdit>>`.
4. Update `SelectedCompletion()` similarly.
5. Update `LSPEditorWrapper.h` — `fCurrentCompletion` type changes to `lsp::CompletionList`.

### Step 2.9 — Migrate `Diagnostic` (with extension wrapper)

`lsp::Diagnostic` does **not** have `category` or `codeActions` (clangd extensions). **Strategy:** keep a Genio wrapper struct:

```cpp
// LSPCompat.h
struct GenioDiagnostic : public lsp::Diagnostic {
    std::optional<std::string> category;
    std::optional<std::vector<lsp::CodeAction>> codeActions;
};
```

Or keep the existing `LSPDiagnostic` wrapper in `LSPEditorWrapper.h` (which already bundles a `Diagnostic` with extra fields) and just change the inner `diagnostic` field type to `lsp::Diagnostic`.

Update `_DoDiagnostics()` deserialization to:
1. Deserialize base `lsp::Diagnostic` using `lsp::fromJson`.
2. Manually extract `category` and `codeActions` from the raw JSON (clangd extensions are extra JSON fields).

### Step 2.10 — Migrate `CodeAction`, `WorkspaceEdit`

1. Remove Genio's `CodeAction`, `WorkspaceEdit`, `TweakArgs`, `LspCommand`, `ExecuteCommandParams` from `protocol_objects.h`.
2. Import `lsp::CodeAction`, `lsp::WorkspaceEdit`, `lsp::Command`.
3. **`CodeAction.kind`**: `option<string>` → `Opt<CodeActionKindEnum>`. Comparison with string constants needs `.value().str()` or enum comparison.
4. **`CodeAction.data`**: `option<nlohmann::json>` → `Opt<LSPAny>` = `Opt<lsp::json::Value>`. Code in `_DoCodeActions()/_DoCodeActionResolve()` that accesses `action.data.value()["Identifier"]` needs to use `lsp::json::Value` API instead.
5. **`WorkspaceEdit.changes`**: map key changes from `std::string` to `lsp::DocumentUri`.
6. **`TweakArgs`, `LspCommand`, `ExecuteCommandParams`**: These are clangd-specific. `lsp::Command` replaces `LspCommand` partially. `TweakArgs` and `ExecuteCommandParams` are Genio-specific and stay in Genio (or get removed if unused after migration).

### Step 2.11 — Migrate `SignatureHelp`, `SignatureInformation`, `ParameterInformation`

1. Remove Genio structs from `protocol_objects.h`.
2. Import lsp-framework types.
3. **`ParameterInformation.label`**: Genio splits into `labelString` + `labelOffsets`. lsp-framework uses `OneOf<String, Tuple<uint, uint>>`. Update `CallTipContext.cpp` to use `std::visit` or `std::get_if`.
4. **`ParameterInformation.documentation`**: `std::string` → `Opt<OneOf<String, MarkupContent>>`.
5. **`SignatureHelp.activeParameter`**: `int` → `Opt<uint>`.
6. **`SignatureHelp.argListStart`** (Genio custom field): **NOT in lsp-framework**. Keep it as an extension or remove if unused.

### Step 2.12 — Migrate `DocumentSymbol`, `SymbolInformation`, `SymbolKind`

1. Remove Genio structs from `protocol_objects.h`.
2. Import lsp-framework types.
3. **`SymbolKind`**: plain `enum class` → `SymbolKindEnum` (Enumeration wrapper). The `(int32)sym.kind` casts in `_DoRecursiveDocumentSymbol` and `FunctionsOutlineView.cpp` need `.value()` first: `static_cast<int32>(sym.kind.value())`.
4. **`DocumentSymbol.detail`**: `std::string` → `Opt<String>` — `.c_str()` calls need `.value_or("").c_str()`.
5. **`DocumentSymbol.children`**: `vector<>` → `Opt<Array<>>` — `.size()` check needs `.value_or({}).size()` or `children.has_value() && children->size() > 0`.
6. **`DocumentSymbol.deprecated`**: `bool` → `Opt<bool>`.
7. **FunctionsOutlineView.cpp**: Uses `SymbolKind::File`, `SymbolKind::Method`, etc. — these become `lsp::SymbolKind::File`, etc. (the Enumeration class provides these constants).

### Step 2.13 — Migrate remaining param structs from `protocol.h`

After the type structs are migrated, replace param structs one at a time. These are only used in `LSPProjectWrapper.cpp` method implementations.

| Genio struct | lsp-framework equivalent | Used in |
|---|---|---|
| `InitializeParams` | `lsp::InitializeParams` | `Initialize()` |
| `ClientCapabilities` | `lsp::ClientCapabilities` (via InitializeParams) | `Initialize()` |
| `DidOpenTextDocumentParams` | `lsp::notifications::TextDocument_DidOpen::Params` | `DidOpen()` |
| `DidCloseTextDocumentParams` | `lsp::notifications::TextDocument_DidClose::Params` | `DidClose()` |
| `DidChangeTextDocumentParams` | `lsp::notifications::TextDocument_DidChange::Params` | `DidChange()` |
| `DidSaveTextDocumentParams` | `lsp::notifications::TextDocument_DidSave::Params` | `DidSave()` |
| `CompletionParams` | `lsp::requests::TextDocument_Completion::Params` | `Completion()` |
| `TextDocumentPositionParams` | `lsp::TextDocumentPositionParams` | `Hover()`, `GoTo*()`, etc. |
| `DocumentFormattingParams` | `lsp::requests::TextDocument_Formatting::Params` | `Formatting()` |
| `DocumentRangeFormattingParams` | `lsp::requests::TextDocument_RangeFormatting::Params` | `RangeFomatting()` |
| `CodeActionParams` | `lsp::CodeActionParams` | `CodeAction()` |
| `RenameParams` | `lsp::requests::TextDocument_Rename::Params` | `Rename()` |
| `DocumentSymbolParams` | `lsp::DocumentSymbolParams` | `DocumentSymbol()` |
| `ReferenceParams` | `lsp::ReferenceParams` | `References()` |
| `PublishDiagnosticsParams` | `lsp::PublishDiagnosticsParams` | `_DoDiagnostics()` |

For each: replace the struct, update `SendNotify`/`SendRequest` to serialize via `LSPBridge::toNlohmann()`, compile, test.

**Note on `ClientCapabilities`:** Genio's hand-built `ClientCapabilities` struct is heavily customized with clangd-specific fields and a complex `JSON_SERIALIZE` block. lsp-framework's `ClientCapabilities` is auto-generated from the spec. The advertised capabilities may differ. **This migration must be verified carefully** — incorrect capabilities will cause the server to enable/disable features incorrectly.

### Step 2.14 — Remove dead code

After all types are migrated:

1. Delete all `JSON_SERIALIZE(...)` blocks from `protocol.h` and `protocol_objects.h`.
2. Delete all `NLOHMANN_JSON_SERIALIZE_ENUM(...)` blocks.
3. Delete the `MAP_JSON`/`MAP_KEY`/`MAP_TO`/`MAP_KV`/`FROM_KEY`/`JSON_SERIALIZE` macro definitions.
4. Delete remaining empty struct definitions.
5. If `protocol_objects.h` is now empty, delete it and update `#include`s.
6. If `protocol.h` is now empty (or only contains non-type definitions), clean it up.
7. Delete the `option<T>` class from `uri.h`.
8. Delete `string_ref` from `uri.h` if fully replaced by `std::string`/`std::string_view`.
9. Delete `DocumentUri = string_ref` alias from `uri.h`.
10. Remove `LSPCompat.h` if all aliases are now direct includes.
11. Remove `LSPJsonBridge.h` if the transport layer has been updated (or keep it for the future connection migration).

---

## Include Dependency Chain (before → after)

**Before:**
```
FunctionsOutlineView.cpp → protocol_objects.h → uri.h
LSPEditorWrapper.h       → protocol_objects.h → uri.h
                         → LSPProjectWrapper.h → protocol_objects.h
LSPEditorWrapper.cpp     → protocol.h → protocol_objects.h → uri.h
LSPProjectWrapper.h      → protocol_objects.h → uri.h
LSPProjectWrapper.cpp    → protocol.h → protocol_objects.h → uri.h
CallTipContext.h         → protocol_objects.h → uri.h
MessageHandler.h         → uri.h
```

**After:**
```
FunctionsOutlineView.cpp → LSPCompat.h → <lsp/types.h>
LSPEditorWrapper.h       → LSPCompat.h → <lsp/types.h>
                         → LSPProjectWrapper.h → LSPCompat.h
LSPEditorWrapper.cpp     → LSPCompat.h, LSPJsonBridge.h
LSPProjectWrapper.h      → LSPCompat.h → <lsp/types.h>
LSPProjectWrapper.cpp    → LSPCompat.h, LSPJsonBridge.h, <lsp/messages.h>
CallTipContext.h         → LSPCompat.h → <lsp/types.h>
MessageHandler.h         → (unchanged until connection migration)
```

---

## Recommended Order of Execution

| # | Step | Risk | Compile gate |
|---|---|---|---|
| 1 | Create `LSPCompat.h` (empty) + `LSPJsonBridge.h` | None | Yes |
| 2 | Migrate `option<T>` → `std::optional<T>` everywhere | Low | Yes |
| 3 | Fix `position.character = -1` sentinel | Low | Yes |
| 4 | Migrate `Position`, `Range` + add comparison operators | Medium | Yes |
| 5 | Migrate `Location`, `TextEdit` | Low | Yes |
| 6 | Migrate `MarkupContent`, `Hover` | Medium | Yes |
| 7 | Migrate `CompletionItem`, `CompletionList` + helpers | **High** | Yes |
| 8 | Migrate `Diagnostic` (with extension wrapper) | Medium | Yes |
| 9 | Migrate `CodeAction`, `WorkspaceEdit` | Medium | Yes |
| 10 | Migrate `SignatureHelp`, `ParameterInformation` | Medium | Yes |
| 11 | Migrate `DocumentSymbol`, `SymbolInformation`, `SymbolKind` | Medium | Yes |
| 12 | Migrate param structs from `protocol.h` | Medium | Yes |
| 13 | Migrate `ClientCapabilities` / `InitializeParams` | **High** | Yes + full server test |
| 14 | Remove dead code, delete old files | Low | Yes |

**Each step is an independent, compilable commit.** Test with clangd after each step: open a C++ file, trigger completion, hover, go-to-definition, view diagnostics, format, rename, and check the document outline.

---

## Files Modified Per Step Summary

| File | Steps |
|---|---|
| `src/lsp-client/LSPCompat.h` (new) | 1, 4–11 |
| `src/lsp-client/LSPJsonBridge.h` (new) | 1, 5+ |
| `src/lsp-client/protocol_objects.h` | 2, 4–11 (progressively emptied) |
| `src/lsp-client/protocol.h` | 2, 6–7, 12–13 (progressively emptied) |
| `src/lsp-client/uri.h` | 2, 14 |
| `src/lsp-client/LSPEditorWrapper.h` | 4, 7–11 |
| `src/lsp-client/LSPEditorWrapper.cpp` | 2–12 |
| `src/lsp-client/LSPProjectWrapper.h` | 2, 4–6, 12 |
| `src/lsp-client/LSPProjectWrapper.cpp` | 2, 12–13 |
| `src/lsp-client/CallTipContext.h` | 10 |
| `src/lsp-client/CallTipContext.cpp` | 10 |
| `src/ui/FunctionsOutlineView.cpp` | 11 |
| `src/lsp-client/MessageHandler.h` | 14 (remove `uri.h` include) |
