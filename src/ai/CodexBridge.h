/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include "VibeCoding.h"

#include <string>

namespace Haikode::AI {

enum class CodexLoginState {
	Unavailable,
	LoggedOut,
	LoggedIn,
	Unknown
};

struct CodexBridgeSettings {
	std::string executable = "codex";
	std::string projectRoot;
	std::string model;
	bool useOss = false;
	std::string localProvider;
	std::string outputLastMessagePath;
};

class CodexBridge {
public:
	static std::string FindExecutable(const std::string& executableName,
		const std::string& pathEnv, std::string& error);
	static CodexLoginState ParseLoginStatus(const std::string& output);
	static const char* LoginStateLabel(CodexLoginState state);
	static std::string FormatLoginStatusSummary(const std::string& output);

	static CommandRequest LoginStatusCommand(const std::string& executable);
	static CommandRequest DeviceLoginCommand(const std::string& executable);
	static bool IsLoginStatusCommand(const CommandRequest& command);
	static PromptBuildResult BuildReadOnlyPrompt(
		const VibeCodingRequest& request, size_t maxBytesPerFile,
		size_t maxFiles, size_t maxProjectFiles = 50);
	static bool BuildReadOnlyAskCommand(const CodexBridgeSettings& settings,
		const std::string& prompt, CommandRequest& command,
		std::string& error);
	static bool IsReadOnlyAskCommand(const CommandRequest& command);
};

} // namespace Haikode::AI
