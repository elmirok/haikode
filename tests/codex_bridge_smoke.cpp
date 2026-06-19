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
	assert(Haikode::AI::CodexBridge::FormatLoginStatusSummary(
		"Logged in using ChatGPT\n") == "Codex status: Logged in");
	assert(Haikode::AI::CodexBridge::FormatLoginStatusSummary(
		"Not logged in") == "Codex status: Logged out");
	assert(Haikode::AI::CodexBridge::FormatLoginStatusSummary(
		"codex: not found") == "Codex status: Not found");
	assert(Haikode::AI::CodexBridge::FormatLoginStatusSummary(
		"unexpected output").find("Unknown") != std::string::npos);

	Haikode::AI::VibeCodingRequest codexRequest;
	codexRequest.projectRoot = root.string();
	codexRequest.userPrompt = "Explain the app startup path";
	codexRequest.defaultBuildCommand = "make";
	codexRequest.defaultTestCommand = "make test";
	codexRequest.files.push_back({
		"src/main.cpp",
		"int main() { return 0; }\n",
		false,
		"deac66ccb79f6d31c0fa7d358de48e083c15c02ff50ec1ebd4b64314b9e6e196"
	});
	codexRequest.projectFiles.push_back({"src/main.cpp", "C++", "source",
		"high", false, "Application entry point"});
	const Haikode::AI::PromptBuildResult codexPrompt
		= Haikode::AI::CodexBridge::BuildReadOnlyPrompt(codexRequest, 1024,
			10, 10);
	assert(codexPrompt.prompt.find("Codex CLI bridge") != std::string::npos);
	assert(codexPrompt.prompt.find("read-only sandbox") != std::string::npos);
	assert(codexPrompt.prompt.find("Do not write files") != std::string::npos);
	assert(codexPrompt.prompt.find("Explain the app startup path")
		!= std::string::npos);
	assert(codexPrompt.prompt.find("Project map:") != std::string::npos);
	assert(codexPrompt.prompt.find("src/main.cpp [C++] role=source risk=high")
		!= std::string::npos);
	assert(codexPrompt.prompt.find("Build: make") != std::string::npos);
	assert(codexPrompt.prompt.find("Test: make test") != std::string::npos);
	assert(codexPrompt.prompt.find("int main()") != std::string::npos);
	assert(codexPrompt.prompt.find("haikode-command") != std::string::npos);

	Haikode::AI::CommandRequest status
		= Haikode::AI::CodexBridge::LoginStatusCommand("/bin/codex");
	assert(status.argv.size() == 3);
	assert(status.argv[0] == "/bin/codex");
	assert(status.argv[1] == "login");
	assert(status.argv[2] == "status");
	assert(status.runnable);
	assert(Haikode::AI::CodexBridge::IsLoginStatusCommand(status));

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
	assert(Haikode::AI::CodexBridge::IsReadOnlyAskCommand(ask));
	assert(Haikode::AI::CommandDisplayString(ask).find("read-only")
		!= std::string::npos);
	assert(!Haikode::AI::CodexBridge::IsReadOnlyAskCommand(status));
	assert(!Haikode::AI::CodexBridge::IsLoginStatusCommand(ask));

	settings.localProvider = "ollama";
	settings.useOss = true;
	assert(Haikode::AI::CodexBridge::BuildReadOnlyAskCommand(settings,
		"Use local model", ask, error));
	assert(std::find(ask.argv.begin(), ask.argv.end(), "--oss")
		!= ask.argv.end());
	assert(std::find(ask.argv.begin(), ask.argv.end(), "--local-provider")
		!= ask.argv.end());

	settings.useOss = false;
	settings.localProvider = "";
	settings.outputLastMessagePath = (root / "codex-last.txt").string();
	assert(!Haikode::AI::CodexBridge::BuildReadOnlyAskCommand(settings,
		"Explain", ask, error));
	assert(error.find(".haikode") != std::string::npos);

	settings.outputLastMessagePath = (root.parent_path() / "codex-last.txt")
		.string();
	assert(!Haikode::AI::CodexBridge::BuildReadOnlyAskCommand(settings,
		"Explain", ask, error));
	assert(error.find(".haikode") != std::string::npos);

	settings.outputLastMessagePath = ".haikode/codex-last.txt";
	assert(Haikode::AI::CodexBridge::BuildReadOnlyAskCommand(settings,
		"Explain", ask, error));
	assert(std::find(ask.argv.begin(), ask.argv.end(), "--output-last-message")
		!= ask.argv.end());

	settings.projectRoot = root / "missing";
	assert(!Haikode::AI::CodexBridge::BuildReadOnlyAskCommand(settings,
		"Explain", ask, error));
	assert(error.find("root") != std::string::npos);

	fs::remove_all(root);
	std::cout << "codex-bridge-smoke-ok\n";
	return 0;
}
