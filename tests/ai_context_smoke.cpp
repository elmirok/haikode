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

int
main()
{
	assert(Haikode::AI::SelectContextText("selected text", "full file")
		== "selected text");
	assert(Haikode::AI::SelectContextText("", "full file") == "full file");

	Haikode::AI::VibeCodingRequest request;
	request.projectRoot = "/boot/home/project";
	request.userPrompt = "Explain this";
	request.files.push_back({
		"/boot/home/project/src/main.cpp",
		Haikode::AI::SelectContextText("", "int main() { return 0; }\n"),
		false
	});
	request.files.push_back({
		"/boot/home/project/src/App.cpp",
		"void App() {}\n",
		false
	});
	request.files.push_back({
		"/boot/home/project/src/Large.cpp",
		"truncated prefix\n",
		true
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
	assert(result.prompt.find("int main()") != std::string::npos);
	assert(result.prompt.find("src/App.cpp") != std::string::npos);
	assert(result.prompt.find("void App()") != std::string::npos);
	assert(result.prompt.find("src/Large.cpp") != std::string::npos);
	assert(result.prompt.find("Project map:") != std::string::npos);
	assert(result.prompt.find("Application entry point") != std::string::npos);
	assert(result.prompt.find("Response contract:") != std::string::npos);
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
		std::ofstream(root / "build" / "main.o") << "object";
		std::ofstream(root / ".git" / "config") << "ignored";
	}
	const std::vector<Haikode::AI::ProjectFileSummary> map
		= Haikode::AI::BuildProjectMap(root.string(), 10);
	assert(map.size() == 2);
	assert(map[0].path == "Readme.md");
	assert(map[0].role == "docs");
	assert(map[1].path == "src/main.cpp");
	assert(map[1].language == "C++");
	assert(map[1].hasTodo);
	assert(map[1].summary.find("4 line(s)") != std::string::npos);
	Haikode::AI::ContextFile selectedFile;
	std::string loadError;
	assert(Haikode::AI::LoadProjectContextFile(root.string(), "src/main.cpp",
		1024, selectedFile, loadError));
	assert(selectedFile.path == "src/main.cpp");
	assert(selectedFile.text.find("TODO: wire app") != std::string::npos);
	assert(!selectedFile.truncated);
	assert(Haikode::AI::LoadProjectContextFile(root.string(), "src/main.cpp",
		12, selectedFile, loadError));
	assert(selectedFile.truncated);
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
	size_t totalProjectFiles = 0;
	const std::vector<Haikode::AI::ProjectFileSummary> limitedMap
		= Haikode::AI::BuildProjectMap(root.string(), 1, &totalProjectFiles);
	assert(limitedMap.size() == 1);
	assert(totalProjectFiles == 2);
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
