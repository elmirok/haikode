#ifndef HAIKODE_CORE_PATCH_MANAGER_H
#define HAIKODE_CORE_PATCH_MANAGER_H

#include <string>
#include <vector>

struct PatchValidation {
	std::vector<std::string> files;
	std::vector<std::string> warnings;
};

struct PatchApplyResult {
	std::string backupDirectory;
	std::vector<std::string> changedFiles;
};

class PatchManager {
public:
	bool SavePatch(const std::string& projectRoot, const std::string& diff,
		std::string& patchPath, std::string& error) const;
	bool Validate(const std::string& projectRoot, const std::string& diff,
		PatchValidation& validation, std::string& error) const;
	bool Apply(const std::string& projectRoot, const std::string& diff,
		PatchApplyResult& result, std::string& error) const;
};

#endif
