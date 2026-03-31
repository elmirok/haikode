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

using TextType = std::string;

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

// Clangd-specific structs (no lsp-framework equivalent).
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

// All other LSP types (params, responses, enums) have been migrated to
// lsp-framework types. See LSPCompat.h for type aliases.

#endif //LSP_PROTOCOL_H
