#include "core/OAuthClient.h"

#include "core/Crypto.h"

#include <iomanip>
#include <sstream>

namespace {

std::string
urlEncode(const std::string& value)
{
	std::ostringstream out;
	out << std::hex << std::uppercase << std::setfill('0');
	for (unsigned char c : value) {
		if ((c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
			|| (c >= '0' && c <= '9') || c == '-' || c == '_' || c == '.'
			|| c == '~') {
			out << c;
		} else {
			out << '%' << std::setw(2) << static_cast<int>(c);
		}
	}
	return out.str();
}

}

std::string
OAuthClient::GenerateCodeVerifier()
{
	return Crypto::RandomBase64Url(32);
}

std::string
OAuthClient::BuildCodeChallenge(const std::string& verifier)
{
	return Crypto::Base64UrlEncode(Crypto::Sha256Bytes(verifier));
}

std::string
OAuthClient::BuildAuthorizationUrl(const ProviderSettings& settings,
	const std::string& verifier, const std::string& state)
{
	std::ostringstream url;
	url << settings.authUrl
		<< "?response_type=code"
		<< "&client_id=" << urlEncode(settings.clientId)
		<< "&redirect_uri=" << urlEncode(RedirectUri(settings))
		<< "&scope=" << urlEncode(settings.scope)
		<< "&state=" << urlEncode(state)
		<< "&code_challenge=" << urlEncode(BuildCodeChallenge(verifier))
		<< "&code_challenge_method=S256";
	return url.str();
}

std::string
OAuthClient::RedirectUri(const ProviderSettings& settings)
{
	std::ostringstream uri;
	uri << "http://127.0.0.1:" << settings.redirectPort << "/oauth/callback";
	return uri.str();
}

