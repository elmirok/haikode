/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "ai/UnifiedDiff.h"

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

static std::string
ReadFile(const fs::path& path)
{
	std::ifstream file(path, std::ios::binary);
	return std::string(std::istreambuf_iterator<char>(file),
		std::istreambuf_iterator<char>());
}

int
main()
{
	const fs::path root = fs::temp_directory_path() / "haikode-patch-smoke";
	fs::remove_all(root);
	fs::create_directories(root / "src");
	WriteFile(root / "src" / "main.cpp", "one\nold\nthree\n");

	const std::string diff =
		"diff --git a/src/main.cpp b/src/main.cpp\n"
		"--- a/src/main.cpp\n"
		"+++ b/src/main.cpp\n"
		"@@ -1,3 +1,3 @@\n"
		" one\n"
		"-old\n"
		"+new\n"
		" three\n";

	Haikode::AI::UnifiedDiff parsed;
	std::string error;
	assert(Haikode::AI::UnifiedDiff::Parse(diff, parsed, error));
	assert(parsed.Files().size() == 1);
	assert(parsed.Files()[0].newPath == "src/main.cpp");
	assert(parsed.ChangedPaths().size() == 1);
	assert(parsed.ChangedPaths()[0] == "src/main.cpp");
	assert(parsed.HunkCount() == 1);
	const std::vector<Haikode::AI::PatchFileStats> stats
		= parsed.FileStats();
	assert(stats.size() == 1);
	assert(stats[0].path == "src/main.cpp");
	assert(stats[0].additions == 1);
	assert(stats[0].deletions == 1);
	assert(stats[0].hunkCount == 1);
	assert(!stats[0].newFile);
	const std::string preview = parsed.ReviewText();
	assert(preview.find("Patch preview: 1 file(s), 1 hunk(s), +1 -1")
		!= std::string::npos);
	assert(preview.find("src/main.cpp (+1 -1, 1 hunk(s))")
		!= std::string::npos);
	assert(preview.find("@@ -1,3 +1,3 @@") != std::string::npos);
	assert(preview.find("-old") != std::string::npos);
	assert(preview.find("+new") != std::string::npos);
	assert(parsed.ReviewTextForFile("src/main.cpp").find(
		"src/main.cpp (+1 -1, 1 hunk(s))") != std::string::npos);
	assert(parsed.ReviewTextForFile("missing.cpp").empty());

	Haikode::AI::PatchApplyResult result;
	assert(parsed.Apply(root.string(), result, error));
	assert(ReadFile(root / "src" / "main.cpp") == "one\nnew\nthree\n");
	assert(!result.backupDirectory.empty());
	assert(fs::exists(result.backupDirectory));
	assert(ReadFile(fs::path(result.backupDirectory) / "src" / "main.cpp")
		== "one\nold\nthree\n");

	WriteFile(root / "src" / "main.cpp", "one\nold\nthree\n");
	WriteFile(root / "src" / "other.cpp", "alpha\nbeta\n");
	const std::string multiDiff =
		diff
		+ "diff --git a/src/other.cpp b/src/other.cpp\n"
		+ "--- a/src/other.cpp\n"
		+ "+++ b/src/other.cpp\n"
		+ "@@ -1,2 +1,2 @@\n"
		+ " alpha\n"
		+ "-beta\n"
		+ "+gamma\n";
	Haikode::AI::UnifiedDiff multiPatch;
	assert(Haikode::AI::UnifiedDiff::Parse(multiDiff, multiPatch, error));
	assert(multiPatch.ChangedPaths().size() == 2);
	const std::string otherPreview = multiPatch.ReviewTextForFile("src/other.cpp");
	assert(otherPreview.find("Patch preview: 1 file(s), 1 hunk(s), +1 -1")
		!= std::string::npos);
	assert(otherPreview.find("src/other.cpp (+1 -1, 1 hunk(s))")
		!= std::string::npos);
	assert(otherPreview.find("+gamma") != std::string::npos);
	assert(otherPreview.find("src/main.cpp") == std::string::npos);
	assert(multiPatch.ApplyFile(root.string(), "src/main.cpp", result, error));
	assert(ReadFile(root / "src" / "main.cpp") == "one\nnew\nthree\n");
	assert(ReadFile(root / "src" / "other.cpp") == "alpha\nbeta\n");
	assert(result.changedFiles.size() == 1);
	assert(result.changedFiles[0] == "src/main.cpp");
	assert(multiPatch.RemoveFile("src/main.cpp"));
	assert(multiPatch.ChangedPaths().size() == 1);
	assert(multiPatch.ChangedPaths()[0] == "src/other.cpp");
	assert(!multiPatch.RemoveFile("missing.cpp"));
	std::string removedPath;
	assert(multiPatch.RemoveFirstFile(&removedPath));
	assert(removedPath == "src/other.cpp");
	assert(multiPatch.IsEmpty());
	assert(!multiPatch.RemoveFirstFile(&removedPath));

	WriteFile(root / "src" / "main.cpp", "one\nold\nthree\nkeep\nlast\n");
	const std::string hunkDiff =
		"diff --git a/src/main.cpp b/src/main.cpp\n"
		"--- a/src/main.cpp\n"
		"+++ b/src/main.cpp\n"
		"@@ -1,3 +1,3 @@\n"
		" one\n"
		"-old\n"
		"+new\n"
		" three\n"
		"@@ -3,3 +3,3 @@\n"
		" three\n"
		"-keep\n"
		"+changed\n"
		" last\n";
	Haikode::AI::UnifiedDiff hunkPatch;
	assert(Haikode::AI::UnifiedDiff::Parse(hunkDiff, hunkPatch, error));
	assert(hunkPatch.HunkCountForFile("src/main.cpp") == 2);
	assert(hunkPatch.ReviewTextForHunk("src/main.cpp", 1).find("+changed")
		!= std::string::npos);
	assert(hunkPatch.ReviewTextForHunk("src/main.cpp", 3).empty());
	assert(hunkPatch.ApplyHunk(root.string(), "src/main.cpp", 0, result,
		error));
	assert(ReadFile(root / "src" / "main.cpp")
		== "one\nnew\nthree\nkeep\nlast\n");
	assert(result.changedFiles.size() == 1);
	assert(result.changedFiles[0] == "src/main.cpp");
	assert(hunkPatch.RemoveHunk("src/main.cpp", 0));
	assert(hunkPatch.HunkCountForFile("src/main.cpp") == 1);
	assert(hunkPatch.ApplyHunk(root.string(), "src/main.cpp", 0, result,
		error));
	assert(ReadFile(root / "src" / "main.cpp")
		== "one\nnew\nthree\nchanged\nlast\n");
	assert(hunkPatch.RemoveHunk("src/main.cpp", 0));
	assert(hunkPatch.IsEmpty());

	std::string savedPatchPath;
	assert(Haikode::AI::UnifiedDiff::SavePatchText(root.string(), diff,
		savedPatchPath, error));
	assert(savedPatchPath.find(".haikode/patches/patch-") != std::string::npos);
	assert(ReadFile(savedPatchPath) == diff);
	assert(!Haikode::AI::UnifiedDiff::SavePatchText(root.string(), "",
		savedPatchPath, error));
	assert(error.find("empty") != std::string::npos);

	const std::string newFileDiff =
		"diff --git a/src/new_feature.cpp b/src/new_feature.cpp\n"
		"new file mode 100644\n"
		"--- /dev/null\n"
		"+++ b/src/new_feature.cpp\n"
		"@@ -0,0 +1,4 @@\n"
		"+#include <iostream>\n"
		"+int answer() {\n"
		"+\treturn 42;\n"
		"+}\n";
	Haikode::AI::UnifiedDiff newFilePatch;
	assert(Haikode::AI::UnifiedDiff::Parse(newFileDiff, newFilePatch, error));
	const std::vector<Haikode::AI::PatchFileStats> newFileStats
		= newFilePatch.FileStats();
	assert(newFileStats.size() == 1);
	assert(newFileStats[0].path == "src/new_feature.cpp");
	assert(newFileStats[0].additions == 4);
	assert(newFileStats[0].deletions == 0);
	assert(newFileStats[0].newFile);
	assert(newFilePatch.ReviewText().find(
		"src/new_feature.cpp (+4 -0, 1 hunk(s), new file)")
		!= std::string::npos);
	assert(newFilePatch.Apply(root.string(), result, error));
	assert(ReadFile(root / "src" / "new_feature.cpp")
		== "#include <iostream>\nint answer() {\n\treturn 42;\n}\n");

	Haikode::AI::UnifiedDiff duplicateNewFilePatch;
	assert(Haikode::AI::UnifiedDiff::Parse(newFileDiff, duplicateNewFilePatch,
		error));
	assert(!duplicateNewFilePatch.Apply(root.string(), result, error));
	assert(error.find("already exists") != std::string::npos);

	const std::string wrappedResponse =
		"Here is the patch:\n\n```diff\n" + diff + "```\n";
	Haikode::AI::UnifiedDiff wrapped;
	std::string rawDiff;
	assert(Haikode::AI::UnifiedDiff::ExtractFromText(wrappedResponse, wrapped,
		rawDiff, error));
	assert(wrapped.Files().size() == 1);
	assert(rawDiff.find("diff --git") == 0);

	const std::string unsafeDiff =
		"diff --git a/../outside.cpp b/../outside.cpp\n"
		"--- a/../outside.cpp\n"
		"+++ b/../outside.cpp\n"
		"@@ -1 +1 @@\n"
		"-old\n"
		"+new\n";
	Haikode::AI::UnifiedDiff unsafe;
	assert(Haikode::AI::UnifiedDiff::Parse(unsafeDiff, unsafe, error));
	assert(!unsafe.Apply(root.string(), result, error));
	assert(error.find("Unsafe") != std::string::npos);

	fs::remove_all(root);
	std::cout << "ai-patch-smoke-ok\n";
	return 0;
}
