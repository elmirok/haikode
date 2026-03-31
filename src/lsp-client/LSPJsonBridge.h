/*
 * Copyright 2026, Andrea Anzani
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

/*
 * Convenience wrappers around lsp::fromJson / lsp::toJson.
 *
 * fromJson<T>(val) — Deserialize a typed lsp-framework struct from a json::Value.
 * toJson(val)      — Serialize a typed lsp-framework struct into a json::Value.
 *                    Accepts lvalues (copies internally) and rvalues (moves).
 */

#include <lsp/json/json.h>
#include <lsp/serialization.h>

namespace LSPBridge {

// Deserialize an lsp-framework type from a lsp::json::Value
template<typename T>
T fromJson(const lsp::json::Value& j)
{
	T result{};
	lsp::fromJson(lsp::json::Value(j), result);
	return result;
}

// Serialize an lsp-framework type into a lsp::json::Value.
// lsp::toJson() requires rvalues, so we always copy+move to handle lvalue args.
template<typename T>
lsp::json::Value toJson(T&& val)
{
	using U = std::remove_cvref_t<T>;
	U copy{std::forward<T>(val)};
	return lsp::toJson(std::move(copy));
}

} // namespace LSPBridge
