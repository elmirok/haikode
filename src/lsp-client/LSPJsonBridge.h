/*
 * Copyright 2026, Andrea Anzani
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

/*
 * Bridge between nlohmann::json (used by the existing transport layer)
 * and lsp::json::Value (used by lsp-framework types).
 *
 * Uses stringify → parse round-tripping. Simple and correct for the
 * small payloads typical of LSP messages.
 *
 * This file is temporary scaffolding — it will be removed once the
 * transport layer itself migrates to lsp-framework's Connection.
 */

#include <json.hpp>
#include <lsp/json/json.h>
#include <lsp/serialization.h>

namespace LSPBridge {

// nlohmann::json → lsp::json::Value
inline lsp::json::Value toLspJson(const nlohmann::json& nj)
{
	return lsp::json::parse(nj.dump());
}

// lsp::json::Value → nlohmann::json
inline nlohmann::json toNlohmannJson(const lsp::json::Value& lj)
{
	return nlohmann::json::parse(lsp::json::stringify(lj));
}

// Deserialize an lsp-framework type from a nlohmann::json value
template<typename T>
T fromNlohmann(const nlohmann::json& nj)
{
	T result{};
	lsp::fromJson(toLspJson(nj), result);
	return result;
}

// Serialize an lsp-framework type into a nlohmann::json value
template<typename T>
nlohmann::json toNlohmann(T&& value)
{
	return toNlohmannJson(lsp::toJson(std::forward<T>(value)));
}

} // namespace LSPBridge
