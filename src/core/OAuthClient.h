#ifndef HAIKODE_CORE_OAUTH_CLIENT_H
#define HAIKODE_CORE_OAUTH_CLIENT_H

#include <string>

struct ProviderSettings {
	std::string baseUrl = "https://api.openai.com";
	std::string model = "gpt-4.1-mini";
	std::string authUrl;
	std::string tokenUrl;
	std::string clientId;
	std::string scope = "openid profile email";
	int redirectPort = 17654;
};

struct OAuthToken {
	std::string accessToken;
	std::string refreshToken;
	long long expiresAt = 0;

	bool HasAccessToken() const { return !accessToken.empty(); }
};

class OAuthClient {
public:
	static std::string GenerateCodeVerifier();
	static std::string BuildCodeChallenge(const std::string& verifier);
	static std::string BuildAuthorizationUrl(const ProviderSettings& settings,
		const std::string& verifier, const std::string& state);
	static std::string RedirectUri(const ProviderSettings& settings);
};

#endif

