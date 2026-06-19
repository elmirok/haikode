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
	SummarizeProject,
	ProposePatch,
	ReviewDiff
};

struct ContextFile {
	std::string path;
	std::string text;
	bool truncated = false;
};

struct ProjectFileSummary {
	std::string path;
	std::string language;
	std::string role;
	std::string risk;
	bool hasTodo = false;
	std::string summary;
};

struct VibeCodingRequest {
	PromptMode mode = PromptMode::Ask;
	std::string projectRoot;
	std::string userPrompt;
	std::vector<ContextFile> files;
	std::vector<ProjectFileSummary> projectFiles;
	size_t projectMapCandidateCount = 0;
	std::string pendingDiff;
};

struct PromptBuildResult {
	std::string prompt;
	std::vector<std::string> warnings;
};

struct CommandRequest {
	std::string summary;
	std::vector<std::string> argv;
	bool dangerous = false;
	bool runnable = true;
	std::string warning;
};

struct PendingActionSummary {
	struct PatchFile {
		std::string path;
		size_t additions = 0;
		size_t deletions = 0;
		size_t hunkCount = 0;
		bool newFile = false;
	};

	std::vector<PatchFile> patchFiles;
	std::vector<std::string> changedPaths;
	size_t hunkCount = 0;
	std::vector<CommandRequest> commands;
};

struct AiSessionRecord {
	std::string userPrompt;
	std::string providerBaseUrl;
	std::string providerModel;
	std::string authMode;
	std::string activeFile;
	std::string responseText;
	std::string pendingActions;
	std::string savedPatchPath;
};

std::string SelectContextText(const std::string& selection,
	const std::string& fullFileText);
bool ExtractCommandRequests(const std::string& text,
	std::vector<CommandRequest>& commands, std::string& error);
bool SaveCommandRequests(const std::string& projectRoot,
	const std::vector<CommandRequest>& commands, std::string& savedPath,
	std::string& error);
std::string CommandDisplayString(const CommandRequest& command);
std::string FormatPendingActions(const PendingActionSummary& summary);
bool SaveAiSession(const std::string& projectRoot,
	const AiSessionRecord& session, std::string& savedPath,
	std::string& error);
std::vector<ProjectFileSummary> BuildProjectMap(const std::string& projectRoot,
	size_t maxFiles, size_t* candidateCount = nullptr);

class PromptBuilder {
public:
	PromptBuildResult Build(const VibeCodingRequest& request,
		size_t maxBytesPerFile, size_t maxFiles,
		size_t maxProjectFiles = 50) const;

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
