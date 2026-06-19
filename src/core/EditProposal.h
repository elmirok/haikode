#ifndef HAIKODE_CORE_EDIT_PROPOSAL_H
#define HAIKODE_CORE_EDIT_PROPOSAL_H

#include <string>

class ProjectModel;

struct EditProposal {
	std::string path;
	std::string summary;
	std::string originalSha256;
	std::string replacement;

	static bool ParseFromResponse(const std::string& response,
		EditProposal& proposal);
	bool Apply(const ProjectModel& project, std::string& error) const;
	bool IsForSelectedFile(const ProjectModel& project) const;
};

#endif

