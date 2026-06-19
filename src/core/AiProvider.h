#ifndef HAIKODE_CORE_AI_PROVIDER_H
#define HAIKODE_CORE_AI_PROVIDER_H

#include "core/ChatRequest.h"
#include "core/OAuthClient.h"

#include <string>

class AiProvider {
public:
	virtual ~AiProvider() = default;

	virtual bool Send(const ProviderSettings& settings,
		const ChatRequest& request, std::string& responseText,
		std::string& error) const = 0;
};

class OpenAICompatibleProvider final : public AiProvider {
public:
	bool Send(const ProviderSettings& settings, const ChatRequest& request,
		std::string& responseText, std::string& error) const override;

	static std::string ReadApiKeyFromEnvironment();
};

#endif
