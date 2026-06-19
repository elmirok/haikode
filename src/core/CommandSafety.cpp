#include "core/CommandSafety.h"

#include <cctype>

namespace {

std::string
toLower(std::string value)
{
	for (char& c : value)
		c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
	return value;
}

bool
contains(const std::string& value, const std::string& needle)
{
	return value.find(needle) != std::string::npos;
}

}

CommandSafetyResult
CommandSafety::Check(const std::string& command) const
{
	CommandSafetyResult result;
	const std::string lower = toLower(command);
	const char* patterns[] = {
		"rm -rf",
		"mkfs",
		"dd ",
		"sudo",
		"su ",
		"chmod -r 777",
		"chown -r",
		"curl | sh",
		"wget | sh",
		"| sh"
	};

	for (const char* pattern : patterns) {
		if (contains(lower, pattern)) {
			result.requiresStrongConfirmation = true;
			result.warnings.push_back("Command contains a dangerous pattern: "
				+ std::string(pattern));
		}
	}
	return result;
}

bool
CommandSafety::SplitCommand(const std::string& command,
	std::vector<std::string>& argv, std::string& error)
{
	argv.clear();
	error.clear();
	std::string current;
	bool inQuote = false;
	char quoteChar = '\0';

	for (size_t i = 0; i < command.size(); ++i) {
		const char c = command[i];
		if (inQuote) {
			if (c == quoteChar) {
				inQuote = false;
			} else {
				current.push_back(c);
			}
			continue;
		}
		if (c == '\'' || c == '"') {
			inQuote = true;
			quoteChar = c;
			continue;
		}
		if (std::isspace(static_cast<unsigned char>(c))) {
			if (!current.empty()) {
				argv.push_back(current);
				current.clear();
			}
			continue;
		}
		current.push_back(c);
	}

	if (inQuote) {
		error = "Command has an unterminated quote.";
		return false;
	}
	if (!current.empty())
		argv.push_back(current);
	if (argv.empty()) {
		error = "Command is empty.";
		return false;
	}
	return true;
}
