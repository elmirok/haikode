/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "ai/VibeCoding.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

static void
WriteFile(const fs::path& path, const std::string& text)
{
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
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
	assert(Haikode::AI::SelectContextText("selected text", "full file")
		== "selected text");
	assert(Haikode::AI::SelectContextText("", "full file") == "full file");
	std::string loadError;

	const fs::path freshRoot = fs::temp_directory_path()
		/ "haikode-fresh-command-smoke";
	fs::remove_all(freshRoot);
	fs::create_directories(freshRoot);
	WriteFile(freshRoot / "Makefile",
		"all:\n\t@echo build\n\ntest:\n\t@echo test\n");
	Haikode::AI::ProjectMemory inferredMemory;
	assert(Haikode::AI::InferProjectCommands(freshRoot.string(),
		inferredMemory, loadError));
	assert(inferredMemory.defaultBuildCommand == "make");
	assert(inferredMemory.defaultTestCommand == "make test");
	fs::remove_all(freshRoot);

	Haikode::AI::VibeCodingRequest request;
	request.projectRoot = "/boot/home/project";
	request.userPrompt = "Explain this";
	request.files.push_back({
		"/boot/home/project/src/main.cpp",
		Haikode::AI::SelectContextText("", "int main() { return 0; }\n"),
		false,
		"deac66ccb79f6d31c0fa7d358de48e083c15c02ff50ec1ebd4b64314b9e6e196"
	});
	request.files.push_back({
		"/boot/home/project/src/App.cpp",
		"void App() {}\n",
		false,
		"b46466272d30e06c09d43791f5ca023cc4b9d530a8f21f621bd2d4a2067799ce"
	});
	request.files.push_back({
		"/boot/home/project/src/Large.cpp",
		"truncated prefix\n",
		true,
		""
	});
	request.contextWarnings.push_back(
		"Selected project file was not included: ignored path");
	request.projectFiles.push_back({"src/main.cpp", "C++", "source",
		"high", true, "Application entry point"});
	request.projectFiles.push_back({"Readme.md", "Markdown", "docs",
		"low", false, "Project documentation"});

	Haikode::AI::PromptBuilder builder;
	const Haikode::AI::PromptBuildResult result = builder.Build(request, 1024, 10);
	assert(result.prompt.find("src/main.cpp") != std::string::npos);
	assert(result.prompt.find(
		"sha256=deac66ccb79f6d31c0fa7d358de48e083c15c02ff50ec1ebd4b64314b9e6e196")
		!= std::string::npos);
	assert(result.prompt.find("int main()") != std::string::npos);
	assert(result.prompt.find("src/App.cpp") != std::string::npos);
	assert(result.prompt.find(
		"sha256=b46466272d30e06c09d43791f5ca023cc4b9d530a8f21f621bd2d4a2067799ce")
		!= std::string::npos);
	assert(result.prompt.find("void App()") != std::string::npos);
	assert(result.prompt.find("src/Large.cpp") != std::string::npos);
	assert(result.prompt.find("File: /boot/home/project/src/Large.cpp sha256=")
		== std::string::npos);
	assert(result.prompt.find("Project map:") != std::string::npos);
	assert(result.prompt.find("Application entry point") != std::string::npos);
	assert(result.prompt.find("Response contract:") != std::string::npos);
	assert(result.prompt.find("Patch paths must not be absolute")
		!= std::string::npos);
	assert(result.prompt.find(".haikode") != std::string::npos);
	assert(result.prompt.find("Return at most one unified diff")
		!= std::string::npos);
	assert(result.prompt.find("```haikode-edit") != std::string::npos);
	assert(result.prompt.find("\"original_sha256\"") != std::string::npos);
	assert(result.prompt.find("```haikode-command") != std::string::npos);
	assert(result.prompt.find("\"argv\":[\"make\",\"test\"]")
		!= std::string::npos);
	assert(result.prompt.find("Do not use shell strings") != std::string::npos);
	assert(result.prompt.find("Never claim that a command was run")
		!= std::string::npos);
	assert(result.warnings.size() >= 2);
	assert(result.warnings[0].find("Selected project file was not included")
		!= std::string::npos);
	assert(result.warnings[1].find("src/Large.cpp was truncated")
		!= std::string::npos);

	const Haikode::AI::PromptBuildResult fileLimited
		= builder.Build(request, 1024, 1);
	assert(fileLimited.prompt.find("int main()") != std::string::npos);
	assert(fileLimited.prompt.find("void App()") == std::string::npos);
	assert(!fileLimited.warnings.empty());
	bool sawFileLimitWarning = false;
	for (const std::string& warning : fileLimited.warnings) {
		sawFileLimitWarning = sawFileLimitWarning
			|| warning.find("Some files were omitted") != std::string::npos;
	}
	assert(sawFileLimitWarning);

	const Haikode::AI::PromptBuildResult limited = builder.Build(request, 1024, 10,
		1);
	assert(limited.prompt.find("Application entry point") != std::string::npos);
	assert(limited.prompt.find("Project documentation") == std::string::npos);
	assert(!limited.warnings.empty());

	request.mode = Haikode::AI::PromptMode::ExplainSelection;
	request.userPrompt = "";
	const Haikode::AI::PromptBuildResult explanation
		= builder.Build(request, 1024, 10);
	assert(explanation.prompt.find("Explain the selected code or active file")
		!= std::string::npos);
	assert(explanation.prompt.find("int main()") != std::string::npos);

	request.mode = Haikode::AI::PromptMode::SummarizeProject;
	request.userPrompt = "";
	const Haikode::AI::PromptBuildResult projectSummary
		= builder.Build(request, 1024, 10);
	assert(projectSummary.prompt.find("Summarize this Haiku project")
		!= std::string::npos);
	assert(projectSummary.prompt.find("Project map:") != std::string::npos);

	request.mode = Haikode::AI::PromptMode::ReviewDiff;
	request.userPrompt = "Please sanity-check this patch.";
	request.pendingDiffPath = "src/main.cpp";
	request.pendingDiff = "diff --git a/src/main.cpp b/src/main.cpp\n"
		"--- a/src/main.cpp\n"
		"+++ b/src/main.cpp\n"
		"@@ -1 +1 @@\n"
		"-old\n"
		"+new\n";
	const Haikode::AI::PromptBuildResult patchReview
		= builder.Build(request, 1024, 10);
	assert(patchReview.prompt.find("Selected patch file: src/main.cpp")
		!= std::string::npos);
	assert(patchReview.prompt.find("Pending unified diff:") != std::string::npos);
	assert(patchReview.prompt.find("```diff") != std::string::npos);
	assert(patchReview.prompt.find("-old") != std::string::npos);

	const fs::path root = fs::temp_directory_path() / "haikode-context-smoke";
	fs::remove_all(root);
	fs::create_directories(root / "src");
	fs::create_directories(root / "build");
	fs::create_directories(root / ".git");
	{
		std::ofstream(root / "src" / "main.cpp")
			<< "int main() {\n\t// TODO: wire app\n\treturn 0;\n}\n";
		std::ofstream(root / "Readme.md") << "# Demo\n";
		std::ofstream(root / "Makefile")
			<< "all:\n\t@echo build\n\ntest:\n\t@echo test\n";
		std::ofstream(root / "build" / "main.o") << "object";
		std::ofstream(root / ".git" / "config") << "ignored";
	}
	const std::vector<Haikode::AI::ProjectFileSummary> map
		= Haikode::AI::BuildProjectMap(root.string(), 10);
	assert(map.size() == 3);
	assert(map[0].path == "Makefile");
	assert(map[0].role == "build");
	assert(map[1].path == "Readme.md");
	assert(map[1].role == "docs");
	assert(map[2].path == "src/main.cpp");
	assert(map[2].language == "C++");
	assert(map[2].hasTodo);
	assert(map[2].summary.find("4 line(s)") != std::string::npos);
	std::string memoryPath;
	const fs::path canonicalRoot = fs::weakly_canonical(root);
	assert(Haikode::AI::SaveProjectMemory(root.string(), map, map.size(),
		memoryPath, loadError));
	assert(memoryPath == (canonicalRoot / ".haikode" / "project.json").string());
	assert(fs::is_directory(root / ".haikode" / "sessions"));
	assert(fs::is_directory(root / ".haikode" / "notes"));
	assert(fs::is_directory(root / ".haikode" / "patches"));
	assert(fs::is_directory(root / ".haikode" / "logs"));
	assert(fs::is_directory(root / ".haikode" / "backups"));
	assert(fs::is_directory(root / ".haikode" / "commands"));
	const std::string memory = ReadFile(memoryPath);
	assert(memory.find("\"name\":\"haikode-context-smoke\"")
		!= std::string::npos);
	assert(memory.find("\"path\":\"src/main.cpp\"") != std::string::npos);
	assert(memory.find("\"todo\":true") != std::string::npos);
	assert(memory.find("\"default_build_command\":\"make\"")
		!= std::string::npos);
	assert(memory.find("\"default_test_command\":\"make test\"")
		!= std::string::npos);
	std::vector<Haikode::AI::ProjectFileSummary> rememberedFiles;
	size_t rememberedCandidateCount = 0;
	assert(Haikode::AI::LoadProjectMemory(root.string(), 10,
		rememberedFiles, rememberedCandidateCount, loadError));
	Haikode::AI::ProjectMemory rememberedMemory;
	assert(Haikode::AI::LoadProjectMemory(root.string(), 10,
		rememberedMemory, loadError));
	assert(rememberedMemory.defaultBuildCommand == "make");
	assert(rememberedMemory.defaultTestCommand == "make test");
	assert(rememberedCandidateCount == map.size());
	assert(rememberedFiles.size() == map.size());
	assert(rememberedFiles[2].path == "src/main.cpp");
	assert(rememberedFiles[2].hasTodo);
	Haikode::AI::VibeCodingRequest rememberedRequest;
	rememberedRequest.projectRoot = root.string();
	rememberedRequest.defaultBuildCommand = rememberedMemory.defaultBuildCommand;
	rememberedRequest.defaultTestCommand = rememberedMemory.defaultTestCommand;
	rememberedRequest.projectFiles = rememberedFiles;
	rememberedRequest.projectMapCandidateCount = rememberedCandidateCount;
	const Haikode::AI::PromptBuildResult rememberedPrompt
		= builder.Build(rememberedRequest, 1024, 10);
	assert(rememberedPrompt.prompt.find("src/main.cpp") != std::string::npos);
	assert(rememberedPrompt.prompt.find("TODO marker") != std::string::npos);
	assert(rememberedPrompt.prompt.find("Project commands:")
		!= std::string::npos);
	assert(rememberedPrompt.prompt.find("Build: make") != std::string::npos);
	assert(rememberedPrompt.prompt.find("Test: make test") != std::string::npos);
	const fs::path noBuildRoot = fs::temp_directory_path()
		/ "haikode-no-build-command-smoke";
	fs::remove_all(noBuildRoot);
	fs::create_directories(noBuildRoot / "src");
	WriteFile(noBuildRoot / "src" / "plain.cpp", "int plain() { return 1; }\n");
	const std::vector<Haikode::AI::ProjectFileSummary> noBuildMap
		= Haikode::AI::BuildProjectMap(noBuildRoot.string(), 10);
	std::string noBuildMemoryPath;
	assert(Haikode::AI::SaveProjectMemory(noBuildRoot.string(), noBuildMap,
		noBuildMap.size(), noBuildMemoryPath, loadError));
	const std::string noBuildMemory = ReadFile(noBuildMemoryPath);
	assert(noBuildMemory.find("\"default_build_command\":\"make\"")
		== std::string::npos);
	assert(noBuildMemory.find("\"default_test_command\":\"make test\"")
		== std::string::npos);
	Haikode::AI::ProjectMemory noBuildMemoryLoaded;
	assert(Haikode::AI::LoadProjectMemory(noBuildRoot.string(), 10,
		noBuildMemoryLoaded, loadError));
	assert(noBuildMemoryLoaded.defaultBuildCommand.empty());
	assert(noBuildMemoryLoaded.defaultTestCommand.empty());
	fs::remove_all(noBuildRoot);
	Haikode::AI::ContextFile selectedFile;
	assert(Haikode::AI::LoadProjectContextFile(root.string(), "src/main.cpp",
		1024, selectedFile, loadError));
	assert(selectedFile.path == "src/main.cpp");
	assert(selectedFile.text.find("TODO: wire app") != std::string::npos);
	assert(!selectedFile.truncated);
	assert(selectedFile.sha256
		== "02944f8a4393b14def327b5b34051d7d430505ead65fd5248ed39f5967dab6bb");
	assert(Haikode::AI::LoadProjectContextFile(root.string(), "src/main.cpp",
		12, selectedFile, loadError));
	assert(selectedFile.truncated);
	assert(selectedFile.sha256.empty());
	assert(selectedFile.text.size() == 12);
	WriteFile(root / "src" / "binary.dat", std::string("abc\0def", 7));
	assert(!Haikode::AI::LoadProjectContextFile(root.string(), "src/binary.dat",
		1024, selectedFile, loadError));
	assert(loadError.find("binary") != std::string::npos);
	assert(!Haikode::AI::LoadProjectContextFile(root.string(), "../outside.cpp",
		1024, selectedFile, loadError));
	assert(loadError.find("Unsafe") != std::string::npos);
	assert(!Haikode::AI::LoadProjectContextFile(root.string(), ".git/config",
		1024, selectedFile, loadError));
	assert(loadError.find("ignored") != std::string::npos);
	std::string normalizedPath;
	assert(Haikode::AI::NormalizeProjectContextPath(root.string(),
		(root / "src" / "main.cpp").string(), normalizedPath, loadError));
	assert(normalizedPath == "src/main.cpp");
	assert(Haikode::AI::NormalizeProjectContextPath(root.string(),
		"src/main.cpp", normalizedPath, loadError));
	assert(normalizedPath == "src/main.cpp");
	assert(!Haikode::AI::NormalizeProjectContextPath(root.string(),
		(root.parent_path() / "outside.cpp").string(), normalizedPath,
		loadError));
	assert(loadError.find("outside") != std::string::npos);
	assert(!Haikode::AI::NormalizeProjectContextPath(root.string(),
		".git/config", normalizedPath, loadError));
	assert(loadError.find("ignored") != std::string::npos);
	size_t totalProjectFiles = 0;
	const std::vector<Haikode::AI::ProjectFileSummary> limitedMap
		= Haikode::AI::BuildProjectMap(root.string(), 1, &totalProjectFiles);
	assert(limitedMap.size() == 1);
	assert(totalProjectFiles == 3);
	Haikode::AI::VibeCodingRequest mapLimitedRequest;
	mapLimitedRequest.projectRoot = root.string();
	mapLimitedRequest.projectFiles = limitedMap;
	mapLimitedRequest.projectMapCandidateCount = totalProjectFiles;
	const Haikode::AI::PromptBuildResult mapLimitedPrompt
		= builder.Build(mapLimitedRequest, 1024, 10);
	assert(!mapLimitedPrompt.warnings.empty());
	assert(mapLimitedPrompt.warnings[0].find("project-map") != std::string::npos);
	fs::remove_all(root);

	std::cout << "ai-context-smoke-ok\n";
	return 0;
}
