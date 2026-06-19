/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "ai/AIProvider.h"
#include "ai/AIReadiness.h"

#include <cassert>
#include <iostream>

int
main()
{
	Haikode::AI::ProviderSettings provider;
	provider.baseUrl = " https://api.openai.com/ ";
	provider.model = " gpt-4.1-mini ";
	provider.authMode = Haikode::AI::AuthMode::ApiKey;

	Haikode::AI::AIReadinessStatus status
		= Haikode::AI::EvaluateAIReadiness(provider, false);
	assert(status.state == Haikode::AI::AIReadinessState::NetworkDisabled);
	assert(status.ready == false);
	assert(status.summary.find("Network AI disabled") != std::string::npos);
	assert(status.action.find("HAIKODE_AI_NETWORK=1") != std::string::npos);
	assert(!status.safeDetails.empty());

	status = Haikode::AI::EvaluateAIReadiness(provider, true);
	assert(status.state == Haikode::AI::AIReadinessState::MissingCredential);
	assert(status.ready == false);
	assert(status.summary.find("API key missing") != std::string::npos);
	assert(status.action.find("AI Setup") != std::string::npos);

	provider.apiKey = " sk-secret-test-key ";
	status = Haikode::AI::EvaluateAIReadiness(provider, true);
	assert(status.state == Haikode::AI::AIReadinessState::Ready);
	assert(status.ready);
	assert(status.summary.find("Ready") != std::string::npos);
	assert(status.safeDetails.find("sk-secret") == std::string::npos);
	assert(status.safeDetails.find("api-key") != std::string::npos);
	assert(status.safeDetails.find("https://api.openai.com/v1/chat/completions")
		!= std::string::npos);

	provider.authMode = Haikode::AI::AuthMode::OAuth;
	provider.apiKey = "";
	provider.oauthToken = " oauth-secret-token ";
	status = Haikode::AI::EvaluateAIReadiness(provider, true);
	assert(status.ready);
	assert(status.safeDetails.find("oauth-secret") == std::string::npos);
	assert(status.safeDetails.find("oauth") != std::string::npos);

	provider.authMode = Haikode::AI::AuthMode::Local;
	provider.oauthToken = "";
	provider.baseUrl = " http://127.0.0.1:11434 ";
	provider.model = " llama3.1 ";
	status = Haikode::AI::EvaluateAIReadiness(provider, false);
	assert(status.state == Haikode::AI::AIReadinessState::NetworkDisabled);
	assert(!status.ready);
	assert(status.summary.find("Network AI disabled") != std::string::npos);
	assert(status.safeDetails.find("127.0.0.1") != std::string::npos);

	status = Haikode::AI::EvaluateAIReadiness(provider, true);
	assert(status.state == Haikode::AI::AIReadinessState::Ready);
	assert(status.ready);
	assert(status.summary.find("Ready") != std::string::npos);

	provider.model = "   ";
	status = Haikode::AI::EvaluateAIReadiness(provider, true);
	assert(status.state == Haikode::AI::AIReadinessState::InvalidProvider);
	assert(!status.ready);
	assert(status.summary.find("Model missing") != std::string::npos);

	std::cout << "ai-readiness-smoke-ok\n";
	return 0;
}
