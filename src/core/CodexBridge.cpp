#include "core/CodexBridge.h"

#include <sstream>

CodexBridge::CodexBridge(const std::string& codexPath)
	:
	fCodexPath(codexPath.empty() ? "codex" : codexPath)
{
}

void
CodexBridge::SetCodexPath(const std::string& codexPath)
{
	fCodexPath = codexPath.empty() ? "codex" : codexPath;
}

bool
CodexBridge::Resolve(std::string& resolvedPath, std::string& error) const
{
	if (!ProcessRunner::ResolveExecutable(fCodexPath, resolvedPath)) {
		error = "Codex CLI not found. Install Codex CLI or set the codex path in Haikode settings.";
		return false;
	}
	return true;
}

CodexStatus
CodexBridge::Status()
{
	CodexStatus status;
	std::string resolved;
	std::string error;
	if (!Resolve(resolved, error)) {
		status.state = CodexState::NotFound;
		status.message = error;
		return status;
	}

	ProcessRequest request;
	request.argv.push_back(resolved);
	request.argv.push_back("login");
	request.argv.push_back("status");
	request.timeoutSeconds = 10;

	ProcessResult result;
	if (!fRunner.Run(request, result, error)) {
		status.state = CodexState::Error;
		status.message = error;
		return status;
	}

	if (result.exitCode == 0) {
		status.state = CodexState::LoggedIn;
		status.message = result.stdoutText.empty() ? "Codex logged in." : result.stdoutText;
		return status;
	}

	status.state = CodexState::LoggedOut;
	status.message = !result.stderrText.empty() ? result.stderrText
		: "Codex is installed but not logged in.";
	return status;
}

bool
CodexBridge::StartDeviceLogin(ProcessResult& result, std::string& error)
{
	std::string resolved;
	if (!Resolve(resolved, error))
		return false;

	ProcessRequest request;
	request.argv.push_back(resolved);
	request.argv.push_back("login");
	request.argv.push_back("--device-auth");
	request.timeoutSeconds = 120;
	return fRunner.Run(request, result, error);
}

std::string
CodexBridge::BuildPrompt(const std::string& prompt) const
{
	std::ostringstream text;
	text << "You are running inside Haikode, a native Haiku OS coding assistant.\n"
		<< "Stay in supervised mode. Do not run shell/build commands. "
		<< "Do not edit files directly. If code changes are useful, return either "
		<< "a concise explanation or a unified diff for the user to review in Haikode.\n\n"
		<< prompt;
	return text.str();
}

bool
CodexBridge::RunReadOnlyPrompt(const std::string& projectRoot,
	const std::string& prompt, ProcessResult& result, std::string& error)
{
	std::string resolved;
	if (!Resolve(resolved, error))
		return false;

	ProcessRequest request;
	request.argv.push_back(resolved);
	request.argv.push_back("exec");
	request.argv.push_back("--sandbox");
	request.argv.push_back("read-only");
	request.argv.push_back("--cd");
	request.argv.push_back(projectRoot);
	request.argv.push_back("--skip-git-repo-check");
	request.argv.push_back("--ephemeral");
	request.argv.push_back("--color");
	request.argv.push_back("never");
	request.argv.push_back("-");
	request.workingDirectory = projectRoot;
	request.stdinText = BuildPrompt(prompt);
	request.timeoutSeconds = 180;

	return fRunner.RunInsideProject(projectRoot, request, result, error);
}

void
CodexBridge::Cancel()
{
	fRunner.Cancel();
}
