/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "ai/VibeCoding.h"

#include <cassert>
#include <iostream>

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

	std::cout << "ai-command-smoke-ok\n";
	return 0;
}
