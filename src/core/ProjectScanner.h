#ifndef HAIKODE_CORE_PROJECT_SCANNER_H
#define HAIKODE_CORE_PROJECT_SCANNER_H

#include <string>
#include <vector>

class IgnoreRules;

struct ProjectFileMetadata {
	std::string path;
	std::string relativePath;
	std::string language;
	std::string role;
	std::string riskLevel;
	std::string testStatus;
	std::string lastReviewed;
	std::string todo;
	std::string summary;
	bool hasTodo = false;
	bool needsReview = true;
	bool highRisk = false;
	bool recentlyChanged = false;
	bool missingDocs = false;
	bool aiNotesStale = true;
	int lineCount = 0;
	int todoCount = 0;
};

struct ProjectRadarCounts {
	int needsReview = 0;
	int todoFiles = 0;
	int highRiskFiles = 0;
	int recentlyChanged = 0;
	int missingDocs = 0;
	int aiNotesStale = 0;
};

struct ProjectScanResult {
	std::vector<ProjectFileMetadata> files;
	ProjectRadarCounts radar;
};

class ProjectScanner {
public:
	ProjectScanResult Scan(const std::string& rootPath,
		size_t maxFileBytes = 64 * 1024) const;
	ProjectScanResult Scan(const std::string& rootPath,
		const IgnoreRules& ignoreRules, size_t maxFileBytes = 64 * 1024) const;

private:
	void ScanDirectory(const std::string& rootPath, const std::string& directory,
		bool projectHasDocs, const IgnoreRules* ignoreRules, size_t maxFileBytes,
		std::vector<ProjectFileMetadata>& files) const;
	bool AnalyzeFile(const std::string& rootPath, const std::string& path,
		bool projectHasDocs, size_t maxFileBytes,
		ProjectFileMetadata& metadata) const;
	ProjectRadarCounts BuildRadar(
		const std::vector<ProjectFileMetadata>& files) const;
};

#endif
