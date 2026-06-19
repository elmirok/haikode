/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include <string>
#include <vector>

namespace Haikode::AI {

enum class PromptMode {
	Ask,
	ExplainSelection,
	ProposePatch,
	ReviewDiff
};

struct ContextFile {
	std::string path;
	std::string text;
	bool truncated = false;
};

struct VibeCodingRequest {
	PromptMode mode = PromptMode::Ask;
	std::string projectRoot;
	std::string userPrompt;
	std::vector<ContextFile> files;
};

struct PromptBuildResult {
	std::string prompt;
	std::vector<std::string> warnings;
};

std::string SelectContextText(const std::string& selection,
	const std::string& fullFileText);

class PromptBuilder {
public:
	PromptBuildResult Build(const VibeCodingRequest& request,
		size_t maxBytesPerFile, size_t maxFiles) const;

private:
	std::string ModeInstruction(PromptMode mode) const;
};

class SafetyPolicy {
public:
	bool AllowsAutomaticFileWrite() const { return false; }
	bool AllowsAutomaticCommandRun() const { return false; }
	bool RequiresPatchReview() const { return true; }
	bool RequiresCommandApproval() const { return true; }
};

} // namespace Haikode::AI
