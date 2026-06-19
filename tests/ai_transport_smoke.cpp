/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "ai/AIProvider.h"
#include "ai/OpenAICompatibleClient.h"

#include <cassert>
#include <iostream>

int
main()
{
	Haikode::AI::ProviderSettings apiKeyProvider;
	apiKeyProvider.model = "test-model";
	apiKeyProvider.apiKey = "sk-test";
	Haikode::AI::ChatRequest request;
	request.prompt = "Explain this file";

	Haikode::AI::PreparedChatRequest prepared
		= Haikode::AI::OpenAICompatibleClient::Prepare(apiKeyProvider, request);
	assert(prepared.url == "https://api.openai.com/v1/chat/completions");
	assert(prepared.authorizationHeader == "Authorization: Bearer sk-test");
	assert(prepared.body.find("\"model\":\"test-model\"") != std::string::npos);
	assert(prepared.body.find("Explain this file") != std::string::npos);

	Haikode::AI::ProviderSettings localProvider;
	localProvider.baseUrl = "http://127.0.0.1:11434";
	localProvider.authMode = Haikode::AI::AuthMode::Local;
	prepared = Haikode::AI::OpenAICompatibleClient::Prepare(localProvider, request);
	assert(prepared.url == "http://127.0.0.1:11434/v1/chat/completions");
	assert(prepared.authorizationHeader.empty());

	Haikode::AI::ProviderSettings oauthProvider;
	oauthProvider.authMode = Haikode::AI::AuthMode::OAuth;
	oauthProvider.oauthToken = "oauth-token";
	prepared = Haikode::AI::OpenAICompatibleClient::Prepare(oauthProvider, request);
	assert(prepared.authorizationHeader == "Authorization: Bearer oauth-token");

	const std::string response =
		"{\"choices\":[{\"message\":{\"content\":\"Hello from model\"}}]}";
	std::string text;
	std::string error;
	assert(Haikode::AI::OpenAICompatibleClient::ExtractResponseText(response,
		text, error));
	assert(text == "Hello from model");

	const std::string spacedResponse =
		"{\"choices\":[{\"message\":{\"content\" : \"Hello with spaces\"}}]}";
	assert(Haikode::AI::OpenAICompatibleClient::ExtractResponseText(
		spacedResponse, text, error));
	assert(text == "Hello with spaces");

	std::cout << "ai-transport-smoke-ok\n";
	return 0;
}
