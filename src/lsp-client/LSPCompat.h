/*
 * Copyright 2026, Andrea Anzani
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include <lsp/json/json.h>
#include <lsp/types.h>

// Comparison operators not provided by lsp-framework.
namespace lsp {
	inline bool operator==(const lsp::Position& a, const lsp::Position& b) {
		return a.line == b.line && a.character == b.character;
	}
	inline bool operator==(const lsp::Range& a, const lsp::Range& b) {
		return a.start == b.start && a.end == b.end;
	}
	inline bool operator!=(const lsp::Range& a, const lsp::Range& b) {
		return !(a == b);
	}
} // namespace lsp

