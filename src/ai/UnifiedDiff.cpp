/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "UnifiedDiff.h"

#include <algorithm>
#include <cctype>
#include <chrono>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <sstream>

namespace Haikode::AI {

namespace fs = std::filesystem;

namespace {

std::vector<std::string>
SplitLines(const std::string& text)
{
	std::vector<std::string> lines;
	std::string line;
	std::istringstream in(text);
	while (std::getline(in, line)) {
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.resize(line.size() - 1);
		lines.push_back(line);
	}
	return lines;
}

std::string
TrimPatchPath(std::string path)
{
	const size_t tab = path.find('\t');
	if (tab != std::string::npos)
		path.resize(tab);
	const size_t space = path.find(' ');
	if (space != std::string::npos)
		path.resize(space);
	if (path.rfind("a/", 0) == 0 || path.rfind("b/", 0) == 0)
		path.erase(0, 2);
	return path;
}

bool
IsSafeRelativePath(const std::string& path)
{
	if (path.empty() || path[0] == '/' || path == "/dev/null")
		return false;

	fs::path fsPath(path);
	for (const fs::path& part : fsPath) {
		if (part == "..")
			return false;
	}
	return true;
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

bool
ReadTextFile(const fs::path& path, std::vector<std::string>& lines,
	std::string& error)
{
	std::ifstream file(path, std::ios::binary);
	if (!file) {
		error = "Could not open file to patch.";
		return false;
	}
	std::string text((std::istreambuf_iterator<char>(file)),
		std::istreambuf_iterator<char>());
	if (text.find('\0') != std::string::npos) {
		error = "Refusing to patch binary file.";
		return false;
	}
	lines = SplitLines(text);
	return true;
}

bool
WriteTextFile(const fs::path& path, const std::vector<std::string>& lines,
	std::string& error)
{
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file) {
		error = "Could not write patched file.";
		return false;
	}
	for (const std::string& line : lines)
		file << line << "\n";
	return true;
}

std::string
Timestamp()
{
	const auto now = std::chrono::system_clock::now().time_since_epoch();
	return std::to_string(std::chrono::duration_cast<std::chrono::seconds>(now)
		.count());
}

std::string
PatchTimestamp()
{
	const auto now = std::chrono::system_clock::now().time_since_epoch();
	return std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(
		now).count());
}

bool
ApplyHunks(const std::vector<std::string>& original,
	const std::vector<PatchHunk>& hunks, std::vector<std::string>& patched,
	std::string& error)
{
	patched.clear();
	size_t cursor = 0;
	for (const PatchHunk& hunk : hunks) {
		const size_t target = hunk.oldStart <= 0
			? 0 : static_cast<size_t>(hunk.oldStart - 1);
		if (target < cursor || target > original.size()) {
			error = "Patch hunk points outside the current file.";
			return false;
		}

		while (cursor < target)
			patched.push_back(original[cursor++]);

		for (const PatchHunkLine& line : hunk.lines) {
			if (line.kind == ' ' || line.kind == '-') {
				if (cursor >= original.size() || original[cursor] != line.text) {
					error = "Patch context does not match the current file.";
					return false;
				}
				if (line.kind == ' ')
					patched.push_back(original[cursor]);
				++cursor;
			} else if (line.kind == '+') {
				patched.push_back(line.text);
			}
		}
	}

	while (cursor < original.size())
		patched.push_back(original[cursor++]);
	return true;
}

bool
BuildNewFileContents(const std::vector<PatchHunk>& hunks,
	std::vector<std::string>& contents, std::string& error)
{
	contents.clear();
	for (const PatchHunk& hunk : hunks) {
		for (const PatchHunkLine& line : hunk.lines) {
			if (line.kind == '-') {
				error = "New-file patch cannot delete existing lines.";
				return false;
			}
			if (line.kind == '+' || line.kind == ' ')
				contents.push_back(line.text);
		}
	}
	return true;
}

} // namespace

bool
UnifiedDiff::Parse(const std::string& text, UnifiedDiff& diff, std::string& error)
{
	diff.fFiles.clear();
	error.clear();

	PatchFile* currentFile = nullptr;
	PatchHunk* currentHunk = nullptr;
	const std::vector<std::string> lines = SplitLines(text);

	for (const std::string& line : lines) {
		if (line.rfind("--- ", 0) == 0) {
			diff.fFiles.push_back(PatchFile());
			currentFile = &diff.fFiles.back();
			currentFile->oldPath = TrimPatchPath(line.substr(4));
			currentHunk = nullptr;
			continue;
		}

		if (line.rfind("+++ ", 0) == 0 && currentFile != nullptr) {
			currentFile->newPath = TrimPatchPath(line.substr(4));
			continue;
		}

		if (line.rfind("@@ ", 0) == 0 && currentFile != nullptr) {
			currentFile->hunks.push_back(PatchHunk());
			currentHunk = &currentFile->hunks.back();
			if (!ParseHunkHeader(line, *currentHunk)) {
				error = "Invalid unified diff hunk header.";
				return false;
			}
			continue;
		}

		if (currentHunk != nullptr && !line.empty()) {
			const char kind = line[0];
			if (kind == ' ' || kind == '-' || kind == '+')
				currentHunk->lines.push_back({kind, line.substr(1)});
		}
	}

	diff.fFiles.erase(std::remove_if(diff.fFiles.begin(), diff.fFiles.end(),
		[](const PatchFile& file) {
			return file.newPath.empty() || file.hunks.empty();
		}), diff.fFiles.end());

	if (diff.fFiles.empty()) {
		error = "No unified diff found.";
		return false;
	}
	return true;
}


