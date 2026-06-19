/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "AIReadiness.h"

#include <cctype>
#include <sstream>

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


AIReadinessStatus
Status(AIReadinessState state, const std::string& summary,
	const std::string& action, const ProviderSettings& provider)
{
	AIReadinessStatus status;
	status.state = state;
	status.ready = state == AIReadinessState::Ready;
	status.summary = summary;
	status.action = action;

	std::ostringstream details;
	details
		<< "auth=" << ToString(provider.authMode)
		<< ", model=" << TrimWhitespace(provider.model)
		<< ", endpoint=" << provider.ChatCompletionsEndpoint();
	status.safeDetails = details.str();
	return status;
}

} // namespace

const char*
ToString(AIReadinessState state)
{
	switch (state) {
		case AIReadinessState::Ready:
			return "ready";
		case AIReadinessState::NetworkDisabled:
			return "network-disabled";
		case AIReadinessState::MissingCredential:
			return "missing-credential";
		case AIReadinessState::InvalidProvider:
			return "invalid-provider";
	}
	return "unknown";
}


AIReadinessStatus
EvaluateAIReadiness(const ProviderSettings& provider, bool networkEnabled)
{
	if (TrimWhitespace(provider.baseUrl).empty()) {
		return Status(AIReadinessState::InvalidProvider,
			"Provider URL missing.",
			"Open AI Setup and choose a provider preset or enter a base URL.",
			provider);
	}

	if (TrimWhitespace(provider.model).empty()) {
		return Status(AIReadinessState::InvalidProvider,
			"Model missing.",
			"Open AI Setup and enter the model name expected by your provider.",
			provider);
	}

	if (provider.authMode == AuthMode::None) {
		return Status(AIReadinessState::InvalidProvider,
			"AI provider auth is disabled.",
			"Choose api-key, oauth, or local auth mode in AI Setup.",
			provider);
	}

	if (provider.authMode != AuthMode::None && !networkEnabled) {
		return Status(AIReadinessState::NetworkDisabled,
			"Network AI disabled in this build.",
			"Rebuild Haikode with HAIKODE_AI_NETWORK=1 and install curl_devel.",
			provider);
	}

	if (provider.authMode == AuthMode::ApiKey
		&& TrimWhitespace(provider.apiKey).empty()) {
		return Status(AIReadinessState::MissingCredential,
			"API key missing.",
			"Open AI Setup, paste the provider API key, then use Save & Test.",
			provider);
	}

	if (provider.authMode == AuthMode::OAuth
		&& TrimWhitespace(provider.oauthToken).empty()) {
		return Status(AIReadinessState::MissingCredential,
			"OAuth bearer token missing.",
			"Open AI Setup, complete OAuth, then use Save & Test.",
			provider);
	}

	return Status(AIReadinessState::Ready,
		"Ready to send AI requests.",
		"Use Ask, Explain file, Summarize project, or Propose patch.",
		provider);
}


bool
ShouldOpenSetupForReadiness(const AIReadinessStatus& status)
{
	return status.state == AIReadinessState::MissingCredential
		|| status.state == AIReadinessState::InvalidProvider;
}


std::string
FormatReadinessFailureTranscript(const AIReadinessStatus& status)
{
	std::ostringstream transcript;
	transcript << "AI request not started: " << status.summary;
	if (!status.action.empty())
		transcript << "\nNext step: " << status.action;
	if (!status.safeDetails.empty())
		transcript << "\nProvider: " << status.safeDetails;
	return transcript.str();
}

} // namespace Haikode::AI
