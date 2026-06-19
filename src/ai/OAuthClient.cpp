/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "OAuthClient.h"

#include <openssl/evp.h>

#include <array>
#include <cctype>
#include <iomanip>
#include <random>
#include <sstream>

#ifdef HAIKODE_AI_NETWORK
#include <curl/curl.h>
#endif

namespace Haikode::AI {

namespace {

bool
IsVerifierChar(char c)
{
	return std::isalnum(static_cast<unsigned char>(c)) || c == '-'
		|| c == '.' || c == '_' || c == '~';
}


std::string
UrlEncode(const std::string& value)
{
	std::ostringstream out;
	out << std::uppercase << std::hex;
	for (const unsigned char c : value) {
		if (std::isalnum(c) || c == '-' || c == '.' || c == '_' || c == '~')
			out << static_cast<char>(c);
		else
			out << '%' << std::setw(2) << std::setfill('0') << int(c);
	}
	return out.str();
}


int
HexValue(char c)
{
	if (c >= '0' && c <= '9')
		return c - '0';
	if (c >= 'a' && c <= 'f')
		return 10 + c - 'a';
	if (c >= 'A' && c <= 'F')
		return 10 + c - 'A';
	return -1;
}


std::string
UrlDecode(const std::string& value)
{
	std::ostringstream out;
	for (size_t i = 0; i < value.size(); i++) {
		if (value[i] == '+') {
			out << ' ';
			continue;
		}
		if (value[i] == '%' && i + 2 < value.size()) {
			const int high = HexValue(value[i + 1]);
			const int low = HexValue(value[i + 2]);
			if (high >= 0 && low >= 0) {
				out << static_cast<char>((high << 4) | low);
				i += 2;
				continue;
			}
		}
		out << value[i];
	}
	return out.str();
}


bool
FindQueryValue(const std::string& input, const std::string& key,
	std::string& value)
{
	size_t start = input.find('?');
	if (start == std::string::npos)
		start = input.find('#');
	if (start == std::string::npos)
		return false;
	start++;

	while (start <= input.size()) {
		const size_t end = input.find_first_of("&#", start);
		const std::string pair = input.substr(start,
			end == std::string::npos ? std::string::npos : end - start);
		const size_t equals = pair.find('=');
		const std::string name = UrlDecode(
			equals == std::string::npos ? pair : pair.substr(0, equals));
		if (name == key) {
			value = UrlDecode(equals == std::string::npos ? ""
				: pair.substr(equals + 1));
			return true;
		}
		if (end == std::string::npos || input[end] == '#')
			break;
		start = end + 1;
	}
	return false;
}


std::string
TrimWhitespace(std::string value)
{
	size_t start = 0;
	while (start < value.size()
		&& std::isspace(static_cast<unsigned char>(value[start]))) {
		start++;
	}
	size_t end = value.size();
	while (end > start
		&& std::isspace(static_cast<unsigned char>(value[end - 1]))) {
		end--;
	}
	return value.substr(start, end - start);
}


std::string
ToLower(std::string value)
{
	for (char& c : value)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return value;
}


bool
StartsWith(const std::string& value, const std::string& prefix)
{
	return value.rfind(prefix, 0) == 0;
}


bool
IsHttpUrl(const std::string& value)
{
	const std::string lower = ToLower(TrimWhitespace(value));
	return StartsWith(lower, "http://") || StartsWith(lower, "https://");
}


bool
IsLoopbackHttpRedirectUri(const std::string& value)
{
	const std::string lower = ToLower(TrimWhitespace(value));
	return StartsWith(lower, "http://127.0.0.1/")
		|| StartsWith(lower, "http://127.0.0.1:")
		|| StartsWith(lower, "http://localhost/")
		|| StartsWith(lower, "http://localhost:");
}


std::string
Base64UrlEncode(const unsigned char* data, size_t length)
{
	const int encodedLength = 4 * ((int(length) + 2) / 3);
	std::string encoded(encodedLength, '\0');
	const int actualLength = EVP_EncodeBlock(
		reinterpret_cast<unsigned char*>(&encoded[0]), data, int(length));
	encoded.resize(actualLength);

	for (char& c : encoded) {
		if (c == '+')
			c = '-';
		else if (c == '/')
			c = '_';
	}
	while (!encoded.empty() && encoded[encoded.size() - 1] == '=')
		encoded.pop_back();
	return encoded;
}


std::string
UnescapeJsonString(const std::string& value)
{
	std::ostringstream out;
	for (size_t i = 0; i < value.size(); i++) {
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
		pos++;
	}
	if (pos >= body.size() || body[pos] != ':')
		return false;
	pos++;
	while (pos < body.size()
		&& std::isspace(static_cast<unsigned char>(body[pos]))) {
		pos++;
	}
	if (pos >= body.size() || body[pos] != '"')
		return false;
	pos++;

	std::ostringstream raw;
	bool escaping = false;
	for (; pos < body.size(); pos++) {
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


int
ProgressCallback(void* userdata, curl_off_t, curl_off_t, curl_off_t, curl_off_t)
{
	const CancellationToken* cancellation
		= static_cast<const CancellationToken*>(userdata);
	return cancellation != nullptr && cancellation->IsCancelled() ? 1 : 0;
}
#endif

} // namespace


std::string
OAuthClient::GenerateVerifier()
{
	static constexpr char kAlphabet[]
		= "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-._~";
	std::random_device random;
	std::uniform_int_distribution<size_t> distribution(0,
		sizeof(kAlphabet) - 2);

	std::string verifier;
	verifier.reserve(64);
	for (int i = 0; i < 64; i++)
		verifier.push_back(kAlphabet[distribution(random)]);
	return verifier;
}


bool
OAuthClient::IsValidVerifier(const std::string& verifier)
{
	if (verifier.size() < 43 || verifier.size() > 128)
		return false;
	for (const char c : verifier) {
		if (!IsVerifierChar(c))
			return false;
	}
	return true;
}


std::string
OAuthClient::CodeChallenge(const std::string& verifier)
{
	std::array<unsigned char, EVP_MAX_MD_SIZE> hash;
	unsigned int hashLength = 0;

	EVP_MD_CTX* context = EVP_MD_CTX_new();
	if (context == nullptr)
		return "";
	EVP_DigestInit_ex(context, EVP_sha256(), nullptr);
	EVP_DigestUpdate(context, verifier.data(), verifier.size());
	EVP_DigestFinal_ex(context, hash.data(), &hashLength);
	EVP_MD_CTX_free(context);

	return Base64UrlEncode(hash.data(), hashLength);
}


bool
OAuthClient::ValidateSettings(const OAuthSettings& settings, std::string& error)
{
	error.clear();
	if (TrimWhitespace(settings.authUrl).empty()) {
		error = "OAuth auth URL is required.";
		return false;
	}
	if (!IsHttpUrl(settings.authUrl)) {
		error = "OAuth auth URL must start with http:// or https://.";
		return false;
	}
	if (TrimWhitespace(settings.tokenUrl).empty()) {
		error = "OAuth token URL is required.";
		return false;
	}
	if (!IsHttpUrl(settings.tokenUrl)) {
		error = "OAuth token URL must start with http:// or https://.";
		return false;
	}
	if (TrimWhitespace(settings.clientId).empty()) {
		error = "OAuth client ID is required.";
		return false;
	}
	if (TrimWhitespace(settings.redirectUri).empty()) {
		error = "OAuth redirect URI is required.";
		return false;
	}
	if (!IsLoopbackHttpRedirectUri(settings.redirectUri)) {
		error = "OAuth redirect URI must be an http://127.0.0.1 or http://localhost callback.";
		return false;
	}
	return true;
}


std::string
OAuthClient::BuildAuthUrl(const OAuthSettings& settings,
	const std::string& verifier, const std::string& state)
{
	std::ostringstream url;
	url << settings.authUrl
		<< (settings.authUrl.find('?') == std::string::npos ? '?' : '&')
		<< "response_type=code"
		<< "&client_id=" << UrlEncode(settings.clientId)
		<< "&redirect_uri=" << UrlEncode(settings.redirectUri)
		<< "&scope=" << UrlEncode(settings.scope)
		<< "&code_challenge=" << UrlEncode(CodeChallenge(verifier))
		<< "&code_challenge_method=S256";
	if (!state.empty())
		url << "&state=" << UrlEncode(state);
	return url.str();
}


OAuthTokenRequest
OAuthClient::PrepareTokenRequest(const OAuthSettings& settings,
	const std::string& code, const std::string& verifier)
{
	OAuthTokenRequest request;
	request.url = settings.tokenUrl;
	std::ostringstream body;
	body << "grant_type=authorization_code"
		<< "&client_id=" << UrlEncode(settings.clientId)
		<< "&code=" << UrlEncode(code)
		<< "&redirect_uri=" << UrlEncode(settings.redirectUri)
		<< "&code_verifier=" << UrlEncode(verifier);
	request.body = body.str();
	return request;
}


bool
OAuthClient::ExtractAccessToken(const std::string& body, std::string& token,
	std::string& error)
{
	token.clear();
	error.clear();

	if (ExtractJsonStringField(body, "access_token", token))
		return true;

	std::string providerError;
	if (ExtractJsonStringField(body, "error", providerError)) {
		error = providerError;
		std::string description;
		if (ExtractJsonStringField(body, "error_description", description))
			error += ": " + description;
		return false;
	}

	error = "OAuth token response did not include access_token.";
	return false;
}


bool
OAuthClient::ExtractAuthorizationCode(const std::string& pastedValue,
	std::string& code, std::string& error)
{
	return ExtractAuthorizationCode(pastedValue, "", code, error);
}


bool
OAuthClient::ExtractAuthorizationCode(const std::string& pastedValue,
	const std::string& expectedState, std::string& code, std::string& error)
{
	code.clear();
	error.clear();

	const std::string value = pastedValue;
	if (value.empty()) {
		error = "OAuth authorization code is required.";
		return false;
	}

	std::string providerError;
	if (FindQueryValue(value, "error", providerError)) {
		error = providerError;
		std::string description;
		if (FindQueryValue(value, "error_description", description)
			&& !description.empty()) {
			error += ": " + description;
		}
		return false;
	}

	std::string state;
	if (!expectedState.empty() && FindQueryValue(value, "state", state)
		&& state != expectedState) {
		error = "OAuth callback state did not match the active login request.";
		return false;
	}

	if (FindQueryValue(value, "code", code)) {
		if (!code.empty())
			return true;
		error = "OAuth callback did not include an authorization code.";
		return false;
	}

	code = value;
	return true;
}


bool
OAuthClient::ExchangeCode(const OAuthSettings& settings, const std::string& code,
	const std::string& verifier, OAuthTokenResponse& response,
	std::string& error, const CancellationToken* cancellation) const
{
	response = OAuthTokenResponse();
	error.clear();

	if (cancellation != nullptr && cancellation->IsCancelled()) {
		error = "OAuth token request cancelled.";
		return false;
	}

	if (!ValidateSettings(settings, error))
		return false;
	if (code.empty()) {
		error = "OAuth authorization code is required.";
		return false;
	}
	if (!IsValidVerifier(verifier)) {
		error = "OAuth PKCE verifier is missing or invalid.";
		return false;
	}

#ifndef HAIKODE_AI_NETWORK
	error = "Haikode was built without AI network support.";
	return false;
#else
	const OAuthTokenRequest request = PrepareTokenRequest(settings, code,
		verifier);

	CURL* curl = curl_easy_init();
	if (curl == nullptr) {
		error = "Could not initialize curl.";
		return false;
	}

	std::string body;
	struct curl_slist* headers = nullptr;
	headers = curl_slist_append(headers,
		"Content-Type: application/x-www-form-urlencoded");

	curl_easy_setopt(curl, CURLOPT_URL, request.url.c_str());
	curl_easy_setopt(curl, CURLOPT_POSTFIELDS, request.body.c_str());
	curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WriteCallback);
	curl_easy_setopt(curl, CURLOPT_WRITEDATA, &body);
	curl_easy_setopt(curl, CURLOPT_HTTPHEADER, headers);
	curl_easy_setopt(curl, CURLOPT_USERAGENT, "Haikode/0.1");
	if (cancellation != nullptr) {
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 0L);
		curl_easy_setopt(curl, CURLOPT_XFERINFOFUNCTION, ProgressCallback);
		curl_easy_setopt(curl, CURLOPT_XFERINFODATA, cancellation);
	}

	const CURLcode result = curl_easy_perform(curl);
	curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response.httpStatus);
	curl_slist_free_all(headers);
	curl_easy_cleanup(curl);

	response.rawBody = body;

	if (result != CURLE_OK) {
		if (result == CURLE_ABORTED_BY_CALLBACK
			|| (cancellation != nullptr && cancellation->IsCancelled())) {
			error = "OAuth token request cancelled.";
		} else
			error = curl_easy_strerror(result);
		return false;
	}

	if (response.httpStatus < 200 || response.httpStatus >= 300) {
		std::ostringstream message;
		message << "OAuth token request failed with HTTP status "
			<< response.httpStatus << ".";
		std::string providerError;
		if (!body.empty()
			&& !ExtractAccessToken(body, response.accessToken, providerError)) {
			message << " " << providerError;
		}
		error = message.str();
		return false;
	}

	return ExtractAccessToken(body, response.accessToken, error);
#endif
}

} // namespace Haikode::AI
