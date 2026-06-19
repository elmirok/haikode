#ifndef HAIKODE_CORE_PROJECT_MEMORY_H
#define HAIKODE_CORE_PROJECT_MEMORY_H

#include "core/IgnoreRules.h"
#include "core/ProjectScanner.h"

#include <map>
#include <string>

class ProjectMemory {
public:
	bool Open(const std::string& projectRoot, std::string& error);
	bool UpdateFile(const ProjectFileMetadata& metadata, std::string& error);
	bool UpdateFiles(const std::vector<ProjectFileMetadata>& files,
		std::string& error);
	bool SetDefaultBuildCommand(const std::string& command, std::string& error);
	bool SetDefaultTestCommand(const std::string& command, std::string& error);
	bool SaveSession(const std::string& text, std::string& path,
		std::string& error) const;
	bool SaveCommandLog(const std::string& text, std::string& path,
		std::string& error) const;

	const std::string& ProjectRoot() const { return fProjectRoot; }
	const std::string& ProjectName() const { return fProjectName; }
	const std::string& DefaultBuildCommand() const { return fDefaultBuildCommand; }
	const std::string& DefaultTestCommand() const { return fDefaultTestCommand; }
	std::string HaikodeDirectory() const;
	std::string ProjectJsonPath() const;
	std::string PatchesDirectory() const;
	std::string LogsDirectory() const;
	std::string BackupsDirectory() const;
	size_t FileCount() const { return fFiles.size(); }
	IgnoreRules& Rules() { return fIgnoreRules; }
	const IgnoreRules& Rules() const { return fIgnoreRules; }

private:
	bool EnsureLayout(std::string& error) const;
	bool LoadExisting();
	bool Save(std::string& error) const;
	bool WriteText(const std::string& path, const std::string& text,
		std::string& error) const;

	std::string fProjectRoot;
	std::string fProjectName;
	std::string fCreatedAt;
	std::string fUpdatedAt;
	std::string fDefaultBuildCommand;
	std::string fDefaultTestCommand;
	IgnoreRules fIgnoreRules;
	std::map<std::string, ProjectFileMetadata> fFiles;
};

#endif
