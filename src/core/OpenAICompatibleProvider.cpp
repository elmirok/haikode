#include "core/AiProvider.h"

#include "core/ChatClient.h"

#include <cstdlib>

std::string
OpenAICompatibleProvider::ReadApiKeyFromEnvironment()
{
	const char* key = getenv("HAIKODE_API_KEY");
	if (key != nullptr && key[0] != '\0')
		return key;
	key = getenv("OPENAI_API_KEY");
	if (key != nullptr && key[0] != '\0')
		return key;
	return std::string();
}

bool
OpenAICompatibleProvider::Send(const ProviderSettings& settings,
	const ChatRequest& request, std::string& responseText,
	std::string& error) const
{
	ChatClient client;
	return client.SendWithApiKey(settings, ReadApiKeyFromEnvironment(), request,
		responseText, error);
}
