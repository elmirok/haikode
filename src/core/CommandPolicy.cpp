#include "core/CommandPolicy.h"

void
CommandPolicy::Approve(const std::string& commandId)
{
	if (!commandId.empty())
		fApprovedCommands.insert(commandId);
}

void
CommandPolicy::Revoke(const std::string& commandId)
{
	fApprovedCommands.erase(commandId);
}

bool
CommandPolicy::CanRun(const PendingCommand& command, std::string& error) const
{
	error.clear();
	if (command.id.empty()) {
		error = "Command has no approval id.";
		return false;
	}
	if (fApprovedCommands.count(command.id) == 0) {
		error = "Command has not been explicitly approved.";
		return false;
	}
	if (command.argv.empty()) {
		error = "Command argv is empty.";
		return false;
	}
	return true;
}
