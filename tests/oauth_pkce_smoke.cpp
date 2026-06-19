/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "ai/OAuthClient.h"

#include <cassert>
#include <iostream>

int
main()
{
	const std::string verifier
		= "dBjftJeZ4CVP-mB92K27uhbUJU1p1r_wW1gFWFOEjXk";
	assert(Haikode::AI::OAuthClient::IsValidVerifier(verifier));
	assert(Haikode::AI::OAuthClient::CodeChallenge(verifier)
		== "E9Melhoa2OwvFrEMTJguCHaoeK1t8URWbuGJSstw-cM");
	assert(!Haikode::AI::OAuthClient::IsValidVerifier("too-short"));
	assert(!Haikode::AI::OAuthClient::IsValidVerifier(
		"contains spaces and symbols!"));

	Haikode::AI::OAuthSettings settings;
	settings.authUrl = "https://provider.example/oauth/authorize";
	settings.tokenUrl = "https://provider.example/oauth/token";
	settings.clientId = "haikode desktop";
	settings.scope = "openid profile offline_access";
	settings.redirectUri = "http://127.0.0.1:8765/callback";

	const std::string authUrl = Haikode::AI::OAuthClient::BuildAuthUrl(
		settings, verifier, "state value");
	assert(authUrl.find("client_id=haikode%20desktop") != std::string::npos);
	assert(authUrl.find("redirect_uri=http%3A%2F%2F127.0.0.1%3A8765%2Fcallback")
		!= std::string::npos);
	assert(authUrl.find("scope=openid%20profile%20offline_access")
		!= std::string::npos);
	assert(authUrl.find("code_challenge_method=S256") != std::string::npos);
	assert(authUrl.find("state=state%20value") != std::string::npos);

	const Haikode::AI::OAuthTokenRequest tokenRequest
		= Haikode::AI::OAuthClient::PrepareTokenRequest(settings,
			"code value", verifier);
	assert(tokenRequest.url == settings.tokenUrl);
	assert(tokenRequest.body.find("grant_type=authorization_code")
		!= std::string::npos);
	assert(tokenRequest.body.find("client_id=haikode%20desktop")
		!= std::string::npos);
	assert(tokenRequest.body.find("code=code%20value") != std::string::npos);
	assert(tokenRequest.body.find("code_verifier=" + verifier)
		!= std::string::npos);

	std::string token;
	std::string error;
	assert(Haikode::AI::OAuthClient::ExtractAccessToken(
		"{\"access_token\":\"oauth-token\",\"token_type\":\"Bearer\"}",
		token, error));
	assert(token == "oauth-token");
	assert(!Haikode::AI::OAuthClient::ExtractAccessToken(
		"{\"error\":\"invalid_grant\",\"error_description\":\"bad code\"}",
		token, error));
	assert(error.find("invalid_grant") != std::string::npos);

	std::string code;
	assert(Haikode::AI::OAuthClient::ExtractAuthorizationCode(
		"http://127.0.0.1:8765/callback?state=haikode&code=code%20value",
		code, error));
	assert(code == "code value");
	assert(Haikode::AI::OAuthClient::ExtractAuthorizationCode(
		"/callback?state=haikode&code=local%20callback", code, error));
	assert(code == "local callback");
	assert(Haikode::AI::OAuthClient::ExtractAuthorizationCode(
		"http://127.0.0.1:8765/callback#code=fragment%20code", code, error));
	assert(code == "fragment code");
	assert(Haikode::AI::OAuthClient::ExtractAuthorizationCode(
		"plain-code", code, error));
	assert(code == "plain-code");
	assert(!Haikode::AI::OAuthClient::ExtractAuthorizationCode(
		"http://127.0.0.1:8765/callback?error=access_denied"
		"&error_description=Not%20allowed",
		code, error));
	assert(error.find("access_denied") != std::string::npos);

	Haikode::AI::CancellationToken cancellation;
	cancellation.Cancel();
	Haikode::AI::OAuthClient client;
	Haikode::AI::OAuthTokenResponse cancelledResponse;
	assert(!client.ExchangeCode(settings, "code value", verifier,
		cancelledResponse, error, &cancellation));
	assert(error.find("cancelled") != std::string::npos);

	std::cout << "oauth-pkce-smoke-ok\n";
	return 0;
}
