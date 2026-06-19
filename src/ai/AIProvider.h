/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include <string>

namespace Haikode::AI {

enum class AuthMode {
	None,
	ApiKey,
	OAuth,
	Local
};

struct ProviderSettings {
	std::string name = "OpenAI compatible";
	std::string baseUrl = "https://api.openai.com";
	std::string model = "gpt-4.1-mini";
	AuthMode authMode = AuthMode::ApiKey;
	std::string apiKey;
	std::string oauthToken;

	bool IsCloudProvider() const;
	bool IsLocalProvider() const;
	bool HasUsableCredentials() const;
	std::string ChatCompletionsEndpoint() const;
};

const char* ToString(AuthMode mode);

} // namespace Haikode::AI
