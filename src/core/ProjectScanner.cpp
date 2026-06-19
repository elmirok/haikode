#include "core/ProjectScanner.h"

#include "core/IgnoreRules.h"

#include <algorithm>
#include <cctype>
#include <ctime>
#include <dirent.h>
#include <fstream>
#include <iterator>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace {

std::string
joinPath(const std::string& left, const std::string& right)
{
	if (left.empty() || left[left.size() - 1] == '/')
		return left + right;
	return left + "/" + right;
}

std::string
baseName(const std::string& path)
{
	const size_t slash = path.find_last_of('/');
	if (slash == std::string::npos)
		return path;
	return path.substr(slash + 1);
}

std::string
toLower(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return value;
}

bool
endsWith(const std::string& value, const std::string& suffix)
{
	if (value.size() < suffix.size())
		return false;
	return value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool
startsWith(const std::string& value, const std::string& prefix)
{
	return value.compare(0, prefix.size(), prefix) == 0;
}

bool
isSkippedDirectory(const std::string& name)
{
	return name.empty() || name[0] == '.' || name == "build" || name == "objects"
		|| name == "node_modules" || name == "generated";
}

bool
isTextByte(unsigned char c)
{
	return c == '\n' || c == '\r' || c == '\t' || (c >= 32 && c != 127);
}

bool
readTextFile(const std::string& path, size_t maxBytes, std::string& text)
{
	text.clear();

	struct stat info;
	if (lstat(path.c_str(), &info) != 0 || S_ISLNK(info.st_mode)
		|| !S_ISREG(info.st_mode)) {
		return false;
	}
	if (static_cast<size_t>(info.st_size) > maxBytes)
		return false;

	std::ifstream file(path, std::ios::binary);
	if (!file)
		return false;

	text.assign(std::istreambuf_iterator<char>(file),
		std::istreambuf_iterator<char>());
	for (unsigned char c : text) {
		if (!isTextByte(c))
			return false;
	}
	return true;
}

std::string
relativePath(const std::string& rootPath, const std::string& path)
{
	if (path == rootPath)
		return std::string();
	if (path.compare(0, rootPath.size(), rootPath) == 0
		&& path.size() > rootPath.size() && path[rootPath.size()] == '/') {
		return path.substr(rootPath.size() + 1);
	}
	return path;
}

std::string
detectLanguage(const std::string& name)
{
	const std::string lower = toLower(name);
	if (endsWith(lower, ".cpp") || endsWith(lower, ".cc") || endsWith(lower, ".cxx")
		|| endsWith(lower, ".hpp")) {
		return "C++";
	}
	if (endsWith(lower, ".h"))
		return "C/C++ Header";
	if (endsWith(lower, ".c"))
		return "C";
	if (endsWith(lower, ".py"))
		return "Python";
	if (endsWith(lower, ".js"))
		return "JavaScript";
	if (endsWith(lower, ".ts"))
		return "TypeScript";
	if (endsWith(lower, ".json"))
		return "JSON";
	if (endsWith(lower, ".yaml") || endsWith(lower, ".yml"))
		return "YAML";
	if (endsWith(lower, ".txt"))
		return "Text";
	if (endsWith(lower, ".md") || startsWith(lower, "readme"))
		return "Markdown";
	if (lower == "makefile" || endsWith(lower, ".make"))
		return "Make";
	if (endsWith(lower, ".recipe"))
		return "HaikuPorts recipe";
	if (endsWith(lower, ".sh"))
		return "Shell";
	return "Text";
}

std::string
detectRole(const std::string& relative, const std::string& language)
{
	const std::string lower = toLower(relative);
	if (language == "Markdown" || startsWith(baseName(lower), "readme"))
		return "docs";
	if (lower == "makefile" || endsWith(lower, ".recipe"))
		return "build";
	if (lower.find("test") != std::string::npos)
		return "test";
	if (endsWith(lower, ".h") || endsWith(lower, ".hpp"))
		return "interface";
	return "source";
}

int
countToken(const std::string& text, const std::string& token)
{
	int count = 0;
	size_t pos = text.find(token);
	while (pos != std::string::npos) {
		++count;
		pos = text.find(token, pos + token.size());
	}
	return count;
}

int
lineCount(const std::string& text)
{
	if (text.empty())
		return 0;
	return static_cast<int>(std::count(text.begin(), text.end(), '\n'))
		+ (text[text.size() - 1] == '\n' ? 0 : 1);
}

bool
isHighRisk(const std::string& relative, const std::string& text,
	const std::string& role)
{
	const std::string lower = toLower(relative + "\n" + text);
	return role == "build" || lower.find("settings") != std::string::npos
		|| lower.find("network") != std::string::npos
		|| lower.find("oauth") != std::string::npos
		|| lower.find("apply") != std::string::npos
		|| lower.find("writeattr") != std::string::npos;
}

bool
isRecentlyChanged(const std::string& path)
{
	struct stat info;
	if (stat(path.c_str(), &info) != 0)
		return false;
	const time_t now = time(nullptr);
	const time_t oneWeek = 7 * 24 * 60 * 60;
	return now >= info.st_mtime && now - info.st_mtime <= oneWeek;
}

std::string
currentTimestamp()
{
	std::ostringstream out;
	out << static_cast<long long>(time(nullptr));
	return out.str();
}

bool
projectHasDocs(const std::string& rootPath)
{
	DIR* dir = opendir(rootPath.c_str());
	if (dir == nullptr)
		return false;

	bool found = false;
	dirent* entry = nullptr;
	while ((entry = readdir(dir)) != nullptr) {
		const std::string name = entry->d_name;
		const std::string lower = toLower(name);
		if (startsWith(lower, "readme") || endsWith(lower, ".md")) {
			found = true;
			break;
		}
	}
	closedir(dir);
	return found;
}

}

