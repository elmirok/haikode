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

	Haikode::AI::PatchApplyResult result;
	assert(parsed.Apply(root.string(), result, error));
	assert(ReadFile(root / "src" / "main.cpp") == "one\nnew\nthree\n");
	assert(!result.backupDirectory.empty());
	assert(fs::exists(result.backupDirectory));
	assert(ReadFile(fs::path(result.backupDirectory) / "src" / "main.cpp")
		== "one\nold\nthree\n");

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
