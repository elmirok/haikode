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
	std::string validationError;
	assert(apiKeyProvider.Validate(validationError));
	Haikode::AI::ChatRequest request;
	request.prompt = "Explain this file";

	Haikode::AI::PreparedChatRequest prepared
		= Haikode::AI::OpenAICompatibleClient::Prepare(apiKeyProvider, request);
	assert(prepared.url == "https://api.openai.com/v1/chat/completions");
	assert(prepared.authorizationHeader == "Authorization: Bearer sk-test");
	assert(prepared.body.find("\"model\":\"test-model\"") != std::string::npos);
	assert(prepared.body.find("Explain this file") != std::string::npos);

	request.maxTokens = 32;
	prepared = Haikode::AI::OpenAICompatibleClient::Prepare(apiKeyProvider, request);
	assert(prepared.body.find("\"max_tokens\":32") != std::string::npos);

	const Haikode::AI::ProviderSettings openaiPreset
		= Haikode::AI::ProviderPresetSettings(
			Haikode::AI::ProviderPreset::OpenAI);
	assert(openaiPreset.baseUrl == "https://api.openai.com");
	assert(openaiPreset.model == "gpt-4.1-mini");
	assert(openaiPreset.authMode == Haikode::AI::AuthMode::ApiKey);

	const Haikode::AI::ProviderSettings ollamaPreset
		= Haikode::AI::ProviderPresetSettings(
			Haikode::AI::ProviderPreset::Ollama);
	assert(ollamaPreset.baseUrl == "http://127.0.0.1:11434");
	assert(ollamaPreset.model == "llama3.1");
	assert(ollamaPreset.authMode == Haikode::AI::AuthMode::Local);

	const Haikode::AI::ProviderSettings lmStudioPreset
		= Haikode::AI::ProviderPresetSettings(
			Haikode::AI::ProviderPreset::LMStudio);
	assert(lmStudioPreset.baseUrl == "http://127.0.0.1:1234");
	assert(lmStudioPreset.authMode == Haikode::AI::AuthMode::Local);

	const Haikode::AI::ProviderSettings openRouterPreset
		= Haikode::AI::ProviderPresetSettings(
			Haikode::AI::ProviderPreset::OpenRouter);
	assert(openRouterPreset.baseUrl == "https://openrouter.ai/api");
	assert(openRouterPreset.model == "openai/gpt-4.1-mini");
	assert(openRouterPreset.authMode == Haikode::AI::AuthMode::ApiKey);
	Haikode::AI::ProviderSettings openRouterV1 = openRouterPreset;
	openRouterV1.apiKey = "router-key";
	openRouterV1.baseUrl = "https://openrouter.ai/api/v1";
	prepared = Haikode::AI::OpenAICompatibleClient::Prepare(openRouterV1, request);
	assert(prepared.url == "https://openrouter.ai/api/v1/chat/completions");

	const Haikode::AI::ProviderSettings llamaCppPreset
		= Haikode::AI::ProviderPresetSettings(
			Haikode::AI::ProviderPreset::LlamaCpp);
	assert(llamaCppPreset.baseUrl == "http://127.0.0.1:8080");
	assert(llamaCppPreset.authMode == Haikode::AI::AuthMode::Local);

	Haikode::AI::ProviderSettings localProvider;
	localProvider.baseUrl = "http://127.0.0.1:11434";
	localProvider.authMode = Haikode::AI::AuthMode::Local;
	assert(localProvider.Validate(validationError));
	prepared = Haikode::AI::OpenAICompatibleClient::Prepare(localProvider, request);
	assert(prepared.url == "http://127.0.0.1:11434/v1/chat/completions");
	assert(prepared.authorizationHeader.empty());

	localProvider.baseUrl = "http://127.0.0.1:11434/";
	prepared = Haikode::AI::OpenAICompatibleClient::Prepare(localProvider, request);
	assert(prepared.url == "http://127.0.0.1:11434/v1/chat/completions");
	localProvider.baseUrl = "http://127.0.0.1:11434/v1";
	prepared = Haikode::AI::OpenAICompatibleClient::Prepare(localProvider, request);
	assert(prepared.url == "http://127.0.0.1:11434/v1/chat/completions");
	localProvider.baseUrl = "http://127.0.0.1:11434/v1/chat/completions";
	prepared = Haikode::AI::OpenAICompatibleClient::Prepare(localProvider, request);
	assert(prepared.url == "http://127.0.0.1:11434/v1/chat/completions");

	Haikode::AI::ProviderSettings oauthProvider;
	oauthProvider.authMode = Haikode::AI::AuthMode::OAuth;
	oauthProvider.oauthToken = "oauth-token";
	assert(oauthProvider.Validate(validationError));
	prepared = Haikode::AI::OpenAICompatibleClient::Prepare(oauthProvider, request);
	assert(prepared.authorizationHeader == "Authorization: Bearer oauth-token");

	Haikode::AI::CancellationToken cancellation;
	cancellation.Cancel();
	Haikode::AI::OpenAICompatibleClient client;
	Haikode::AI::ChatResponse cancelledResponse;
	std::string cancelledError;
	assert(!client.Send(apiKeyProvider, request, cancelledResponse,
		cancelledError, &cancellation));
	assert(cancelledError.find("cancelled") != std::string::npos);

	Haikode::AI::ProviderSettings missingApiKey;
	assert(!missingApiKey.Validate(validationError));
	assert(validationError.find("API key") != std::string::npos);

	Haikode::AI::ProviderSettings missingOAuth;
	missingOAuth.authMode = Haikode::AI::AuthMode::OAuth;
	assert(!missingOAuth.Validate(validationError));
	assert(validationError.find("OAuth") != std::string::npos);

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

	const std::string contentArrayResponse =
		"{\"choices\":[{\"message\":{\"content\":["
		"{\"type\":\"text\",\"text\":\"first part\"},"
		"{\"type\":\"text\",\"text\":\"second part\"}]}}]}";
	assert(Haikode::AI::OpenAICompatibleClient::ExtractResponseText(
		contentArrayResponse, text, error));
	assert(text == "first part\nsecond part");

	assert(Haikode::AI::OpenAICompatibleClient::ExtractResponseText(
		"{\"text\":\"Local model text\"}", text, error));
	assert(text == "Local model text");
	assert(Haikode::AI::OpenAICompatibleClient::ExtractResponseText(
		"{\"response\":\"Local model response\"}", text, error));
	assert(text == "Local model response");

	assert(Haikode::AI::OpenAICompatibleClient::ExtractErrorMessage(
		"{\"error\":{\"message\":\"model not found\",\"type\":\"invalid_request\"}}")
		== "model not found");
	assert(Haikode::AI::OpenAICompatibleClient::ExtractErrorMessage(
		"{\"message\":\"local server unavailable\"}") == "local server unavailable");
	assert(Haikode::AI::OpenAICompatibleClient::ExtractErrorMessage(
		"{\"error\":\"invalid_api_key\"}") == "invalid_api_key");

	std::cout << "ai-transport-smoke-ok\n";
	return 0;
}
