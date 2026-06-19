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
	const std::vector<Haikode::AI::PatchReviewRow> reviewRows
		= parsed.ReviewRowsForFile("src/main.cpp");
	assert(reviewRows.size() == 6);
	assert(reviewRows[0].kind == Haikode::AI::PatchReviewRowKind::File);
	assert(reviewRows[0].path == "src/main.cpp");
	assert(reviewRows[0].text == "src/main.cpp (+1 -1, 1 hunk(s))");
	assert(reviewRows[1].kind == Haikode::AI::PatchReviewRowKind::Hunk);
	assert(reviewRows[1].text == "@@ -1,3 +1,3 @@");
	assert(reviewRows[2].kind == Haikode::AI::PatchReviewRowKind::Context);
	assert(reviewRows[2].oldLine == 1);
	assert(reviewRows[2].newLine == 1);
	assert(reviewRows[2].text == "one");
	assert(reviewRows[3].kind == Haikode::AI::PatchReviewRowKind::Removal);
	assert(reviewRows[3].oldLine == 2);
	assert(reviewRows[3].newLine == 0);
	assert(reviewRows[3].text == "old");
	assert(reviewRows[4].kind == Haikode::AI::PatchReviewRowKind::Addition);
	assert(reviewRows[4].oldLine == 0);
	assert(reviewRows[4].newLine == 2);
	assert(reviewRows[4].text == "new");
	assert(reviewRows[5].kind == Haikode::AI::PatchReviewRowKind::Context);
	assert(reviewRows[5].oldLine == 3);
	assert(reviewRows[5].newLine == 3);
	assert(parsed.ReviewRowsForFile("missing.cpp").empty());

	Haikode::AI::PatchApplyResult result;
	assert(parsed.Apply(root.string(), result, error));
	assert(ReadFile(root / "src" / "main.cpp") == "one\nnew\nthree\n");
	assert(!result.backupDirectory.empty());
	assert(fs::exists(result.backupDirectory));
	assert(ReadFile(fs::path(result.backupDirectory) / "src" / "main.cpp")
		== "one\nold\nthree\n");
	const std::string firstBackupDirectory = result.backupDirectory;
	WriteFile(root / "src" / "main.cpp", "one\nold\nthree\n");
	assert(parsed.Apply(root.string(), result, error));
	assert(!result.backupDirectory.empty());
	assert(result.backupDirectory != firstBackupDirectory);
	assert(ReadFile(fs::path(firstBackupDirectory) / "src" / "main.cpp")
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

	const std::string fencedDiffResponse =
		"Here is a small patch:\n"
		"```diff\n"
		"--- a/src/main.cpp\n"
		"+++ b/src/main.cpp\n"
		"@@ -1,3 +1,3 @@\n"
		" one\n"
		"-old\n"
		"+new\n"
		" three\n"
		"```\n"
		"After applying it, review the build manually.\n";
	Haikode::AI::UnifiedDiff fencedPatch;
	std::string fencedRawDiff;
	assert(Haikode::AI::UnifiedDiff::ExtractFromText(fencedDiffResponse,
		fencedPatch, fencedRawDiff, error));
	assert(fencedPatch.ChangedPaths().size() == 1);
	assert(fencedPatch.ChangedPaths()[0] == "src/main.cpp");
	assert(fencedRawDiff.find("```") == std::string::npos);
	assert(fencedRawDiff.find("After applying") == std::string::npos);
	assert(fencedRawDiff.find("--- a/src/main.cpp") == 0);
	assert(fencedRawDiff.find("+new") != std::string::npos);

	const std::string jsonPatchResponse =
		"{\"summary\":\"Small replacement\","
		"\"unified_diff\":\"--- a/src/main.cpp\\n"
		"+++ b/src/main.cpp\\n"
		"@@ -1,3 +1,3 @@\\n"
		" one\\n"
		"-old\\n"
		"+new\\n"
		" three\\n\"}";
	Haikode::AI::UnifiedDiff jsonPatch;
	std::string jsonRawDiff;
	assert(Haikode::AI::UnifiedDiff::ExtractFromText(jsonPatchResponse,
		jsonPatch, jsonRawDiff, error));
	assert(jsonPatch.ChangedPaths().size() == 1);
	assert(jsonPatch.ChangedPaths()[0] == "src/main.cpp");
	assert(jsonRawDiff.find("--- a/src/main.cpp") == 0);
	assert(jsonRawDiff.find("\\n") == std::string::npos);
	assert(jsonRawDiff.find("+new") != std::string::npos);

	const std::string jsonPatchArrayResponse =
		"{\"patches\":["
		"{\"summary\":\"Main replacement\","
		"\"unified_diff\":\"--- a/src/main.cpp\\n"
		"+++ b/src/main.cpp\\n"
		"@@ -1,3 +1,3 @@\\n"
		" one\\n-old\\n+new\\n three\\n\"},"
		"{\"summary\":\"Other replacement\","
		"\"diff\":\"--- a/src/other.cpp\\n"
		"+++ b/src/other.cpp\\n"
		"@@ -1,2 +1,2 @@\\n"
		" alpha\\n-beta\\n+gamma\\n\"}"
		"]}";
	Haikode::AI::UnifiedDiff jsonPatchArray;
	std::string jsonArrayRawDiff;
	assert(Haikode::AI::UnifiedDiff::ExtractFromText(jsonPatchArrayResponse,
		jsonPatchArray, jsonArrayRawDiff, error));
	assert(jsonPatchArray.ChangedPaths().size() == 2);
	assert(jsonPatchArray.ChangedPaths()[0] == "src/main.cpp");
	assert(jsonPatchArray.ChangedPaths()[1] == "src/other.cpp");
	assert(jsonArrayRawDiff.find("--- a/src/main.cpp") == 0);
	assert(jsonArrayRawDiff.find("--- a/src/other.cpp") != std::string::npos);
	assert(jsonArrayRawDiff.find("\\n") == std::string::npos);

	const std::string fencedJsonPatchResponse =
		"Here is the structured patch:\n"
		"```json\n"
		"{\"patches\":[{\"summary\":\"Main replacement\","
		"\"unified_diff\":\"--- a/src/main.cpp\\n"
		"+++ b/src/main.cpp\\n"
		"@@ -1,3 +1,3 @@\\n"
		" one\\n-old\\n+new\\n three\\n\"}]}\n"
		"```\n"
		"Review it before applying.\n";
	Haikode::AI::UnifiedDiff fencedJsonPatch;
	std::string fencedJsonRawDiff;
	assert(Haikode::AI::UnifiedDiff::ExtractFromText(fencedJsonPatchResponse,
		fencedJsonPatch, fencedJsonRawDiff, error));
	assert(fencedJsonPatch.ChangedPaths().size() == 1);
	assert(fencedJsonPatch.ChangedPaths()[0] == "src/main.cpp");
	assert(fencedJsonRawDiff.find("--- a/src/main.cpp") == 0);
	assert(fencedJsonRawDiff.find("```") == std::string::npos);
	assert(fencedJsonRawDiff.find("\\n") == std::string::npos);

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
	assert(result.backupDirectory.empty());

	Haikode::AI::UnifiedDiff duplicateNewFilePatch;
	assert(Haikode::AI::UnifiedDiff::Parse(newFileDiff, duplicateNewFilePatch,
		error));
	assert(!duplicateNewFilePatch.Apply(root.string(), result, error));
	assert(error.find("already exists") != std::string::npos);

	WriteFile(root / "src" / "real.cpp", "one\nold\nthree\n");
	const fs::path linkPath = root / "src" / "link.cpp";
	std::error_code symlinkError;
	fs::remove(linkPath, symlinkError);
	symlinkError.clear();
	fs::create_symlink(root / "src" / "real.cpp", linkPath, symlinkError);
	if (!symlinkError) {
		const std::string symlinkDiff =
			"diff --git a/src/link.cpp b/src/link.cpp\n"
			"--- a/src/link.cpp\n"
			"+++ b/src/link.cpp\n"
			"@@ -1,3 +1,3 @@\n"
			" one\n"
			"-old\n"
			"+new\n"
			" three\n";
		Haikode::AI::UnifiedDiff symlinkPatch;
		assert(Haikode::AI::UnifiedDiff::Parse(symlinkDiff, symlinkPatch,
			error));
		assert(!symlinkPatch.Apply(root.string(), result, error));
		assert(error.find("symbolic link") != std::string::npos);
		assert(ReadFile(root / "src" / "real.cpp") == "one\nold\nthree\n");
		assert(!symlinkPatch.ApplyHunk(root.string(), "src/link.cpp", 0,
			result, error));
		assert(error.find("symbolic link") != std::string::npos);
	}

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

	const std::string metadataDiff =
		"diff --git a/.haikode/project.json b/.haikode/project.json\n"
		"--- a/.haikode/project.json\n"
		"+++ b/.haikode/project.json\n"
		"@@ -1 +1 @@\n"
		"-old\n"
		"+new\n";
	Haikode::AI::UnifiedDiff metadataPatch;
	assert(Haikode::AI::UnifiedDiff::Parse(metadataDiff, metadataPatch,
		error));
	assert(!metadataPatch.Apply(root.string(), result, error));
	assert(error.find("sensitive project metadata") != std::string::npos);
	assert(!metadataPatch.ApplyFile(root.string(), ".haikode/project.json",
		result, error));
	assert(error.find("sensitive project metadata") != std::string::npos);

	const std::string ignoredBuildPathDiff =
		"diff --git a/build/generated.cpp b/build/generated.cpp\n"
		"new file mode 100644\n"
		"--- /dev/null\n"
		"+++ b/build/generated.cpp\n"
		"@@ -0,0 +1 @@\n"
		"+new\n";
	Haikode::AI::UnifiedDiff ignoredBuildPatch;
	assert(Haikode::AI::UnifiedDiff::Parse(ignoredBuildPathDiff,
		ignoredBuildPatch, error));
	assert(!ignoredBuildPatch.Apply(root.string(), result, error));
	assert(error.find("ignored/generated path") != std::string::npos);

	const std::string ignoredObjectPathDiff =
		"diff --git a/src/main.o b/src/main.o\n"
		"new file mode 100644\n"
		"--- /dev/null\n"
		"+++ b/src/main.o\n"
		"@@ -0,0 +1 @@\n"
		"+new\n";
	Haikode::AI::UnifiedDiff ignoredObjectPatch;
	assert(Haikode::AI::UnifiedDiff::Parse(ignoredObjectPathDiff,
		ignoredObjectPatch, error));
	assert(!ignoredObjectPatch.Apply(root.string(), result, error));
	assert(error.find("ignored/generated path") != std::string::npos);

	const std::string genioSettingsDiff =
		"diff --git a/.genio b/.genio\n"
		"--- a/.genio\n"
		"+++ b/.genio\n"
		"@@ -1 +1 @@\n"
		"-old\n"
		"+new\n";
	Haikode::AI::UnifiedDiff genioSettingsPatch;
	assert(Haikode::AI::UnifiedDiff::Parse(genioSettingsDiff,
		genioSettingsPatch, error));
	assert(!genioSettingsPatch.Apply(root.string(), result, error));
	assert(error.find("sensitive project metadata") != std::string::npos);

	const std::string sensitiveOldPathDiff =
		"diff --git a/.genio b/src/copied.cpp\n"
		"--- a/.genio\n"
		"+++ b/src/copied.cpp\n"
		"@@ -1 +1 @@\n"
		"-old\n"
		"+new\n";
	Haikode::AI::UnifiedDiff sensitiveOldPathPatch;
	assert(Haikode::AI::UnifiedDiff::Parse(sensitiveOldPathDiff,
		sensitiveOldPathPatch, error));
	assert(!sensitiveOldPathPatch.Apply(root.string(), result, error));
	assert(error.find("sensitive project metadata") != std::string::npos);
	assert(!sensitiveOldPathPatch.ApplyHunk(root.string(), "src/copied.cpp", 0,
		result, error));
	assert(error.find("sensitive project metadata") != std::string::npos);

	const std::string unsafeOldPathDiff =
		"diff --git a/../secret.cpp b/src/safe.cpp\n"
		"--- a/../secret.cpp\n"
		"+++ b/src/safe.cpp\n"
		"@@ -1 +1 @@\n"
		"-old\n"
		"+new\n";
	Haikode::AI::UnifiedDiff unsafeOldPathPatch;
	assert(Haikode::AI::UnifiedDiff::Parse(unsafeOldPathDiff,
		unsafeOldPathPatch, error));
	assert(!unsafeOldPathPatch.Apply(root.string(), result, error));
	assert(error.find("Unsafe") != std::string::npos);

	fs::remove_all(root);
	std::cout << "ai-patch-smoke-ok\n";
	return 0;
}
