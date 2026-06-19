#ifndef HAIKODE_CORE_CODEX_BRIDGE_H
#define HAIKODE_CORE_CODEX_BRIDGE_H

#include "core/ProcessRunner.h"

#include <string>

enum class CodexState {
	NotFound,
	LoggedOut,
	LoggedIn,
	Error,
	Running
};

struct CodexStatus {
	CodexState state = CodexState::NotFound;
	std::string message;
};

class CodexBridge {
public:
	explicit CodexBridge(const std::string& codexPath = "codex");

	const std::string& CodexPath() const { return fCodexPath; }
	void SetCodexPath(const std::string& codexPath);
	CodexStatus Status();
	bool StartDeviceLogin(ProcessResult& result, std::string& error);
	bool RunReadOnlyPrompt(const std::string& projectRoot,
		const std::string& prompt, ProcessResult& result,
		std::string& error);
	void Cancel();

private:
	bool Resolve(std::string& resolvedPath, std::string& error) const;
	std::string BuildPrompt(const std::string& prompt) const;

	std::string fCodexPath;
	ProcessRunner fRunner;
};

#endif
