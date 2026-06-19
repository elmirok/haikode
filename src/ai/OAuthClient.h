/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include <string>

namespace Haikode::AI {

struct OAuthSettings {
	std::string authUrl;
	std::string tokenUrl;
	std::string clientId;
	std::string scope;
	std::string redirectUri;
};

struct OAuthTokenRequest {
	std::string url;
	std::string body;
};

struct OAuthTokenResponse {
	std::string accessToken;
	std::string rawBody;
	long httpStatus = 0;
};

class OAuthClient {
public:
	static std::string GenerateVerifier();
	static bool IsValidVerifier(const std::string& verifier);
	static std::string CodeChallenge(const std::string& verifier);
	static std::string BuildAuthUrl(const OAuthSettings& settings,
		const std::string& verifier, const std::string& state);
	static OAuthTokenRequest PrepareTokenRequest(const OAuthSettings& settings,
		const std::string& code, const std::string& verifier);
	static bool ExtractAccessToken(const std::string& body, std::string& token,
		std::string& error);
	static bool ExtractAuthorizationCode(const std::string& pastedValue,
		std::string& code, std::string& error);

	bool ExchangeCode(const OAuthSettings& settings, const std::string& code,
		const std::string& verifier, OAuthTokenResponse& response,
		std::string& error) const;
};

} // namespace Haikode::AI