bool
UnifiedDiff::ExtractFromText(const std::string& text, UnifiedDiff& diff,
	std::string& rawDiff, std::string& error)
{
	const size_t diffPos = text.find("diff --git ");
	const size_t oldPos = text.find("--- ");
	size_t start = std::string::npos;
	if (diffPos != std::string::npos)
		start = diffPos;
	else if (oldPos != std::string::npos)
		start = oldPos;

	if (start == std::string::npos) {
		error = "No unified diff found.";
		return false;
	}

	rawDiff = text.substr(start);
	return Parse(rawDiff, diff, error);
}


bool
UnifiedDiff::SavePatchText(const std::string& projectRoot,
	const std::string& rawDiff, std::string& savedPath, std::string& error)
{
	savedPath.clear();
	error.clear();
	try {
		if (projectRoot.empty()) {
			error = "No active project root.";
			return false;
		}
		if (rawDiff.empty()) {
			error = "Cannot save an empty patch.";
			return false;
		}

		const fs::path root = fs::weakly_canonical(projectRoot);
		const fs::path patchesRoot = root / ".haikode" / "patches";
		fs::create_directories(patchesRoot);

		const fs::path patchPath = patchesRoot / ("patch-" + PatchTimestamp() + ".diff");
		if (!IsInsideDirectory(patchPath, root)) {
			error = "Unsafe patch save path.";
			return false;
		}

		std::ofstream file(patchPath, std::ios::binary | std::ios::trunc);
		if (!file) {
			error = "Could not save patch.";
			return false;
		}
		file << rawDiff;
		savedPath = patchPath.string();
		return true;
	} catch (const std::exception& exception) {
		error = exception.what();
		return false;
	}
}


bool
UnifiedDiff::ParseHunkHeader(const std::string& line, PatchHunk& hunk)
{
	size_t oldPos = line.find('-');
	size_t newPos = line.find('+', oldPos == std::string::npos ? 0 : oldPos);
	if (oldPos == std::string::npos || newPos == std::string::npos)
		return false;

	hunk.oldStart = std::atoi(line.c_str() + oldPos + 1);
	const char* oldComma = std::strchr(line.c_str() + oldPos, ',');
	hunk.oldCount = oldComma == nullptr ? 1 : std::atoi(oldComma + 1);

	hunk.newStart = std::atoi(line.c_str() + newPos + 1);
	const char* newComma = std::strchr(line.c_str() + newPos, ',');
	hunk.newCount = newComma == nullptr ? 1 : std::atoi(newComma + 1);
	return hunk.oldStart >= 0 && hunk.newStart >= 0;
}


std::vector<std::string>
UnifiedDiff::ChangedPaths() const
{
	std::vector<std::string> paths;
	paths.reserve(fFiles.size());
	for (const PatchFile& file : fFiles)
		paths.push_back(file.newPath);
	return paths;
}


std::vector<PatchFileStats>
UnifiedDiff::FileStats() const
{
	std::vector<PatchFileStats> stats;
	stats.reserve(fFiles.size());
	for (const PatchFile& file : fFiles) {
		PatchFileStats fileStats;
		fileStats.path = file.newPath;
		fileStats.hunkCount = file.hunks.size();
		fileStats.newFile = file.oldPath == "/dev/null";
		for (const PatchHunk& hunk : file.hunks) {
			for (const PatchHunkLine& line : hunk.lines) {
				if (line.kind == '+')
					fileStats.additions++;
				else if (line.kind == '-')
					fileStats.deletions++;
			}
		}
		stats.push_back(fileStats);
	}
	return stats;
}


int
UnifiedDiff::HunkCount() const
{
	int count = 0;
	for (const PatchFile& file : fFiles)
		count += static_cast<int>(file.hunks.size());
	return count;
}


bool
UnifiedDiff::Apply(const std::string& projectRoot, PatchApplyResult& result,
	std::string& error) const
{
	result = PatchApplyResult();
	error.clear();
	try {
		if (projectRoot.empty()) {
			error = "No active project root.";
			return false;
		}

		const fs::path root = fs::weakly_canonical(projectRoot);
		const fs::path backupRoot = root / ".haikode" / "backups" / Timestamp();

		for (const PatchFile& file : fFiles) {
			if (!IsSafeRelativePath(file.newPath)) {
				error = "Unsafe patch path: " + file.newPath;
				return false;
			}
			if (file.newPath == "/dev/null") {
				error = "File deletion patches are not enabled yet.";
				return false;
			}
		}

		for (const PatchFile& file : fFiles) {
			const fs::path target = file.oldPath == "/dev/null"
				? fs::weakly_canonical((root / file.newPath).parent_path())
					/ fs::path(file.newPath).filename()
				: fs::weakly_canonical(root / file.newPath);
			if (!IsInsideDirectory(target, root)) {
				error = "Unsafe patch path: " + file.newPath;
				return false;
			}

			std::vector<std::string> patched;
			if (file.oldPath == "/dev/null") {
				if (fs::exists(target)) {
					error = "New-file patch target already exists: " + file.newPath;
					return false;
				}
				if (!BuildNewFileContents(file.hunks, patched, error))
					return false;
			} else {
				std::vector<std::string> original;
				if (!ReadTextFile(target, original, error))
					return false;

				if (!ApplyHunks(original, file.hunks, patched, error))
					return false;

				const fs::path backup = backupRoot / file.newPath;
				fs::create_directories(backup.parent_path());
				fs::copy_file(target, backup, fs::copy_options::overwrite_existing);
			}
			fs::create_directories(target.parent_path());
			if (!WriteTextFile(target, patched, error))
				return false;

			result.changedFiles.push_back(file.newPath);
		}

		result.backupDirectory = backupRoot.string();
		return true;
	} catch (const std::exception& exception) {
		error = exception.what();
		return false;
	}
}

} // namespace Haikode::AI
