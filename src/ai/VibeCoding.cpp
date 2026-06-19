/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "VibeCoding.h"

#include <algorithm>
#include <sstream>

namespace Haikode::AI {

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
		<< "must be proposed as unified diffs for explicit review.\n\n"
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
