#ifndef HAIKODE_CORE_OAUTH_CALLBACK_SERVER_H
#define HAIKODE_CORE_OAUTH_CALLBACK_SERVER_H

#include <string>

class OAuthCallbackServer {
public:
	bool WaitForCode(int port, const std::string& expectedState,
		std::string& code, std::string& error) const;
};

#endif

