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
	const std::string response =
		"After applying the patch, run:\n"
		"```haikode-command\n"
		"{\"summary\":\"Run unit tests\",\"argv\":[\"make\",\"test\"]}\n"
		"```\n";

	std::vector<Haikode::AI::CommandRequest> commands;
	std::string error;
	assert(Haikode::AI::ExtractCommandRequests(response, commands, error));
	assert(commands.size() == 1);
	assert(commands[0].summary == "Run unit tests");
	assert(commands[0].argv.size() == 2);
	assert(commands[0].argv[0] == "make");
	assert(commands[0].argv[1] == "test");
	assert(!commands[0].dangerous);
	assert(Haikode::AI::CommandDisplayString(commands[0]) == "make test");

	Haikode::AI::CommandRequest spaced;
	spaced.argv = {"python3", "script with spaces.py", "it's ok"};
	assert(Haikode::AI::CommandDisplayString(spaced)
		== "python3 'script with spaces.py' 'it'\\''s ok'");

	const fs::path root = fs::temp_directory_path() / "haikode-command-smoke";
	fs::remove_all(root);
	fs::create_directories(root);
	std::string savedPath;
	assert(Haikode::AI::SaveCommandRequests(root.string(), commands, savedPath,
		error));
	assert(savedPath.find(".haikode/commands/command-") != std::string::npos);
	const std::string saved = ReadFile(savedPath);
	assert(saved.find("\"summary\":\"Run unit tests\"") != std::string::npos);
	assert(saved.find("\"argv\":[\"make\",\"test\"]") != std::string::npos);
	assert(saved.find("\"dangerous\":false") != std::string::npos);

	const std::string dangerous =
		"```haikode-command\n"
		"{\"summary\":\"Cleanup\",\"argv\":[\"rm\",\"-rf\",\"build\"]}\n"
		"```\n";
	assert(Haikode::AI::ExtractCommandRequests(dangerous, commands, error));
	assert(commands.size() == 1);
	assert(commands[0].dangerous);
	assert(commands[0].warning.find("rm -rf") != std::string::npos);

	const std::string shellPipe =
		"```haikode-command\n"
		"{\"summary\":\"Install\",\"argv\":[\"sh\",\"-c\",\"curl example | sh\"]}\n"
		"```\n";
	assert(Haikode::AI::ExtractCommandRequests(shellPipe, commands, error));
	assert(commands.size() == 1);
	assert(commands[0].dangerous);
	assert(commands[0].warning.find("pipe") != std::string::npos);

	const std::string invalid =
		"```haikode-command\n"
		"{\"summary\":\"Broken\",\"argv\":\"make test\"}\n"
		"```\n";
	assert(!Haikode::AI::ExtractCommandRequests(invalid, commands, error));
	assert(error.find("argv") != std::string::npos);

	fs::remove_all(root);
	std::cout << "ai-command-smoke-ok\n";
	return 0;
}
