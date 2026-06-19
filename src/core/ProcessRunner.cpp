#include "core/ProcessRunner.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <ctime>
#include <cstring>
#include <fcntl.h>
#include <limits.h>
#include <signal.h>
#include <sstream>
#include <sys/select.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>

namespace {

bool
canonicalPath(const std::string& path, std::string& out)
{
	char resolved[PATH_MAX];
	if (realpath(path.c_str(), resolved) == nullptr)
		return false;
	out = resolved;
	return true;
}

bool
hasPathPrefix(const std::string& path, const std::string& root)
{
	if (path == root)
		return true;
	if (path.size() <= root.size())
		return false;
	return path.compare(0, root.size(), root) == 0 && path[root.size()] == '/';
}

std::string
baseName(const std::string& path)
{
	const size_t slash = path.find_last_of('/');
	if (slash == std::string::npos)
		return path;
	return path.substr(slash + 1);
}

bool
containsWhitespace(const std::string& value)
{
	return value.find_first_of(" \t\r\n") != std::string::npos;
}

bool
isShellExecutable(const std::string& value)
{
	const std::string name = baseName(value);
	return name == "sh" || name == "bash" || name == "zsh" || name == "ksh"
		|| name == "dash";
}

void
setNonBlocking(int fd)
{
	const int flags = fcntl(fd, F_GETFL, 0);
	if (flags >= 0)
		fcntl(fd, F_SETFL, flags | O_NONBLOCK);
}

void
appendAvailable(int fd, std::string& text)
{
	char buffer[4096];
	for (;;) {
		const ssize_t count = read(fd, buffer, sizeof(buffer));
		if (count > 0) {
			text.append(buffer, static_cast<size_t>(count));
			continue;
		}
		break;
	}
}

std::vector<char*>
buildArgv(const std::vector<std::string>& args)
{
	std::vector<char*> argv;
	for (const std::string& arg : args)
		argv.push_back(const_cast<char*>(arg.c_str()));
	argv.push_back(nullptr);
	return argv;
}

bool
writeAll(int fd, const std::string& text)
{
	size_t written = 0;
	while (written < text.size()) {
		const ssize_t count = write(fd, text.data() + written,
			text.size() - written);
		if (count < 0) {
			if (errno == EINTR)
				continue;
			return false;
		}
		written += static_cast<size_t>(count);
	}
	return true;
}

}

ProcessRunner::ProcessRunner()
	:
	fChildPid(-1)
{
}

ProcessRunner::~ProcessRunner()
{
	Cancel();
}

bool
ProcessRunner::ResolveExecutable(const std::string& command,
	std::string& resolvedPath)
{
	resolvedPath.clear();
	if (command.empty())
		return false;

	if (command.find('/') != std::string::npos) {
		if (access(command.c_str(), X_OK) == 0) {
			resolvedPath = command;
			return true;
		}
		return false;
	}

	const char* path = getenv("PATH");
	if (path == nullptr)
		return false;

	std::istringstream parts(path);
	std::string directory;
	while (std::getline(parts, directory, ':')) {
		if (directory.empty())
			continue;
		const std::string candidate = directory + "/" + command;
		if (access(candidate.c_str(), X_OK) == 0) {
			resolvedPath = candidate;
			return true;
		}
	}
	return false;
}

bool
ProcessRunner::IsSafeArgv(const std::vector<std::string>& argv,
	std::string& error)
{
	error.clear();
	if (argv.empty() || argv[0].empty()) {
		error = "Command argv is empty.";
		return false;
	}

	if (containsWhitespace(argv[0])) {
		error = "Command argv[0] must be an executable path or name, not a shell command string.";
		return false;
	}

	if (isShellExecutable(argv[0])) {
		error = "Shell executables are not allowed; pass an explicit argv command.";
		return false;
	}

	for (const std::string& arg : argv) {
		if (arg.find('\0') != std::string::npos) {
			error = "Command argv contains an invalid NUL byte.";
			return false;
		}
	}

	return true;
}

