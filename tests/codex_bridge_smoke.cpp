/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "ai/CodexBridge.h"
#include "ai/VibeCoding.h"

#include <algorithm>
#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

static void
WriteFile(const fs::path& path, const std::string& text)
{
	std::ofstream file(path, std::ios::binary);
	file << text;
}

int
main()
{
	const fs::path root = fs::temp_directory_path() / "haikode-codex-bridge";
	fs::remove_all(root);
	fs::create_directories(root / "bin");
	WriteFile(root / "bin" / "codex", "#!/bin/sh\n");

	std::string error;
	assert(Haikode::AI::CodexBridge::FindExecutable("codex",
		(root / "bin").string(), error) == (root / "bin" / "codex").string());
	assert(Haikode::AI::CodexBridge::FindExecutable(
		(root / "bin" / "codex").string(), "", error)
		== (root / "bin" / "codex").string());
	assert(Haikode::AI::CodexBridge::FindExecutable("missing",
		(root / "bin").string(), error).empty());
	assert(error.find("not found") != std::string::npos);

	assert(Haikode::AI::CodexBridge::ParseLoginStatus(
		"Logged in using ChatGPT\n") == Haikode::AI::CodexLoginState::LoggedIn);
	assert(Haikode::AI::CodexBridge::ParseLoginStatus(
		"auth is configured") == Haikode::AI::CodexLoginState::LoggedIn);
	assert(Haikode::AI::CodexBridge::ParseLoginStatus(
		"Not logged in") == Haikode::AI::CodexLoginState::LoggedOut);
	assert(Haikode::AI::CodexBridge::LoginStateLabel(
		Haikode::AI::CodexLoginState::LoggedIn) == std::string("Logged in"));

	Haikode::AI::CommandRequest status
		= Haikode::AI::CodexBridge::LoginStatusCommand("/bin/codex");
	assert(status.argv.size() == 3);
	assert(status.argv[0] == "/bin/codex");
	assert(status.argv[1] == "login");
	assert(status.argv[2] == "status");
	assert(status.runnable);

	Haikode::AI::CommandRequest login
		= Haikode::AI::CodexBridge::DeviceLoginCommand("/bin/codex");
	assert(login.argv.size() == 3);
	assert(login.argv[1] == "login");
	assert(login.argv[2] == "--device-auth");

	Haikode::AI::CodexBridgeSettings settings;
	settings.executable = "/bin/codex";
	settings.projectRoot = root.string();
	settings.model = "gpt-5";
	settings.outputLastMessagePath = (root / ".haikode" / "codex-last.txt").string();
	Haikode::AI::CommandRequest ask;
	assert(Haikode::AI::CodexBridge::BuildReadOnlyAskCommand(settings,
		"Explain this Haiku project", ask, error));
	assert(ask.summary == "Ask Codex in read-only mode");
	assert(ask.argv[0] == "/bin/codex");
	assert(ask.argv[1] == "exec");
	assert(ask.argv[2] == "-C");
	assert(ask.argv[3] == root.string());
	assert(ask.argv[4] == "--sandbox");
	assert(ask.argv[5] == "read-only");
	assert(ask.argv[6] == "--ask-for-approval");
	assert(ask.argv[7] == "never");
	assert(std::find(ask.argv.begin(), ask.argv.end(), "--model")
		!= ask.argv.end());
	assert(ask.argv.back() == "Explain this Haiku project");
	assert(!ask.dangerous);
	assert(ask.runnable);
	assert(Haikode::AI::CommandDisplayString(ask).find("read-only")
		!= std::string::npos);

	settings.localProvider = "ollama";
	settings.useOss = true;
	assert(Haikode::AI::CodexBridge::BuildReadOnlyAskCommand(settings,
		"Use local model", ask, error));
	assert(std::find(ask.argv.begin(), ask.argv.end(), "--oss")
		!= ask.argv.end());
	assert(std::find(ask.argv.begin(), ask.argv.end(), "--local-provider")
		!= ask.argv.end());

	settings.projectRoot = root / "missing";
	assert(!Haikode::AI::CodexBridge::BuildReadOnlyAskCommand(settings,
		"Explain", ask, error));
	assert(error.find("root") != std::string::npos);

	fs::remove_all(root);
	std::cout << "codex-bridge-smoke-ok\n";
	return 0;
}
