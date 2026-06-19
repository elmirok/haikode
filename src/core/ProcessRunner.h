#ifndef HAIKODE_CORE_PROCESS_RUNNER_H
#define HAIKODE_CORE_PROCESS_RUNNER_H

#include <atomic>
#include <string>
#include <vector>

struct ProcessRequest {
	std::vector<std::string> argv;
	std::string workingDirectory;
	std::string stdinText;
	int timeoutSeconds = 0;
};

struct ProcessResult {
	int exitCode = -1;
	bool timedOut = false;
	bool cancelled = false;
	std::string stdoutText;
	std::string stderrText;
};

class ProcessRunner {
public:
	ProcessRunner();
	~ProcessRunner();

	bool Run(const ProcessRequest& request, ProcessResult& result,
		std::string& error);
	bool RunInsideProject(const std::string& projectRoot,
		const ProcessRequest& request, ProcessResult& result,
		std::string& error);
	void Cancel();

	static bool ResolveExecutable(const std::string& command,
		std::string& resolvedPath);
	static bool IsSafeArgv(const std::vector<std::string>& argv,
		std::string& error);

private:
	std::atomic<int> fChildPid;
};

#endif
