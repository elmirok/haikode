#ifndef HAIKODE_CORE_COMMAND_POLICY_H
#define HAIKODE_CORE_COMMAND_POLICY_H

#include <set>
#include <string>
#include <vector>

struct PendingCommand {
	std::string id;
	std::vector<std::string> argv;
	std::string workingDirectory;
};

class CommandPolicy {
public:
	void Approve(const std::string& commandId);
	void Revoke(const std::string& commandId);
	bool CanRun(const PendingCommand& command, std::string& error) const;

private:
	std::set<std::string> fApprovedCommands;
};

#endif
