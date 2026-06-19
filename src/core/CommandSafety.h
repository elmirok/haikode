#ifndef HAIKODE_CORE_COMMAND_SAFETY_H
#define HAIKODE_CORE_COMMAND_SAFETY_H

#include <string>
#include <vector>

struct CommandSafetyResult {
	bool allowed = true;
	bool requiresStrongConfirmation = false;
	std::vector<std::string> warnings;
};

class CommandSafety {
public:
	CommandSafetyResult Check(const std::string& command) const;
	static bool SplitCommand(const std::string& command,
		std::vector<std::string>& argv, std::string& error);
};

#endif
