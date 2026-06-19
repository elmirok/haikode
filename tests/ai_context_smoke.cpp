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

	Haikode::AI::PromptBuilder builder;
	const Haikode::AI::PromptBuildResult result = builder.Build(request, 1024, 10);
	assert(result.prompt.find("src/main.cpp") != std::string::npos);
	assert(result.prompt.find("int main()") != std::string::npos);

	std::cout << "ai-context-smoke-ok\n";
	return 0;
}
