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

	std::cout << "oauth-pkce-smoke-ok\n";
	return 0;
}
