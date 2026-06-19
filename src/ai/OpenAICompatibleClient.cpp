/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "OpenAICompatibleClient.h"

#include <cctype>
#include <sstream>

#ifdef HAIKODE_AI_NETWORK
#include <curl/curl.h>
#endif

namespace Haikode::AI {

namespace {

std::string
EscapeJson(const std::string& value)
{
	std::ostringstream out;
	for (unsigned char c : value) {
		switch (c) {
			case '\\':
				out << "\\\\";
				break;
			case '"':
				out << "\\\"";
				break;
			case '\n':
				out << "\\n";
				break;
			case '\r':
				out << "\\r";
				break;
			case '\t':
				out << "\\t";
				break;
			default:
				if (c < 0x20) {
					out << "\\u00";
					const char* hex = "0123456789abcdef";
					out << hex[(c >> 4) & 0xf] << hex[c & 0xf];
				} else {
					out << static_cast<char>(c);
				}
				break;
		}
	}
	return out.str();
}

std::string
UnescapeJsonString(const std::string& value)
{
	std::ostringstream out;
	for (size_t i = 0; i < value.size(); ++i) {
		if (value[i] != '\\' || i + 1 >= value.size()) {
			out << value[i];
			continue;
		}

		const char escaped = value[++i];
		switch (escaped) {
			case 'n':
				out << '\n';
				break;
			case 'r':
				out << '\r';
				break;
			case 't':
				out << '\t';
				break;
			case '"':
			case '\\':
			case '/':
				out << escaped;
				break;
			default:
				out << escaped;
				break;
		}
	}
	return out.str();
}

bool
ExtractJsonStringField(const std::string& body, const std::string& field,
	std::string& value)
{
	size_t pos = body.find("\"" + field + "\"");
	if (pos == std::string::npos)
		return false;
	pos += field.size() + 2;

	while (pos < body.size()
		&& std::isspace(static_cast<unsigned char>(body[pos]))) {
		++pos;
	}
	if (pos >= body.size() || body[pos] != ':')
		return false;
	++pos;
	while (pos < body.size()
		&& std::isspace(static_cast<unsigned char>(body[pos]))) {
		++pos;
	}
	if (pos >= body.size() || body[pos] != '"')
		return false;
	++pos;

	std::ostringstream raw;
	bool escaping = false;
	for (; pos < body.size(); ++pos) {
		const char c = body[pos];
		if (escaping) {
			raw << '\\' << c;
			escaping = false;
			continue;
		}
		if (c == '\\') {
			escaping = true;
			continue;
		}
		if (c == '"') {
			value = UnescapeJsonString(raw.str());
			return true;
		}
		raw << c;
	}
	return false;
}

#ifdef HAIKODE_AI_NETWORK
size_t
WriteCallback(char* ptr, size_t size, size_t nmemb, void* userdata)
{
	std::string* body = static_cast<std::string*>(userdata);
	body->append(ptr, size * nmemb);
	return size * nmemb;
}
#endif

} // namespace

PreparedChatRequest
OpenAICompatibleClient::Prepare(const ProviderSettings& provider,
	const ChatRequest& request)
{
	PreparedChatRequest prepared;
	prepared.url = provider.ChatCompletionsEndpoint();

	std::ostringstream body;
	body
		<< "{\"model\":\"" << EscapeJson(provider.model) << "\","
		<< "\"messages\":[{\"role\":\"user\",\"content\":\""
		<< EscapeJson(request.prompt) << "\"}]}";
	prepared.body = body.str();

	if (provider.authMode == AuthMode::ApiKey && !provider.apiKey.empty())
		prepared.authorizationHeader = "Authorization: Bearer " + provider.apiKey;
	else if (provider.authMode == AuthMode::OAuth && !provider.oauthToken.empty())
		prepared.authorizationHeader = "Authorization: Bearer " + provider.oauthToken;

	return prepared;
}


bool
OpenAICompatibleClient::ExtractResponseText(const std::string& body,
	std::string& text, std::string& error)
{
	text.clear();
	error.clear();

	if (ExtractJsonStringField(body, "content", text))
		return true;

	if (ExtractJsonStringField(body, "message", error))
		return false;

	error = "AI response did not include choices[0].message.content.";
	return false;
}


bool
OpenAICompatibleClient::Send(const ProviderSettings& provider,
	const ChatRequest& request, ChatResponse& response, std::string& error) const
{
	response = ChatResponse();
	error.clear();

	if (!provider.Validate(error)) {
		return false;
	}

#ifndef HAIKODE_AI_NETWORK
	(void)request;
	error = "Haikode was built without AI network support.";
	return false;
#else
	const PreparedChatRequest prepared = Prepare(provider, request);

	CURL* curl = curl_easy_init();
	if (curl == nullptr) {
		error = "Could not initialize curl.";
		return false;
	}

	std::string body;
	struct curl_slist* headers = nullptr;
	headers = curl_slist_append(headers, "Content-Type: application/json");
	if (!prepared.authorizationHeader.empty())
		headers = curl_slist_append(headers, prepared.authorizationHeader.c_str());

	curl_easy_setopt(curl, CURLOPT_URL, prepared.url.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, prepared.body.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Haikode/0.1");

	const CURLcode result = curl_easy_perform(curl);
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.httpStatus);
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	response.rawBody = body;

	if (result != CURLE_OK) {
		error = curl_easy_strerror(result);
		return false;
	}

	if (response.httpStatus < 200 || response.httpStatus >= 300) {
		std::ostringstream message;
		message << "AI request failed with HTTP status " << response.httpStatus
			<< ".";
		error = message.str();
		return false;
	}

	return ExtractResponseText(body, response.text, error);
#endif
}

} // namespace Haikode::AI
