/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include <string>
#include <vector>

namespace Haikode::AI {

struct PatchHunkLine {
	char kind = ' ';
	std::string text;
};

struct PatchHunk {
	int oldStart = 0;
	int oldCount = 0;
	int newStart = 0;
	int newCount = 0;
	std::vector<PatchHunkLine> lines;
};

struct PatchFile {
	std::string oldPath;
	std::string newPath;
	std::vector<PatchHunk> hunks;
};

struct PatchApplyResult {
	std::vector<std::string> changedFiles;
	std::string backupDirectory;
};

struct PatchFileStats {
	std::string path;
	size_t additions = 0;
	size_t deletions = 0;
	size_t hunkCount = 0;
	bool newFile = false;
};

class UnifiedDiff {
public:
	static bool Parse(const std::string& text, UnifiedDiff& diff,
		std::string& error);
	static bool ExtractFromText(const std::string& text, UnifiedDiff& diff,
		std::string& rawDiff, std::string& error);
	static bool SavePatchText(const std::string& projectRoot,
		const std::string& rawDiff, std::string& savedPath, std::string& error);

	const std::vector<PatchFile>& Files() const { return fFiles; }
	bool IsEmpty() const { return fFiles.empty(); }
	std::vector<std::string> ChangedPaths() const;
	std::vector<PatchFileStats> FileStats() const;
	int HunkCount() const;
	std::string ReviewText() const;

	bool Apply(const std::string& projectRoot, PatchApplyResult& result,
		std::string& error) const;
	bool ApplyFile(const std::string& projectRoot, const std::string& path,
		PatchApplyResult& result, std::string& error) const;
	bool RemoveFile(const std::string& path);
	bool RemoveFirstFile(std::string* removedPath = nullptr);

private:
	static bool ParseHunkHeader(const std::string& line, PatchHunk& hunk);

	std::vector<PatchFile> fFiles;
};

} // namespace Haikode::AI
