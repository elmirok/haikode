/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include "CancellationToken.h"

#include <string>
#include <vector>

namespace Haikode::AI {

struct ProcessCaptureOptions {
	std::vector<std::string> argv;
	std::string workingDirectory;
	int timeoutSeconds = 300;
	size_t maxOutputBytes = 1024 * 1024;
	const CancellationToken* cancellation = nullptr;
};

struct ProcessCaptureResult {
	std::string output;
	int exitCode = -1;
	bool timedOut = false;
	bool cancelled = false;
};

class ProcessCapture {
public:
	static bool Run(const ProcessCaptureOptions& options,
		ProcessCaptureResult& result, std::string& error);
	static bool SaveLog(const std::string& projectRoot,
		const std::string& label, const ProcessCaptureOptions& options,
		const ProcessCaptureResult& result, const std::string& errorText,
		std::string& savedPath, std::string& error);
};

} // namespace Haikode::AI
