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
	std::string sha256;
};

struct ProjectFileSummary {
	std::string path;
	std::string language;
	std::string role;
	std::string risk;
	bool hasTodo = false;
	std::string summary;
};

struct ProjectFileIndex {
	std::vector<ProjectFileSummary> files;
	size_t candidateCount = 0;
	std::string memoryPath;
};

struct ProjectMemory {
	std::string defaultBuildCommand;
	std::string defaultTestCommand;
	std::vector<ProjectFileSummary> files;
	size_t candidateCount = 0;
};

struct VibeCodingRequest {
	PromptMode mode = PromptMode::Ask;
	std::string projectRoot;
	std::string userPrompt;
	std::string defaultBuildCommand;
	std::string defaultTestCommand;
	std::vector<ContextFile> files;
	std::vector<ProjectFileSummary> projectFiles;
	std::vector<std::string> contextWarnings;
	size_t projectMapCandidateCount = 0;
	std::string pendingDiff;
	std::string pendingDiffPath;
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

struct ProjectRecordEntry {
	std::string type;
	std::string path;
	size_t sizeBytes = 0;
};

std::string SelectContextText(const std::string& selection,
	const std::string& fullFileText);
std::vector<std::string> ParseProjectContextPathList(
	const std::string& text, size_t maxPaths);
std::string Sha256HexForText(const std::string& text);
bool ExtractCommandRequests(const std::string& text,
	std::vector<CommandRequest>& commands, std::string& error);
bool SaveCommandRequests(const std::string& projectRoot,
	const std::vector<CommandRequest>& commands, std::string& savedPath,
	std::string& error);
std::string CommandDisplayString(const CommandRequest& command);
std::string CommandPrimaryActionLabel(const CommandRequest& command);
std::string FormatCommandApprovalPrompt(const CommandRequest& command,
	const std::string& projectRoot);
std::string FormatPendingActions(const PendingActionSummary& summary);
bool SaveAiSession(const std::string& projectRoot,
	const AiSessionRecord& session, std::string& savedPath,
	std::string& error);
std::vector<ProjectRecordEntry> ListProjectRecords(
	const std::string& projectRoot, size_t maxRecords);
bool ReadProjectRecord(const std::string& projectRoot,
	const std::string& recordPath, size_t maxBytes, std::string& text,
	std::string& error);
std::vector<ProjectFileSummary> BuildProjectMap(const std::string& projectRoot,
	size_t maxFiles, size_t* candidateCount = nullptr);
bool RefreshProjectFileIndex(const std::string& projectRoot, size_t maxFiles,
	ProjectFileIndex& index, std::string& error);
std::string FormatProjectFileIndexSummary(const ProjectFileIndex& index,
	size_t maxVisibleFiles);
bool SaveProjectMemory(const std::string& projectRoot,
	const std::vector<ProjectFileSummary>& files, size_t candidateCount,
	std::string& savedPath, std::string& error);
bool LoadProjectMemory(const std::string& projectRoot, size_t maxFiles,
	std::vector<ProjectFileSummary>& files, size_t& candidateCount,
	std::string& error);
bool LoadProjectMemory(const std::string& projectRoot, size_t maxFiles,
	ProjectMemory& memory, std::string& error);
bool InferProjectCommands(const std::string& projectRoot,
	ProjectMemory& memory, std::string& error);
bool LoadProjectContextFile(const std::string& projectRoot,
	const std::string& relativePath, size_t maxBytes, ContextFile& file,
	std::string& error);
bool NormalizeProjectContextPath(const std::string& projectRoot,
	const std::string& path, std::string& normalizedPath, std::string& error);

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
