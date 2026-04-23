/*
 * Copyright 2023-2026, Andrea Anzani
 * All rights reserved. Distributed under the terms of the MIT license.
 */


#include "LSPServersManager.h"

#include <Directory.h>
#include <string>
#include <vector>

#include <PathFinder.h>
#include <yaml-cpp/yaml.h>

#include "ConfigManager.h"
#include "GException.h"
#include "GenioApp.h"
#include "Log.h"
#include "LSPLogLevels.h"
#include "LSPProjectWrapper.h"
#include "Utils.h"


class ClangdServerConfig : public LSPServerConfigInterface {
public:
	ClangdServerConfig()
	{
		std::string logLevel("--log=");
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

		// TODO: Use find_path
		fArgv = {
			"/boot/system/bin/clangd",
			strdup(logLevel.c_str()),
			"--offset-encoding=utf-8",
			"--pretty",
			"--header-insertion-decorators=false",
			"--pch-storage=memory"
		};

		fOffset = 1;
	}

	const bool IsFileTypeSupported(const BString& fileType) const override {
		if (fileType.Compare("cpp") != 0 &&
			fileType.Compare("c") != 0 &&
			fileType.Compare("makefile") != 0)
			return false;
		return true;
	}
};


class PylspServerConfig : public LSPServerConfigInterface {
public:
	PylspServerConfig()
	{
		BStringList paths;
		BPath path;
		status_t status = BPathFinder::FindPaths(B_FIND_PATH_BIN_DIRECTORY, paths);
		if (status != B_OK)
			throw GException(status, ::strerror(status));

		for (int32 c = 0; c < paths.CountStrings(); c++) {
			BString binaryName = "pylsp";
			BPath filePath = paths.StringAt(c).String();
			filePath.Append(binaryName);
			if (BEntry(filePath.Path()).Exists()) {
				path = filePath;
				break;
			}
		}

		fArgv = {
			strdup(path.Path()),
			"-v"
		};
	}

	const bool IsFileTypeSupported(const BString& fileType) const override
	{
		return (fileType.Compare("python") == 0);
	}
};


class YAMLServerConfig : public LSPServerConfigInterface
{
	public:
			static YAMLServerConfig*	fromFile(BPath lspFile)
			{
				YAMLServerConfig* config = nullptr;
				try {
					const YAML::Node lsp = YAML::LoadFile(lspFile.Path());
					if (!lsp["name"] ||
						!lsp["command"] ||
						!lsp["fileTypes"])
							return nullptr;

					config = new YAMLServerConfig();
					std::string command = lsp["command"].as<std::string>();;
					config->fArgv.push_back(command.c_str());
					for (const auto& ft : lsp["fileTypes"]) {
						config->fFileTypes.push_back(ft.as<std::string>());
					}
					// args
					if (lsp["args"]) {
						for (const auto& arg : lsp["args"]) {
							std::string argument = arg.as<std::string>();
							if (argument.find("${") != std::string::npos)
								continue;

							config->fArgv.push_back(arg.as<std::string>().c_str());
						}
					}



				} catch (const YAML::Exception & e)  {
					if (config != nullptr){
						delete config;
						config = nullptr;
					}
					//LogError
					printf("Error reading %s (%s)\n", lspFile.Path(), e.msg.c_str());
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

class OmniSharpServerConfig : public LSPServerConfigInterface {
public:
	OmniSharpServerConfig()
	{
		thread_id pid = find_thread(NULL);
		BString spid;
		spid << (int32)pid;

		// TODO: Use find_path
		fArgv = {
			"/boot/system/non-packaged/bin/dotnet/dotnet",
			"/boot/system/non-packaged/bin/OmniSharp/OmniSharp.dll",
			"-lsp",
			"-v",
			"--hostPID",
			strdup(spid.String())
		};

		fOffset = 0;
	}

	const bool	IsFileTypeSupported(const BString& fileType) const override
	{
		return (fileType.Compare("csharp") == 0);
	}
};


// Experimental config for csharp-language-server by razzmatazz
// class CSharpLanguageServerConfig : public LSPServerConfigInterface {
// public:
	// CSharpLanguageServerConfig() {
		// fArgv = {
			// "/boot/system/non-packaged/bin/dotnet/dotnet",
			// "/boot/home/workspace/csharp-language-server/src/CSharpLanguageServer/bin/Debug/net8.0/CSharpLanguageServer.dll",
			// "-s",
			// "##SOLUTION##"
		// };
	// }
	// const bool	IsFileTypeSupported(const BString& fileType) const override {
		// return (fileType.Compare("cs") != 0 &&
				// fileType.Compare("csproj") != 0 &&
				// fileType.Compare("sln") != 0 &&
				// fileType.Compare("cake") != 0);
	// }
// };



std::vector<LSPServerConfigInterface*> LSPServersManager::fConfigs;


/* static */
bool
LSPServersManager::_AddValidConfig(LSPServerConfigInterface* interface)
{
	if (interface->Argc() > 0 && BEntry(interface->Argv()[0], true).Exists()) {
		fConfigs.push_back(interface);
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
		// we should iterate inside the directory
		// TODO move to an external method

		BPath p(path);
		p.Append("lsp");

		BDirectory languages(p.Path());
		if (languages.InitCheck() != B_OK) {
			//LogError
			printf("Can't reading the lsp directory: %s\n", p.Path());
			return;
		}
		//LogDebug
		printf("Reading the lsp directory: %s\n", p.Path());
		entry_ref ref;
		while(languages.GetNextRef(&ref) == B_OK) {
			//LogTrace
			printf("--> LSP file: %s\n", ref.name);
			std::string name(ref.name);
			if (name.ends_with(".yaml") == false) {
				//LogTrace
				printf("    invalid filename: %s\n", ref.name);
				continue;
			}
			name.resize(name.size() - 5);

			BEntry entry(&ref);
			if (entry.InitCheck() == B_OK && entry.IsFile()) {
				BPath lspFile(&ref);
				//LogTrace
				printf("--> Lsp file: %s\n", lspFile.Path());
				YAMLServerConfig* yamllsp =YAMLServerConfig::fromFile(lspFile);
				if (yamllsp != nullptr) {
					//LogInfo
					printf("LSP config loaded! %s\n", lspFile.Path());
					_AddValidConfig(yamllsp);
				}

			}

		}

	});

	_AddValidConfig(new ClangdServerConfig());
	_AddValidConfig(new PylspServerConfig());
	_AddValidConfig(new OmniSharpServerConfig());
	return B_OK;
}


/* static */
status_t
LSPServersManager::DisposeLSPServersConfig()
{
	for (LSPServerConfigInterface* interface: fConfigs) {
		delete interface;
	}
	fConfigs.clear();
	return B_OK;
}


/* static */
LSPProjectWrapper*
LSPServersManager::CreateLSPProject(const BPath& path, const BMessenger& msgr, const BString& fileType)
{
	for (const LSPServerConfigInterface* interface: fConfigs) {
		if (interface->IsFileTypeSupported(fileType)) {
			return new LSPProjectWrapper(path, msgr, *interface);
		}
	}

	LogError("No available LSP server can handle %s file types!", fileType.String());

	return nullptr;
}
