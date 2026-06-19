/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "ai/ProcessCapture.h"

#include <cassert>
#include <filesystem>
#include <fstream>
#include <iostream>

namespace fs = std::filesystem;

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
	const fs::path root = fs::temp_directory_path() / "haikode-process-capture";
	fs::remove_all(root);
	fs::create_directories(root);

	Haikode::AI::ProcessCaptureOptions options;
	options.workingDirectory = root.string();
	options.argv = {"/bin/pwd"};
	Haikode::AI::ProcessCaptureResult result;
	std::string error;
	assert(Haikode::AI::ProcessCapture::Run(options, result, error));
	assert(result.output.find(root.string()) != std::string::npos);
	assert(result.exitCode == 0);
	assert(!result.timedOut);

	options.argv = {"/bin/echo", "argument with spaces", "semi;colon"};
	assert(Haikode::AI::ProcessCapture::Run(options, result, error));
	assert(result.output.find("argument with spaces semi;colon") != std::string::npos);
	std::string savedPath;
	std::string saveError;
	assert(Haikode::AI::ProcessCapture::SaveLog(root.string(), "codex ask",
		options, result, error, savedPath, saveError));
	assert(savedPath.find(".haikode/logs/codex-ask-") != std::string::npos);
	const std::string savedLog = ReadFile(savedPath);
	assert(savedLog.find("label: codex ask") != std::string::npos);
	assert(savedLog.find("[0] /bin/echo") != std::string::npos);
	assert(savedLog.find("argument with spaces semi;colon") != std::string::npos);
	std::string secondSavedPath;
	assert(Haikode::AI::ProcessCapture::SaveLog(root.string(), "codex ask",
		options, result, error, secondSavedPath, saveError));
	assert(secondSavedPath.find(".haikode/logs/codex-ask-")
		!= std::string::npos);
	assert(secondSavedPath != savedPath);
	assert(!ReadFile(secondSavedPath).empty());

	options.argv = {"/bin/sh", "-c", "exit 7"};
	assert(!Haikode::AI::ProcessCapture::Run(options, result, error));
	assert(result.exitCode == 7);

	options.argv = {"/bin/sleep", "2"};
	options.timeoutSeconds = 1;
	assert(!Haikode::AI::ProcessCapture::Run(options, result, error));
	assert(result.timedOut);
	assert(error.find("timed out") != std::string::npos);

	Haikode::AI::CancellationToken cancellation;
	cancellation.Cancel();
	options.timeoutSeconds = 10;
	options.cancellation = &cancellation;
	assert(!Haikode::AI::ProcessCapture::Run(options, result, error));
	assert(result.cancelled);
	assert(error.find("cancelled") != std::string::npos);
	options.cancellation = nullptr;

	options.argv.clear();
	assert(!Haikode::AI::ProcessCapture::Run(options, result, error));
	assert(error.find("argv") != std::string::npos);

	fs::remove_all(root);
	std::cout << "process-capture-smoke-ok\n";
	return 0;
}
