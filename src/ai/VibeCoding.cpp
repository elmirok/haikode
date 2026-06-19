/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "VibeCoding.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <exception>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace Haikode::AI {

namespace fs = std::filesystem;

namespace {

std::string
Timestamp()
{
	const auto now = std::chrono::system_clock::now().time_since_epoch();
	return std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(
		now).count());
}


std::string
EscapeJson(const std::string& value)
{
	std::string escaped;
	for (const char c : value) {
		switch (c) {
			case '\\':
				escaped += "\\\\";
				break;
			case '"':
				escaped += "\\\"";
				break;
			case '\n':
				escaped += "\\n";
				break;
			case '\t':
				escaped += "\\t";
				break;
			default:
				escaped.push_back(c);
				break;
		}
	}
	return escaped;
}


bool
IsInsideDirectory(const fs::path& child, const fs::path& parent)
{
	const fs::path relative = fs::relative(child, parent);
	for (const fs::path& part : relative) {
		if (part == "..")
			return false;
	}
	return !relative.empty();
}


std::string
Trim(const std::string& value)
{
	size_t start = 0;
	while (start < value.size()
		&& std::isspace(static_cast<unsigned char>(value[start]))) {
		start++;
	}
	size_t end = value.size();
	while (end > start
		&& std::isspace(static_cast<unsigned char>(value[end - 1]))) {
		end--;
	}
	return value.substr(start, end - start);
}


bool
ReadJsonString(const std::string& text, size_t& pos, std::string& value)
{
	value.clear();
	if (pos >= text.size() || text[pos] != '"')
		return false;
	pos++;
	while (pos < text.size()) {
		const char c = text[pos++];
		if (c == '"')
			return true;
		if (c == '\\' && pos < text.size()) {
			const char escaped = text[pos++];
			switch (escaped) {
				case 'n':
					value.push_back('\n');
					break;
				case 't':
					value.push_back('\t');
					break;
				default:
					value.push_back(escaped);
					break;
			}
		} else {
			value.push_back(c);
		}
	}
	return false;
}


bool
ExtractJsonStringField(const std::string& json, const std::string& key,
	std::string& value)
{
	const std::string marker = "\"" + key + "\"";
	size_t pos = json.find(marker);
	if (pos == std::string::npos)
		return false;
	pos = json.find(':', pos + marker.size());
	if (pos == std::string::npos)
		return false;
	pos++;
	while (pos < json.size()
		&& std::isspace(static_cast<unsigned char>(json[pos]))) {
		pos++;
	}
	return ReadJsonString(json, pos, value);
}


bool
ExtractJsonStringArrayField(const std::string& json, const std::string& key,
	std::vector<std::string>& values)
{
	values.clear();
	const std::string marker = "\"" + key + "\"";
	size_t pos = json.find(marker);
	if (pos == std::string::npos)
		return false;
	pos = json.find(':', pos + marker.size());
	if (pos == std::string::npos)
		return false;
	pos++;
	while (pos < json.size()
		&& std::isspace(static_cast<unsigned char>(json[pos]))) {
		pos++;
	}
	if (pos >= json.size() || json[pos] != '[')
		return false;
	pos++;
	while (pos < json.size()) {
		while (pos < json.size()
			&& std::isspace(static_cast<unsigned char>(json[pos]))) {
			pos++;
		}
		if (pos < json.size() && json[pos] == ']')
			return true;

		std::string value;
		if (!ReadJsonString(json, pos, value))
			return false;
		values.push_back(value);

		while (pos < json.size()
			&& std::isspace(static_cast<unsigned char>(json[pos]))) {
			pos++;
		}
		if (pos < json.size() && json[pos] == ',') {
			pos++;
			continue;
		}
		if (pos < json.size() && json[pos] == ']')
			return true;
		return false;
	}
	return false;
}


std::string
JoinedArgv(const std::vector<std::string>& argv)
{
	std::string command;
	for (const std::string& arg : argv) {
		if (!command.empty())
			command += " ";
		command += arg;
	}
	return command;
}


bool
NeedsShellQuotes(const std::string& value)
{
	if (value.empty())
		return true;
	for (const char c : value) {
		if (std::isspace(static_cast<unsigned char>(c)) || c == '\''
			|| c == '"' || c == '\\' || c == '|' || c == '&' || c == ';'
			|| c == '<' || c == '>' || c == '$' || c == '`') {
			return true;
		}
	}
	return false;
}


std::string
ShellQuote(const std::string& value)
{
	if (!NeedsShellQuotes(value))
		return value;

	std::string quoted("'");
	for (const char c : value) {
		if (c == '\'')
			quoted += "'\\''";
		else
			quoted.push_back(c);
	}
	quoted += "'";
	return quoted;
}


void
ClassifyCommand(CommandRequest& command)
{
	const std::string joined = JoinedArgv(command.argv);
	if (joined.find("rm -rf") != std::string::npos) {
		command.dangerous = true;
		command.warning = "Dangerous command pattern: rm -rf";
		return;
	}
	if (joined.find("sudo") != std::string::npos
		|| joined.find(" dd ") != std::string::npos
		|| joined.find("mkfs") != std::string::npos) {
		command.dangerous = true;
		command.warning = "Dangerous privileged or destructive command.";
		return;
	}
	if (joined.find("curl") != std::string::npos
		&& joined.find("|") != std::string::npos
		&& joined.find("sh") != std::string::npos) {
		command.dangerous = true;
		command.warning = "Dangerous command pattern: network download pipe to shell";
		return;
	}
	if (joined.find("wget") != std::string::npos
		&& joined.find("|") != std::string::npos
		&& joined.find("sh") != std::string::npos) {
		command.dangerous = true;
		command.warning = "Dangerous command pattern: network download pipe to shell";
		return;
	}
	if (joined.find("chmod -R 777") != std::string::npos) {
		command.dangerous = true;
		command.warning = "Dangerous command pattern: chmod -R 777";
		return;
	}
}

} // namespace

