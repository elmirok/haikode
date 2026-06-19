#ifndef HAIKODE_CORE_CHAT_CLIENT_H
#define HAIKODE_CORE_CHAT_CLIENT_H

#include "core/ChatRequest.h"
#include "core/OAuthClient.h"

#include <string>

class ChatClient {
public:
	bool Send(const ProviderSettings& settings, const OAuthToken& token,
		const ChatRequest& request, std::string& responseText,
		std::string& error) const;
	bool SendWithApiKey(const ProviderSettings& settings,
		const std::string& apiKey, const ChatRequest& request,
		std::string& responseText, std::string& error) const;
};

#endif
