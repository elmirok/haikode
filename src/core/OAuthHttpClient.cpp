#include "core/OAuthHttpClient.h"

#include <curl/curl.h>

#if __has_include(<json/json.h>)
#include <json/json.h>
#else
#include <jsoncpp/json/json.h>
#endif

#include <ctime>
#include <sstream>

namespace {

size_t
writeCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
	std::string* body = static_cast<std::string*>(userdata);
	body->append(ptr, size * nmemb);
	return size * nmemb;
}

std::string
escape(CURL* curl, const std::string& value)
{
	char* escaped = curl_easy_escape(curl, value.c_str(),
		static_cast<int>(value.size()));
	if (escaped == nullptr)
		return std::string();
	std::string out(escaped);
	curl_free(escaped);
	return out;
}

}

bool
OAuthHttpClient::ExchangeCode(const ProviderSettings& settings,
	const std::string& code, const std::string& verifier, OAuthToken& token,
	std::string& error) const
{
	error.clear();
	CURL* curl = curl_easy_init();
	if (curl == nullptr) {
		error = "Could not initialize HTTP client.";
		return false;
	}

	std::ostringstream post;
	post << "grant_type=authorization_code"
		<< "&code=" << escape(curl, code)
		<< "&redirect_uri=" << escape(curl, OAuthClient::RedirectUri(settings))
		<< "&client_id=" << escape(curl, settings.clientId)
		<< "&code_verifier=" << escape(curl, verifier);

	std::string response;
	curl_easy_setopt(curl, CURLOPT_URL, settings.tokenUrl.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, post.str().c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Haikode/0.1");

	struct curl_slist* headers = nullptr;
	headers = curl_slist_append(headers,
		"Content-Type: application/x-www-form-urlencoded");
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);

	const CURLcode result = curl_easy_perform(curl);
	long status = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	if (result != CURLE_OK || status < 200 || status >= 300) {
		error = "OAuth token exchange failed.";
		return false;
	}

	Json::Value root;
	Json::CharReaderBuilder builder;
	std::string parseError;
	std::istringstream in(response);
	if (!Json::parseFromStream(builder, in, &root, &parseError)) {
		error = "OAuth token response was not valid JSON.";
		return false;
	}

	token.accessToken = root.get("access_token", "").asString();
	token.refreshToken = root.get("refresh_token", "").asString();
	const int expiresIn = root.get("expires_in", 0).asInt();
	token.expiresAt = expiresIn > 0 ? static_cast<long long>(time(nullptr)) + expiresIn : 0;

	if (!token.HasAccessToken()) {
		error = "OAuth token response did not include an access token.";
		return false;
	}

	return true;
}

