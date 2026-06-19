#ifndef HAIKODE_CORE_SETTINGS_STORE_H
#define HAIKODE_CORE_SETTINGS_STORE_H

#include "core/OAuthClient.h"

#include <string>

class SettingsStore {
public:
	SettingsStore();

	const std::string& DirectoryPath() const { return fDirectoryPath; }
	std::string ProviderPath() const;
	std::string TokenPath() const;
	std::string CodexPath() const;
	std::string ApiKeyPath() const;

	bool EnsureDirectory(std::string& error) const;
	bool LoadProvider(ProviderSettings& settings) const;
	bool SaveProvider(const ProviderSettings& settings, std::string& error) const;
	bool LoadToken(OAuthToken& token) const;
	bool SaveToken(const OAuthToken& token, std::string& error) const;
	bool LoadCodexPath(std::string& codexPath) const;
	bool SaveCodexPath(const std::string& codexPath, std::string& error) const;
	bool LoadApiKey(std::string& apiKey) const;
	bool SaveApiKey(const std::string& apiKey, std::string& error) const;

private:
	std::string fDirectoryPath;
};

#endif