ProjectScanResult
ProjectScanner::Scan(const std::string& rootPath, size_t maxFileBytes) const
{
	IgnoreRules rules;
	return Scan(rootPath, rules, maxFileBytes);
}

ProjectScanResult
ProjectScanner::Scan(const std::string& rootPath, const IgnoreRules& ignoreRules,
	size_t maxFileBytes) const
{
	ProjectScanResult result;
	ScanDirectory(rootPath, rootPath, projectHasDocs(rootPath), &ignoreRules,
		maxFileBytes, result.files);
	result.radar = BuildRadar(result.files);
	return result;
}

void
ProjectScanner::ScanDirectory(const std::string& rootPath,
	const std::string& directory, bool hasDocs, const IgnoreRules* ignoreRules,
	size_t maxFileBytes, std::vector<ProjectFileMetadata>& files) const
{
	DIR* dir = opendir(directory.c_str());
	if (dir == nullptr)
		return;

	dirent* entry = nullptr;
	while ((entry = readdir(dir)) != nullptr) {
		const std::string name = entry->d_name;
		if (name == "." || name == "..")
			continue;

		const std::string path = joinPath(directory, name);
		const std::string relative = relativePath(rootPath, path);
		if (ignoreRules != nullptr && ignoreRules->ShouldIgnore(relative))
			continue;

		struct stat info;
		if (lstat(path.c_str(), &info) != 0 || S_ISLNK(info.st_mode))
			continue;

		if (S_ISDIR(info.st_mode)) {
			if (!isSkippedDirectory(name))
				ScanDirectory(rootPath, path, hasDocs, ignoreRules,
					maxFileBytes, files);
			continue;
		}

		ProjectFileMetadata metadata;
		if (AnalyzeFile(rootPath, path, hasDocs, maxFileBytes, metadata))
			files.push_back(metadata);
	}

	closedir(dir);
}

bool
ProjectScanner::AnalyzeFile(const std::string& rootPath, const std::string& path,
	bool hasDocs, size_t maxFileBytes, ProjectFileMetadata& metadata) const
{
	const std::string name = baseName(path);
	if (name.empty() || name[0] == '.')
		return false;

	std::string text;
	if (!readTextFile(path, maxFileBytes, text))
		return false;

	metadata.path = path;
	metadata.relativePath = relativePath(rootPath, path);
	metadata.language = detectLanguage(name);
	metadata.role = detectRole(metadata.relativePath, metadata.language);
	metadata.lineCount = lineCount(text);
	metadata.todoCount = countToken(text, "TODO") + countToken(text, "FIXME")
		+ countToken(text, "HACK");
	metadata.hasTodo = metadata.todoCount > 0;
	metadata.todo = metadata.hasTodo ? "true" : "false";
	metadata.highRisk = isHighRisk(metadata.relativePath, text, metadata.role);
	metadata.riskLevel = metadata.highRisk ? "high" : "normal";
	metadata.testStatus = "unknown";
	metadata.lastReviewed = currentTimestamp();
	metadata.recentlyChanged = isRecentlyChanged(path);
	metadata.needsReview = true;
	metadata.aiNotesStale = true;
	metadata.missingDocs = !hasDocs && metadata.role == "source";

	std::ostringstream summary;
	summary << metadata.language << " " << metadata.role << ", "
		<< metadata.lineCount << " lines, " << metadata.todoCount
		<< " TODO markers, risk " << metadata.riskLevel;
	metadata.summary = summary.str();

	return true;
}

ProjectRadarCounts
ProjectScanner::BuildRadar(const std::vector<ProjectFileMetadata>& files) const
{
	ProjectRadarCounts counts;
	for (const ProjectFileMetadata& file : files) {
		if (file.needsReview)
			++counts.needsReview;
		if (file.hasTodo)
			++counts.todoFiles;
		if (file.highRisk)
			++counts.highRiskFiles;
		if (file.recentlyChanged)
			++counts.recentlyChanged;
		if (file.missingDocs)
			++counts.missingDocs;
		if (file.aiNotesStale)
			++counts.aiNotesStale;
	}
	return counts;
}