bool
ProcessRunner::RunInsideProject(const std::string& projectRoot,
	const ProcessRequest& request, ProcessResult& result, std::string& error)
{
	std::string root;
	std::string cwd;
	if (!canonicalPath(projectRoot, root) || !canonicalPath(request.workingDirectory, cwd)) {
		error = "Could not resolve project paths.";
		return false;
	}
	if (!hasPathPrefix(cwd, root)) {
		error = "Command working directory must stay inside the selected project.";
		return false;
	}

	return Run(request, result, error);
}

bool
ProcessRunner::Run(const ProcessRequest& request, ProcessResult& result,
	std::string& error)
{
	result = ProcessResult();
	error.clear();

	if (!IsSafeArgv(request.argv, error))
		return false;

	std::string executable;
	if (!ResolveExecutable(request.argv[0], executable)) {
		error = "Command executable not found.";
		return false;
	}

	int stdoutPipe[2];
	int stderrPipe[2];
	int stdinPipe[2];
	if (pipe(stdoutPipe) != 0 || pipe(stderrPipe) != 0 || pipe(stdinPipe) != 0) {
		error = "Could not create process pipes.";
		return false;
	}

	pid_t pid = fork();
	if (pid < 0) {
		error = "Could not start process.";
		close(stdoutPipe[0]);
		close(stdoutPipe[1]);
		close(stderrPipe[0]);
		close(stderrPipe[1]);
		close(stdinPipe[0]);
		close(stdinPipe[1]);
		return false;
	}

	if (pid == 0) {
		dup2(stdinPipe[0], STDIN_FILENO);
		dup2(stdoutPipe[1], STDOUT_FILENO);
		dup2(stderrPipe[1], STDERR_FILENO);
		close(stdoutPipe[0]);
		close(stdoutPipe[1]);
		close(stderrPipe[0]);
		close(stderrPipe[1]);
		close(stdinPipe[0]);
		close(stdinPipe[1]);

		if (!request.workingDirectory.empty())
			chdir(request.workingDirectory.c_str());

		std::vector<std::string> args = request.argv;
		args[0] = executable;
		std::vector<char*> childArgv = buildArgv(args);
		execv(executable.c_str(), childArgv.data());
		_exit(127);
	}

	fChildPid.store(static_cast<int>(pid));
	close(stdoutPipe[1]);
	close(stderrPipe[1]);
	close(stdinPipe[0]);
	setNonBlocking(stdoutPipe[0]);
	setNonBlocking(stderrPipe[0]);

	if (!request.stdinText.empty())
		writeAll(stdinPipe[1], request.stdinText);
	close(stdinPipe[1]);

	time_t start = time(nullptr);
	int status = 0;
	bool running = true;
	while (running) {
		appendAvailable(stdoutPipe[0], result.stdoutText);
		appendAvailable(stderrPipe[0], result.stderrText);

		const pid_t waited = waitpid(pid, &status, WNOHANG);
		if (waited == pid) {
			running = false;
			break;
		}

		if (request.timeoutSeconds > 0
			&& time(nullptr) - start >= request.timeoutSeconds) {
			kill(pid, SIGTERM);
			result.timedOut = true;
			waitpid(pid, &status, 0);
			running = false;
			break;
		}

		if (fChildPid.load() < 0) {
			result.cancelled = true;
			waitpid(pid, &status, 0);
			running = false;
			break;
		}

		usleep(20000);
	}

	appendAvailable(stdoutPipe[0], result.stdoutText);
	appendAvailable(stderrPipe[0], result.stderrText);
	close(stdoutPipe[0]);
	close(stderrPipe[0]);
	fChildPid.store(-1);

	if (WIFEXITED(status))
		result.exitCode = WEXITSTATUS(status);
	else if (WIFSIGNALED(status))
		result.exitCode = 128 + WTERMSIG(status);

	return true;
}

void
ProcessRunner::Cancel()
{
	const int child = fChildPid.exchange(-1);
	if (child >= 0) {
		kill(static_cast<pid_t>(child), SIGTERM);
	}
}
