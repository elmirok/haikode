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

// Most LSP types have been migrated to lsp-framework (see LSPCompat.h).
// This file retains only MessageType and LogMessageParams.

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
