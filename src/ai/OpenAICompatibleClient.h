/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include "AIProvider.h"

#include <string>

namespace Haikode::AI {

struct ChatRequest {
	std::string prompt;
	size_t maxTokens = 0;
};

struct ChatResponse {
	std::string text;
	std::string rawBody;
	long httpStatus = 0;
};

struct PreparedChatRequest {
	std::string url;
	std::string body;
	std::string authorizationHeader;
};

class OpenAICompatibleClient {
public:
	static PreparedChatRequest Prepare(const ProviderSettings& provider,
		const ChatRequest& request);
	static bool ExtractResponseText(const std::string& body, std::string& text,
		std::string& error);
	static std::string ExtractErrorMessage(const std::string& body);

	bool Send(const ProviderSettings& provider, const ChatRequest& request,
		ChatResponse& response, std::string& error) const;
};

} // namespace Haikode::AI
