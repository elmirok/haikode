#ifndef HAIKODE_CORE_DIFF_PROPOSAL_H
#define HAIKODE_CORE_DIFF_PROPOSAL_H

#include <string>
#include <vector>

struct DiffFileProposal {
	std::string oldPath;
	std::string newPath;
};

class DiffProposal {
public:
	static bool ParseUnifiedDiff(const std::string& diff,
		DiffProposal& proposal);

	const std::vector<DiffFileProposal>& Files() const { return fFiles; }
	bool ValidatePaths(const std::string& projectRoot, std::string& error) const;
	bool IsEmpty() const { return fFiles.empty(); }

private:
	std::vector<DiffFileProposal> fFiles;
};

#endif
