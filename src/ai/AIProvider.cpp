/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "AIProvider.h"

namespace Haikode::AI {

namespace {

bool
EndsWithSlash(const std::string& value)
{
	return !value.empty() && value[value.size() - 1] == '/';
}

} // namespace

bool
ProviderSettings::IsCloudProvider() const
{
	return authMode == AuthMode::ApiKey || authMode == AuthMode::OAuth;
}


bool
ProviderSettings::IsLocalProvider() const
{
	return authMode == AuthMode::Local;
}


bool
ProviderSettings::HasUsableCredentials() const
{
	if (authMode == AuthMode::ApiKey)
		return !apiKey.empty();
	if (authMode == AuthMode::OAuth)
		return !oauthToken.empty();
	if (authMode == AuthMode::Local)
		return !baseUrl.empty();
	return false;
}


std::string
ProviderSettings::ChatCompletionsEndpoint() const
{
	if (EndsWithSlash(baseUrl))
		return baseUrl + "v1/chat/completions";
	return baseUrl + "/v1/chat/completions";
}


const char*
ToString(AuthMode mode)
{
	switch (mode) {
		case AuthMode::None:
			return "none";
		case AuthMode::ApiKey:
			return "api-key";
		case AuthMode::OAuth:
			return "oauth";
		case AuthMode::Local:
			return "local";
	}
	return "unknown";
}

} // namespace Haikode::AI
