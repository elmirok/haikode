/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "CodexBridge.h"

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <sstream>

namespace Haikode::AI {

namespace fs = std::filesystem;

namespace {

std::string
Lowercase(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(),
		[](unsigned char c) {
			return static_cast<char>(std::tolower(c));
		});
	return value;
}


bool
HasPathSeparator(const std::string& path)
{
	return path.find('/') != std::string::npos
		|| path.find('\\') != std::string::npos;
}


bool
LooksExecutable(const fs::path& path)
{
	std::error_code error;
	return fs::exists(path, error) && fs::is_regular_file(path, error);
}


void
AddCommonExecOptions(const CodexBridgeSettings& settings,
	std::vector<std::string>& argv)
{
	argv.push_back("exec");
	argv.push_back("-C");
	argv.push_back(settings.projectRoot);
	argv.push_back("--sandbox");
	argv.push_back("read-only");
	argv.push_back("--ask-for-approval");
	argv.push_back("never");
	if (!settings.model.empty()) {
		argv.push_back("--model");
		argv.push_back(settings.model);
	}
	if (settings.useOss)
		argv.push_back("--oss");
	if (!settings.localProvider.empty()) {
		argv.push_back("--local-provider");
		argv.push_back(settings.localProvider);
	}
	if (!settings.outputLastMessagePath.empty()) {
		argv.push_back("--output-last-message");
		argv.push_back(settings.outputLastMessagePath);
	}
}

} // namespace


std::string
CodexBridge::FindExecutable(const std::string& executableName,
	const std::string& pathEnv, std::string& error)
{
	error.clear();
	if (executableName.empty()) {
		error = "Codex executable name is empty.";
		return "";
	}

	if (HasPathSeparator(executableName)) {
		const fs::path path(executableName);
		if (LooksExecutable(path))
			return path.string();
		error = "Codex executable was not found: " + executableName;
		return "";
	}

	std::istringstream paths(pathEnv);
	std::string directory;
	while (std::getline(paths, directory, ':')) {
		if (directory.empty())
			continue;
		const fs::path candidate = fs::path(directory) / executableName;
		if (LooksExecutable(candidate))
			return candidate.string();
	}

	error = "Codex CLI was not found in PATH.";
	return "";
}


CodexLoginState
CodexBridge::ParseLoginStatus(const std::string& output)
{
	const std::string lower = Lowercase(output);
	if (lower.find("not logged in") != std::string::npos
		|| lower.find("logged out") != std::string::npos
		|| lower.find("not authenticated") != std::string::npos
		|| lower.find("auth is not configured") != std::string::npos) {
		return CodexLoginState::LoggedOut;
	}
	if (lower.find("logged in") != std::string::npos
		|| lower.find("auth is configured") != std::string::npos) {
		return CodexLoginState::LoggedIn;
	}
	if (lower.find("not found") != std::string::npos
		|| lower.find("no such file") != std::string::npos) {
		return CodexLoginState::Unavailable;
	}
	return CodexLoginState::Unknown;
}


const char*
CodexBridge::LoginStateLabel(CodexLoginState state)
{
	switch (state) {
		case CodexLoginState::Unavailable:
			return "Not found";
		case CodexLoginState::LoggedOut:
			return "Logged out";
		case CodexLoginState::LoggedIn:
			return "Logged in";
		case CodexLoginState::Unknown:
			return "Unknown";
	}
	return "Unknown";
}


CommandRequest
CodexBridge::LoginStatusCommand(const std::string& executable)
{
	CommandRequest command;
	command.summary = "Check Codex login status";
	command.argv = {executable.empty() ? "codex" : executable, "login", "status"};
	command.dangerous = false;
	command.runnable = true;
	return command;
}


CommandRequest
CodexBridge::DeviceLoginCommand(const std::string& executable)
{
	CommandRequest command;
	command.summary = "Start Codex device login";
	command.argv = {executable.empty() ? "codex" : executable, "login",
		"--device-auth"};
	command.dangerous = false;
	command.runnable = true;
	return command;
}


bool
CodexBridge::BuildReadOnlyAskCommand(const CodexBridgeSettings& settings,
	const std::string& prompt, CommandRequest& command, std::string& error)
{
	command = CommandRequest();
	error.clear();
	if (settings.executable.empty()) {
		error = "Codex executable is required.";
		return false;
	}
	if (settings.projectRoot.empty()) {
		error = "A project root is required for Codex.";
		return false;
	}
	if (prompt.empty()) {
		error = "A prompt is required for Codex.";
		return false;
	}

	std::error_code fsError;
	if (!fs::exists(settings.projectRoot, fsError)
		|| !fs::is_directory(settings.projectRoot, fsError)) {
		error = "Codex project root does not exist.";
		return false;
	}

	command.summary = "Ask Codex in read-only mode";
	command.argv.push_back(settings.executable);
	AddCommonExecOptions(settings, command.argv);
	command.argv.push_back(prompt);
	command.dangerous = false;
	command.runnable = true;
	command.warning = "Codex is launched read-only; Haikode still requires explicit approval before running this command.";
	return true;
}


bool
CodexBridge::IsReadOnlyAskCommand(const CommandRequest& command)
{
	if (command.argv.size() < 8)
		return false;
	if (command.argv[1] != "exec")
		return false;
	auto sandbox = std::find(command.argv.begin(), command.argv.end(),
		"--sandbox");
	if (sandbox == command.argv.end() || sandbox + 1 == command.argv.end()
		|| *(sandbox + 1) != "read-only") {
		return false;
	}
	auto approval = std::find(command.argv.begin(), command.argv.end(),
		"--ask-for-approval");
	return approval != command.argv.end() && approval + 1 != command.argv.end()
		&& *(approval + 1) == "never";
}

} // namespace Haikode::AI
