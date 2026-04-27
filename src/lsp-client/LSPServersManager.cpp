/*
 * Copyright 2023-2026, Andrea Anzani
 * All rights reserved. Distributed under the terms of the MIT license.
 */


#include "LSPServersManager.h"

#include <Directory.h>
#include <PathFinder.h>

#include <string>
#include <vector>

#include <yaml-cpp/yaml.h>

#include "ConfigManager.h"
#include "GException.h"
#include "GenioApp.h"
#include "Log.h"
#include "LSPLogLevels.h"
#include "LSPProjectWrapper.h"
#include "Utils.h"



std::string
GetCLangLogLevel()
{
	std::string logLevel;
	switch ((int32)gCFG["lsp_clangd_log_level"]) {
		default:
		case LSP_LOG_LEVEL_ERROR:
			logLevel += "error"; // Error messages only
			break;
		case LSP_LOG_LEVEL_INFO:
			logLevel += "info"; // High level execution tracing
			break;
		case LSP_LOG_LEVEL_TRACE:
			logLevel += "verbose"; // Low level details
			break;
	};
	return logLevel;
}


std::string
GetBinaryFullpath(std::string binaryName)
{
	BStringList paths;
	BPath path;
	status_t status = BPathFinder::FindPaths(B_FIND_PATH_BIN_DIRECTORY, paths);
	if (status != B_OK)
		throw GException(status, ::strerror(status));

	for (int32 c = 0; c < paths.CountStrings(); c++) {
		BPath filePath = paths.StringAt(c).String();
		filePath.Append(binaryName.c_str());
		if (BEntry(filePath.Path()).Exists()) {
			path = filePath;
			break;
		}
	}
	return path.Path();
}


class YAMLServerConfig : public LSPServerConfigInterface
{
	public:
			static YAMLServerConfig* fromFile(BPath lspFile)
			{
				YAMLServerConfig* config = nullptr;
				try {
					const YAML::Node lsp = YAML::LoadFile(lspFile.Path());
					if (!lsp["name"] ||
						!lsp["command"] ||
						!lsp["fileTypes"])
							return nullptr;

					config = new YAMLServerConfig();
					std::string command = lsp["command"].as<std::string>();

					// if it's just a binary name (no path)
					// let's search it in the pin paths:
					BPath path(command.c_str());
					if (strcmp(path.Leaf(),command.c_str()) == 0) {
						command = GetBinaryFullpath(command);
						if (command.empty() == true) {
							return nullptr;
						}
					}

					config->fArgv.push_back(strdup(command.c_str()));
					for (const auto& ft : lsp["fileTypes"]) {
						config->fFileTypes.push_back(ft.as<std::string>());
					}
					// args
					if (lsp["args"]) {
						for (const auto& arg : lsp["args"]) {
							std::string argument = arg.as<std::string>();
							size_t startPos = argument.find("${");
							size_t endPos = argument.find("}", startPos);
							if (startPos != std::string::npos && endPos != std::string::npos) {

								  std::string token = argument.substr(startPos + 2, endPos - startPos - 2);
								  if (token == "lsp_clangd_log_level") {
									  std::string level = GetCLangLogLevel();
									  argument.replace(startPos, (endPos - startPos) + 1, level);
									  config->fArgv.push_back(strdup(argument.c_str()));
								  } else if (token == "genio_pid") {
										thread_id pid = find_thread(NULL);
										BString spid;
										spid << (int32)pid;
										config->fArgv.push_back(strdup(spid.String()));
								  } else {
									  // Handle other tokens
									  LogError("Unkown LSP config token : %s", token.c_str());
								  }
								  continue;
							}

							config->fArgv.push_back(strdup(arg.as<std::string>().c_str()));
						}
					}
				} catch (const YAML::Exception & e)  {
					if (config != nullptr){
						delete config;
						config = nullptr;
					}
					LogError("Error reading %s (%s)\n", lspFile.Path(), e.msg.c_str());
				}
				return config;
			}

		const bool IsFileTypeSupported (const BString& fileType) const override {
			 return
				(std::find(fFileTypes.begin(), fFileTypes.end(), fileType.String()) != fFileTypes.end());
		}
	private:
			YAMLServerConfig(){}

			std::vector<std::string>	fFileTypes;
};


std::vector<LSPServerConfigInterface*> LSPServersManager::sConfigs;


/* static */
bool
LSPServersManager::_AddValidConfig(LSPServerConfigInterface* interface)
{
	if (interface->Argc() > 0 && BEntry(interface->Argv()[0], true).Exists()) {
		sConfigs.push_back(interface);
		return true;
	}
	LogInfo("LSP Server [%s] not installed!", interface->Argv()[0]);
	return false;
}


/* static */
status_t
LSPServersManager::InitLSPServersConfig()
{
	DoInAllDataDirectories([&](const BPath& path) {
		// iterate inside the directory
		BPath p(path);
		p.Append("lsp");

		BDirectory languages(p.Path());
		if (languages.InitCheck() != B_OK) {
			LogError("Can't read the lsp directory: %s", p.Path());
			return;
		}
		LogDebug("Reading the lsp directory: %s", p.Path());
		entry_ref ref;
		while(languages.GetNextRef(&ref) == B_OK) {
			LogTrace("--> LSP file: %s", ref.name);
			std::string name(ref.name);
			if (name.ends_with(".yaml") == false) {
				LogTrace("\tinvalid filename: %s", ref.name);
				continue;
			}
			name.resize(name.size() - 5);

			BEntry entry(&ref);
			if (entry.InitCheck() == B_OK && entry.IsFile()) {
				BPath lspFile(&ref);
				LogTrace("--> Lsp file: %s", lspFile.Path());
				YAMLServerConfig* yamllsp =YAMLServerConfig::fromFile(lspFile);
				if (yamllsp != nullptr) {
					LogInfo("LSP config loaded! %s", lspFile.Path());
					_AddValidConfig(yamllsp);
				}
			}
		}
	});
	return B_OK;
}


/* static */
status_t
LSPServersManager::DisposeLSPServersConfig()
{
	for (LSPServerConfigInterface* interface: sConfigs) {
		delete interface;
	}
	sConfigs.clear();
	return B_OK;
}


/* static */
LSPProjectWrapper*
LSPServersManager::CreateLSPProject(const BPath& path, const BMessenger& msgr, const BString& fileType)
{
	for (const LSPServerConfigInterface* interface: sConfigs) {
		if (interface->IsFileTypeSupported(fileType)) {
			return new LSPProjectWrapper(path, msgr, *interface);
		}
	}

	LogError("No available LSP server can handle %s file types!", fileType.String());

	return nullptr;
}
