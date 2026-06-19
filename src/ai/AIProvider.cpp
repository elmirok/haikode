/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "AIProvider.h"

#include <cctype>

namespace Haikode::AI {

namespace {

std::string
TrimWhitespace(std::string value)
{
	size_t start = 0;
	while (start < value.size()
		&& std::isspace(static_cast<unsigned char>(value[start]))) {
		start++;
	}
	size_t end = value.size();
	while (end > start
		&& std::isspace(static_cast<unsigned char>(value[end - 1]))) {
		end--;
	}
	return value.substr(start, end - start);
}


bool
EndsWithSlash(const std::string& value)
{
	return !value.empty() && value[value.size() - 1] == '/';
}

std::string
TrimTrailingSlashes(std::string value)
{
	while (!value.empty() && value[value.size() - 1] == '/')
		value.pop_back();
	return value;
}


bool
EndsWith(const std::string& value, const std::string& suffix)
{
	return value.size() >= suffix.size()
		&& value.compare(value.size() - suffix.size(), suffix.size(), suffix)
			== 0;
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
		return !TrimWhitespace(apiKey).empty();
	if (authMode == AuthMode::OAuth)
		return !TrimWhitespace(oauthToken).empty();
	if (authMode == AuthMode::Local)
		return !TrimWhitespace(baseUrl).empty();
	return false;
}


bool
ProviderSettings::Validate(std::string& error) const
{
	error.clear();
	if (TrimWhitespace(baseUrl).empty()) {
		error = "Provider base URL is required.";
		return false;
	}

	if (TrimWhitespace(model).empty()) {
		error = "Provider model is required.";
		return false;
	}

	if (authMode == AuthMode::ApiKey && TrimWhitespace(apiKey).empty()) {
		error = "API key auth selected, but no API key is configured.";
		return false;
	}

	if (authMode == AuthMode::OAuth && TrimWhitespace(oauthToken).empty()) {
		error = "OAuth auth selected, but no OAuth bearer token is configured.";
		return false;
	}

	if (authMode == AuthMode::None) {
		error = "AI provider auth is disabled.";
		return false;
	}

	return HasUsableCredentials();
}


std::string
ProviderSettings::ChatCompletionsEndpoint() const
{
	const std::string trimmedUrl = TrimWhitespace(baseUrl);
	const std::string base = TrimTrailingSlashes(trimmedUrl);
	if (EndsWith(base, "/v1/chat/completions")
		|| EndsWith(base, "/chat/completions")) {
		return base;
	}
	if (EndsWith(base, "/v1"))
		return base + "/chat/completions";
	if (EndsWithSlash(trimmedUrl))
		return trimmedUrl + "v1/chat/completions";
	return base + "/v1/chat/completions";
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


ProviderSettings
ProviderPresetSettings(ProviderPreset preset)
{
	ProviderSettings settings;
	switch (preset) {
		case ProviderPreset::OpenAI:
			settings.name = "OpenAI";
			settings.baseUrl = "https://api.openai.com";
			settings.model = "gpt-4.1-mini";
			settings.authMode = AuthMode::ApiKey;
			break;
		case ProviderPreset::Ollama:
			settings.name = "Ollama";
			settings.baseUrl = "http://127.0.0.1:11434";
			settings.model = "llama3.1";
			settings.authMode = AuthMode::Local;
			break;
		case ProviderPreset::LMStudio:
			settings.name = "LM Studio";
			settings.baseUrl = "http://127.0.0.1:1234";
			settings.model = "local-model";
			settings.authMode = AuthMode::Local;
			break;
		case ProviderPreset::OpenRouter:
			settings.name = "OpenRouter";
			settings.baseUrl = "https://openrouter.ai/api";
			settings.model = "openai/gpt-4.1-mini";
			settings.authMode = AuthMode::ApiKey;
			break;
		case ProviderPreset::LlamaCpp:
			settings.name = "llama.cpp";
			settings.baseUrl = "http://127.0.0.1:8080";
			settings.model = "local-model";
			settings.authMode = AuthMode::Local;
			break;
	}
	return settings;
}

} // namespace Haikode::AI
