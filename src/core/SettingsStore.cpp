#include "core/SettingsStore.h"

#include <cerrno>
#include <cstdlib>
#include <fstream>
#include <map>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>
#include <vector>

namespace {

std::string
homeSettingsDirectory()
{
	const char* home = getenv("HOME");
	if (home == nullptr || home[0] == '\0')
		return "Haikode";
	return std::string(home) + "/config/settings/Haikode";
}

bool
mkdirIfNeeded(const std::string& path, mode_t mode)
{
	if (path.empty())
		return false;
	struct stat info;
	if (stat(path.c_str(), &info) == 0)
		return S_ISDIR(info.st_mode);
	if (mkdir(path.c_str(), mode) != 0 && errno != EEXIST)
		return false;
	return true;
}

bool
readKeyValues(const std::string& path, std::map<std::string, std::string>& values)
{
	values.clear();
	std::ifstream file(path);
	if (!file)
		return false;

	std::string line;
	while (std::getline(file, line)) {
		const size_t eq = line.find('=');
		if (eq == std::string::npos)
			continue;
		values[line.substr(0, eq)] = line.substr(eq + 1);
	}
	return true;
}

bool
writeTextFilePrivate(const std::string& path, const std::string& text,
	std::string& error)
{
	std::ofstream file(path, std::ios::trunc);
	if (!file) {
		error = "Could not open settings file for writing.";
		return false;
	}
	file << text;
	file.close();
	chmod(path.c_str(), S_IRUSR | S_IWUSR);
	return true;
}

}

SettingsStore::SettingsStore()
	:
	fDirectoryPath(homeSettingsDirectory())
{
}

std::string
SettingsStore::ProviderPath() const
{
	return fDirectoryPath + "/provider.conf";
}

std::string
SettingsStore::TokenPath() const
{
	return fDirectoryPath + "/token.conf";
}

std::string
SettingsStore::CodexPath() const
{
	return fDirectoryPath + "/codex.conf";
}

std::string
SettingsStore::ApiKeyPath() const
{
	return fDirectoryPath + "/api_key.conf";
}

bool
SettingsStore::EnsureDirectory(std::string& error) const
{
	std::string current;
	if (!fDirectoryPath.empty() && fDirectoryPath[0] == '/')
		current = "/";

	std::istringstream parts(fDirectoryPath);
	std::string part;
	while (std::getline(parts, part, '/')) {
		if (part.empty())
			continue;
		if (current.empty() || current == "/")
			current += part;
		else
			current += "/" + part;
		const bool isFinal = current == fDirectoryPath;
		if (!mkdirIfNeeded(current, isFinal ? S_IRWXU : S_IRWXU | S_IRGRP
			| S_IXGRP | S_IROTH | S_IXOTH)) {
			error = "Could not create Haikode settings directory.";
			return false;
		}
		if (isFinal)
			chmod(current.c_str(), S_IRWXU);
	}

	if (current.empty()) {
		error = "Could not create Haikode settings directory.";
		return false;
	}
	return true;
}

bool
SettingsStore::LoadProvider(ProviderSettings& settings) const
{
	std::map<std::string, std::string> values;
	if (!readKeyValues(ProviderPath(), values))
		return false;

	if (values.count("base_url") != 0)
		settings.baseUrl = values["base_url"];
	if (values.count("model") != 0)
		settings.model = values["model"];
	if (values.count("auth_url") != 0)
		settings.authUrl = values["auth_url"];
	if (values.count("token_url") != 0)
		settings.tokenUrl = values["token_url"];
	if (values.count("client_id") != 0)
		settings.clientId = values["client_id"];
	if (values.count("scope") != 0)
		settings.scope = values["scope"];
	if (values.count("redirect_port") != 0)
		settings.redirectPort = std::atoi(values["redirect_port"].c_str());
	return true;
}

bool
SettingsStore::SaveProvider(const ProviderSettings& settings, std::string& error) const
{
	if (!EnsureDirectory(error))
		return false;

	std::ostringstream text;
	text << "base_url=" << settings.baseUrl << "\n"
		<< "model=" << settings.model << "\n"
		<< "auth_url=" << settings.authUrl << "\n"
		<< "token_url=" << settings.tokenUrl << "\n"
		<< "client_id=" << settings.clientId << "\n"
		<< "scope=" << settings.scope << "\n"
		<< "redirect_port=" << settings.redirectPort << "\n";
	return writeTextFilePrivate(ProviderPath(), text.str(), error);
}

bool
SettingsStore::LoadToken(OAuthToken& token) const
{
	std::map<std::string, std::string> values;
	if (!readKeyValues(TokenPath(), values))
		return false;

	token.accessToken = values["access_token"];
	token.refreshToken = values["refresh_token"];
	token.expiresAt = std::atoll(values["expires_at"].c_str());
	return token.HasAccessToken();
}

bool
SettingsStore::SaveToken(const OAuthToken& token, std::string& error) const
{
	if (!EnsureDirectory(error))
		return false;

	std::ostringstream text;
	text << "access_token=" << token.accessToken << "\n"
		<< "refresh_token=" << token.refreshToken << "\n"
		<< "expires_at=" << token.expiresAt << "\n";
	return writeTextFilePrivate(TokenPath(), text.str(), error);
}

bool
SettingsStore::LoadCodexPath(std::string& codexPath) const
{
	std::map<std::string, std::string> values;
	if (!readKeyValues(CodexPath(), values))
		return false;

	codexPath = values["codex_path"];
	if (codexPath.empty())
		codexPath = "codex";
	return true;
}

bool
SettingsStore::SaveCodexPath(const std::string& codexPath,
	std::string& error) const
{
	if (!EnsureDirectory(error))
		return false;

	std::ostringstream text;
	text << "codex_path=" << (codexPath.empty() ? "codex" : codexPath) << "\n";
	return writeTextFilePrivate(CodexPath(), text.str(), error);
}

bool
SettingsStore::LoadApiKey(std::string& apiKey) const
{
	std::map<std::string, std::string> values;
	if (!readKeyValues(ApiKeyPath(), values))
		return false;

	apiKey = values["api_key"];
	return !apiKey.empty();
}

bool
SettingsStore::SaveApiKey(const std::string& apiKey, std::string& error) const
{
	if (!EnsureDirectory(error))
		return false;

	std::ostringstream text;
	text << "api_key=" << apiKey << "\n";
	return writeTextFilePrivate(ApiKeyPath(), text.str(), error);
}
