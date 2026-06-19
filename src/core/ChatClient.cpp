#include "core/ChatClient.h"

#include <curl/curl.h>

#if __has_include(<json/json.h>)
#include <json/json.h>
#else
#include <jsoncpp/json/json.h>
#endif

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
endpointFor(const std::string& baseUrl)
{
	if (baseUrl.size() >= 1 && baseUrl[baseUrl.size() - 1] == '/')
		return baseUrl.substr(0, baseUrl.size() - 1) + "/v1/chat/completions";
	return baseUrl + "/v1/chat/completions";
}

}

bool
ChatClient::Send(const ProviderSettings& settings, const OAuthToken& token,
	const ChatRequest& request, std::string& responseText, std::string& error) const
{
	if (!token.HasAccessToken()) {
		error = "Not logged in.";
		return false;
	}
	return SendWithApiKey(settings, token.accessToken, request, responseText,
		error);
}

bool
ChatClient::SendWithApiKey(const ProviderSettings& settings,
	const std::string& apiKey, const ChatRequest& request,
	std::string& responseText, std::string& error) const
{
	responseText.clear();
	error.clear();

	if (apiKey.empty()) {
		error = "Missing API key. Set HAIKODE_API_KEY or OPENAI_API_KEY.";
		return false;
	}

	CURL* curl = curl_easy_init();
	if (curl == nullptr) {
		error = "Could not initialize HTTP client.";
		return false;
	}

	const std::string body = request.ToChatCompletionsJson();
	const std::string auth = "Authorization: Bearer " + apiKey;
	std::string response;

	struct curl_slist* headers = nullptr;
	headers = curl_slist_append(headers, "Content-Type: application/json");
	headers = curl_slist_append(headers, auth.c_str());

	curl_easy_setopt(curl, CURLOPT_URL, endpointFor(settings.baseUrl).c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, body.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, writeCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &response);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Haikode/0.1");

	const CURLcode result = curl_easy_perform(curl);
	long status = 0;
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &status);
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	if (result != CURLE_OK || status < 200 || status >= 300) {
		std::ostringstream message;
		message << "AI request failed with HTTP status " << status << ".";
		error = message.str();
		return false;
	}

	Json::Value root;
	Json::CharReaderBuilder builder;
	std::string parseError;
	std::istringstream in(response);
	if (!Json::parseFromStream(builder, in, &root, &parseError)) {
		error = "AI response was not valid JSON.";
		return false;
	}

	responseText = root["choices"][0]["message"]["content"].asString();
	if (responseText.empty()) {
		error = "AI response did not include message content.";
		return false;
	}

	return true;
}