std::string
SelectContextText(const std::string& selection, const std::string& fullFileText)
{
	if (!selection.empty())
		return selection;
	return fullFileText;
}


bool
ExtractCommandRequests(const std::string& text,
	std::vector<CommandRequest>& commands, std::string& error)
{
	commands.clear();
	error.clear();
	size_t pos = 0;
	while (true) {
		const size_t fence = text.find("```haikode-command", pos);
		if (fence == std::string::npos)
			return true;
		const size_t bodyStart = text.find('\n', fence);
		if (bodyStart == std::string::npos) {
			error = "Command request fence has no body.";
			return false;
		}
		const size_t bodyEnd = text.find("```", bodyStart + 1);
		if (bodyEnd == std::string::npos) {
			error = "Command request fence is not closed.";
			return false;
		}

		const std::string json = Trim(text.substr(bodyStart + 1,
			bodyEnd - bodyStart - 1));
		CommandRequest command;
		ExtractJsonStringField(json, "summary", command.summary);
		if (!ExtractJsonStringArrayField(json, "argv", command.argv)
			|| command.argv.empty()) {
			error = "Command request argv must be a non-empty JSON string array.";
			return false;
		}
		ClassifyCommand(command);
		commands.push_back(command);
		pos = bodyEnd + 3;
	}
}


std::string
CommandDisplayString(const CommandRequest& command)
{
	std::string display;
	for (const std::string& arg : command.argv) {
		if (!display.empty())
			display += " ";
		display += ShellQuote(arg);
	}
	return display;
}


bool
SaveCommandRequests(const std::string& projectRoot,
	const std::vector<CommandRequest>& commands, std::string& savedPath,
	std::string& error)
{
	savedPath.clear();
	error.clear();
	try {
		if (projectRoot.empty()) {
			error = "No active project root.";
			return false;
		}
		if (commands.empty()) {
			error = "No command requests to save.";
			return false;
		}

		const fs::path root = fs::weakly_canonical(projectRoot);
		const fs::path commandsRoot = root / ".haikode" / "commands";
		fs::create_directories(commandsRoot);
		const fs::path commandPath = commandsRoot
			/ ("command-" + Timestamp() + ".json");
		if (!IsInsideDirectory(commandPath, root)) {
			error = "Unsafe command save path.";
			return false;
		}

		std::ofstream file(commandPath, std::ios::binary | std::ios::trunc);
		if (!file) {
			error = "Could not save command requests.";
			return false;
		}
		file << "{\n  \"commands\":[";
		for (size_t i = 0; i < commands.size(); i++) {
			const CommandRequest& command = commands[i];
			if (i > 0)
				file << ",";
			file << "\n    {\"summary\":\"" << EscapeJson(command.summary)
				<< "\",\"argv\":[";
			for (size_t arg = 0; arg < command.argv.size(); arg++) {
				if (arg > 0)
					file << ",";
				file << "\"" << EscapeJson(command.argv[arg]) << "\"";
			}
			file << "],\"dangerous\":"
				<< (command.dangerous ? "true" : "false")
				<< ",\"warning\":\"" << EscapeJson(command.warning) << "\"}";
		}
		file << "\n  ]\n}\n";
		savedPath = commandPath.string();
		return true;
	} catch (const std::exception& exception) {
		error = exception.what();
		return false;
	}
}


std::string
PromptBuilder::ModeInstruction(PromptMode mode) const
{
	switch (mode) {
		case PromptMode::Ask:
			return "Answer the user's question about this Haiku project.";
		case PromptMode::ExplainSelection:
			return "Explain the selected code and mention Haiku API details when relevant.";
		case PromptMode::ProposePatch:
			return "Propose a small unified diff. Do not include shell commands.";
		case PromptMode::ReviewDiff:
			return "Review the diff for correctness, safety, and Haiku-native style.";
	}
	return "Answer the user's question about this Haiku project.";
}


PromptBuildResult
PromptBuilder::Build(const VibeCodingRequest& request, size_t maxBytesPerFile,
	size_t maxFiles) const
{
	PromptBuildResult result;
	std::ostringstream prompt;

	prompt
		<< "You are Haikode, a native Haiku OS AI coding assistant built as a "
		<< "Genio IDE fork.\n"
		<< "Prefer BeAPI and native C++ patterns. Do not suggest Electron or "
		<< "webview rewrites.\n"
		<< "Never assume permission to run commands or write files. File edits "
		<< "must be proposed as unified diffs for explicit review. If a command "
		<< "would help, propose it in a fenced haikode-command JSON block with "
		<< "summary and argv fields; Haikode will not run it automatically.\n\n"
		<< "Project root: " << request.projectRoot << "\n"
		<< "Mode: " << ModeInstruction(request.mode) << "\n\n"
		<< "User request:\n" << request.userPrompt << "\n\n";

	const size_t fileCount = std::min(maxFiles, request.files.size());
	if (request.files.size() > maxFiles) {
		result.warnings.push_back("Some files were omitted from AI context.");
	}

	for (size_t i = 0; i < fileCount; ++i) {
		const ContextFile& file = request.files[i];
		std::string text = file.text;
		if (text.size() > maxBytesPerFile) {
			text.resize(maxBytesPerFile);
			result.warnings.push_back(file.path + " was truncated for AI context.");
		}

		prompt
			<< "File: " << file.path << "\n"
			<< "```text\n"
			<< text
			<< "\n```\n\n";
	}

	result.prompt = prompt.str();
	return result;
}

} // namespace Haikode::AI
