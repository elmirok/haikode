#ifndef HAIKODE_CORE_OAUTH_HTTP_CLIENT_H
#define HAIKODE_CORE_OAUTH_HTTP_CLIENT_H

#include "core/OAuthClient.h"

#include <string>

class OAuthHttpClient {
public:
	bool ExchangeCode(const ProviderSettings& settings, const std::string& code,
		const std::string& verifier, OAuthToken& token, std::string& error) const;
};

#endif

