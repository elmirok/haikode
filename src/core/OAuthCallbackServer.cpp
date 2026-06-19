#include "core/OAuthCallbackServer.h"

#include <arpa/inet.h>
#include <cstdlib>
#include <cstring>
#include <netinet/in.h>
#include <sstream>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>

namespace {

std::string
urlDecode(const std::string& value)
{
	std::string decoded;
	for (size_t i = 0; i < value.size(); ++i) {
		if (value[i] == '%' && i + 2 < value.size()) {
			const std::string hex = value.substr(i + 1, 2);
			char* end = nullptr;
			const long byte = strtol(hex.c_str(), &end, 16);
			if (end != nullptr && *end == '\0') {
				decoded.push_back(static_cast<char>(byte));
				i += 2;
				continue;
			}
		}
		if (value[i] == '+')
			decoded.push_back(' ');
		else
			decoded.push_back(value[i]);
	}
	return decoded;
}

std::string
queryValue(const std::string& request, const std::string& key)
{
	const std::string marker = key + "=";
	const size_t start = request.find(marker);
	if (start == std::string::npos)
		return std::string();
	size_t valueStart = start + marker.size();
	size_t valueEnd = request.find_first_of(" &\r\n", valueStart);
	if (valueEnd == std::string::npos)
		valueEnd = request.size();
	return urlDecode(request.substr(valueStart, valueEnd - valueStart));
}

void
sendResponse(int client, const std::string& body)
{
	std::ostringstream response;
	response << "HTTP/1.1 200 OK\r\n"
		<< "Content-Type: text/html\r\n"
		<< "Connection: close\r\n"
		<< "Content-Length: " << body.size() << "\r\n\r\n"
		<< body;
	const std::string text = response.str();
	send(client, text.c_str(), text.size(), 0);
}

}

bool
OAuthCallbackServer::WaitForCode(int port, const std::string& expectedState,
	std::string& code, std::string& error) const
{
	code.clear();
	error.clear();

	const int server = socket(AF_INET, SOCK_STREAM, 0);
	if (server < 0) {
		error = "Could not create OAuth callback socket.";
		return false;
	}

	int yes = 1;
	setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &yes, sizeof(yes));

	sockaddr_in address{};
	address.sin_family = AF_INET;
	address.sin_port = htons(static_cast<uint16_t>(port));
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);

	if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address)) != 0
		|| listen(server, 1) != 0) {
		close(server);
		error = "Could not start OAuth localhost callback listener.";
		return false;
	}

	fd_set readSet;
	FD_ZERO(&readSet);
	FD_SET(server, &readSet);
	timeval timeout{};
	timeout.tv_sec = 180;

	const int selected = select(server + 1, &readSet, nullptr, nullptr, &timeout);
	if (selected <= 0) {
		close(server);
		error = "Timed out waiting for OAuth callback.";
		return false;
	}

	const int client = accept(server, nullptr, nullptr);
	close(server);
	if (client < 0) {
		error = "Could not accept OAuth callback.";
		return false;
	}

	char buffer[4096];
	const ssize_t bytes = recv(client, buffer, sizeof(buffer) - 1, 0);
	if (bytes <= 0) {
		close(client);
		error = "OAuth callback was empty.";
		return false;
	}
	buffer[bytes] = '\0';

	const std::string request(buffer);
	const std::string state = queryValue(request, "state");
	code = queryValue(request, "code");

	if (state != expectedState || code.empty()) {
		sendResponse(client, "<html><body>Haikode OAuth failed.</body></html>");
		close(client);
		error = "OAuth callback state did not match or code was missing.";
		return false;
	}

	sendResponse(client,
		"<html><body>Haikode login complete. You can return to Haikode.</body></html>");
	close(client);
	return true;
}
