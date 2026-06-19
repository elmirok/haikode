/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "ProcessCapture.h"

#include <chrono>
#include <cstring>
#include <filesystem>
#include <thread>

#include <errno.h>
#include <fcntl.h>
#include <signal.h>
#include <sys/select.h>
#include <sys/wait.h>
#include <unistd.h>

namespace Haikode::AI {

namespace fs = std::filesystem;

namespace {

bool
ValidateOptions(const ProcessCaptureOptions& options, std::string& error)
{
	if (options.argv.empty() || options.argv[0].empty()) {
		error = "Process argv must include an executable.";
		return false;
	}
	if (options.workingDirectory.empty()) {
		error = "Process working directory is required.";
		return false;
	}

	std::error_code fsError;
	if (!fs::exists(options.workingDirectory, fsError)
		|| !fs::is_directory(options.workingDirectory, fsError)) {
		error = "Process working directory does not exist.";
		return false;
	}
	return true;
}


void
AppendOutput(std::string& output, const char* buffer, ssize_t count,
	size_t maxOutputBytes)
{
	if (count <= 0 || output.size() >= maxOutputBytes)
		return;
	const size_t available = maxOutputBytes - output.size();
	output.append(buffer, static_cast<size_t>(count) > available
		? available : static_cast<size_t>(count));
}

} // namespace


bool
ProcessCapture::Run(const ProcessCaptureOptions& options,
	ProcessCaptureResult& result, std::string& error)
{
	result = ProcessCaptureResult();
	error.clear();
	if (!ValidateOptions(options, error))
		return false;

	int pipeFds[2];
	if (pipe(pipeFds) != 0) {
		error = std::string("Could not create process pipe: ") + strerror(errno);
		return false;
	}

	const pid_t pid = fork();
	if (pid < 0) {
		error = std::string("Could not start process: ") + strerror(errno);
		close(pipeFds[0]);
		close(pipeFds[1]);
		return false;
	}

	if (pid == 0) {
		close(pipeFds[0]);
		dup2(pipeFds[1], STDOUT_FILENO);
		dup2(pipeFds[1], STDERR_FILENO);
		close(pipeFds[1]);
		if (chdir(options.workingDirectory.c_str()) != 0)
			_exit(127);

		std::vector<char*> argv;
		argv.reserve(options.argv.size() + 1);
		for (const std::string& arg : options.argv)
			argv.push_back(const_cast<char*>(arg.c_str()));
		argv.push_back(nullptr);
		execvp(argv[0], argv.data());
		_exit(127);
	}

	close(pipeFds[1]);
	int flags = fcntl(pipeFds[0], F_GETFL, 0);
	if (flags >= 0)
		fcntl(pipeFds[0], F_SETFL, flags | O_NONBLOCK);

	const auto start = std::chrono::steady_clock::now();
	bool childExited = false;
	int status = 0;
	char buffer[4096];
	while (true) {
		const ssize_t count = read(pipeFds[0], buffer, sizeof(buffer));
		if (count > 0) {
			AppendOutput(result.output, buffer, count, options.maxOutputBytes);
			continue;
		}

		const pid_t waitResult = waitpid(pid, &status, WNOHANG);
		if (waitResult == pid) {
			childExited = true;
			while (true) {
				const ssize_t tail = read(pipeFds[0], buffer, sizeof(buffer));
				if (tail <= 0)
					break;
				AppendOutput(result.output, buffer, tail,
					options.maxOutputBytes);
			}
			break;
		}
		if (waitResult < 0) {
			error = std::string("Could not wait for process: ") + strerror(errno);
			close(pipeFds[0]);
			return false;
		}

		if (options.cancellation != nullptr
			&& options.cancellation->IsCancelled()) {
			result.cancelled = true;
			kill(pid, SIGTERM);
			std::this_thread::sleep_for(std::chrono::milliseconds(250));
			if (waitpid(pid, &status, WNOHANG) == 0)
				kill(pid, SIGKILL);
			waitpid(pid, &status, 0);
			break;
		}

		if (options.timeoutSeconds > 0) {
			const auto elapsed = std::chrono::duration_cast<std::chrono::seconds>(
				std::chrono::steady_clock::now() - start).count();
			if (elapsed >= options.timeoutSeconds) {
				result.timedOut = true;
				kill(pid, SIGTERM);
				std::this_thread::sleep_for(std::chrono::milliseconds(250));
				if (waitpid(pid, &status, WNOHANG) == 0)
					kill(pid, SIGKILL);
				waitpid(pid, &status, 0);
				break;
			}
		}

		fd_set readSet;
		FD_ZERO(&readSet);
		FD_SET(pipeFds[0], &readSet);
		timeval timeout;
		timeout.tv_sec = 0;
		timeout.tv_usec = 100000;
		select(pipeFds[0] + 1, &readSet, nullptr, nullptr, &timeout);
	}

	close(pipeFds[0]);
	if (childExited && WIFEXITED(status))
		result.exitCode = WEXITSTATUS(status);
	else if (childExited && WIFSIGNALED(status))
		result.exitCode = 128 + WTERMSIG(status);

	if (result.timedOut) {
		error = "Process timed out.";
		return false;
	}
	if (result.cancelled) {
		error = "Process cancelled.";
		return false;
	}
	if (!childExited) {
		error = "Process did not exit normally.";
		return false;
	}
	if (result.exitCode != 0)
		error = "Process exited with status " + std::to_string(result.exitCode)
			+ ".";
	return result.exitCode == 0;
}

} // namespace Haikode::AI
