#include "core/ProjectMemory.h"

#include "core/TimeUtils.h"

#include <cerrno>
#include <fstream>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace {

std::string
baseName(const std::string& path)
{
	const size_t slash = path.find_last_of('/');
	if (slash == std::string::npos)
		return path;
	return path.substr(slash + 1);
}

bool
makeDirectory(const std::string& path)
{
	if (path.empty())
		return false;
	struct stat info;
	if (stat(path.c_str(), &info) == 0)
		return S_ISDIR(info.st_mode);
	return mkdir(path.c_str(), 0755) == 0 || errno == EEXIST;
}

std::string
jsonEscape(const std::string& value)
{
	std::ostringstream out;
	for (unsigned char c : value) {
		switch (c) {
			case '"':
				out << "\\\"";
				break;
			case '\\':
				out << "\\\\";
				break;
			case '\n':
				out << "\\n";
				break;
			case '\r':
				out << "\\r";
				break;
			case '\t':
				out << "\\t";
				break;
			default:
				if (c < 32)
					out << ' ';
				else
					out << c;
				break;
		}
	}
	return out.str();
}

std::string
extractString(const std::string& text, const std::string& key)
{
	const std::string marker = "\"" + key + "\"";
	size_t pos = text.find(marker);
	if (pos == std::string::npos)
		return std::string();
	pos = text.find(':', pos + marker.size());
	if (pos == std::string::npos)
		return std::string();
	pos = text.find('"', pos + 1);
	if (pos == std::string::npos)
		return std::string();
	++pos;

	std::string value;
	while (pos < text.size()) {
		const char c = text[pos++];
		if (c == '"')
			return value;
		if (c == '\\' && pos < text.size()) {
			const char escaped = text[pos++];
			if (escaped == 'n')
				value.push_back('\n');
			else if (escaped == 't')
				value.push_back('\t');
			else
				value.push_back(escaped);
		} else {
			value.push_back(c);
		}
	}
	return std::string();
}

std::string
extractObjectString(const std::string& object, const std::string& key)
{
	return extractString(object, key);
}

}

std::string
ProjectMemory::HaikodeDirectory() const
{
	return fProjectRoot + "/.haikode";
}

std::string
ProjectMemory::ProjectJsonPath() const
{
	return HaikodeDirectory() + "/project.json";
}

std::string
ProjectMemory::PatchesDirectory() const
{
	return HaikodeDirectory() + "/patches";
}

std::string
ProjectMemory::LogsDirectory() const
{
	return HaikodeDirectory() + "/logs";
}

std::string
ProjectMemory::BackupsDirectory() const
{
	return HaikodeDirectory() + "/backups";
}

bool
ProjectMemory::Open(const std::string& projectRoot, std::string& error)
{
	error.clear();
	char resolved[PATH_MAX];
	if (realpath(projectRoot.c_str(), resolved) == nullptr) {
		error = "Could not resolve project root.";
		return false;
	}

	fProjectRoot = resolved;
	fProjectName = baseName(fProjectRoot);
	if (fProjectName.empty())
		fProjectName = "project";

	if (!EnsureLayout(error))
		return false;

	fCreatedAt = TimeUtils::IsoTimestamp();
	fUpdatedAt = fCreatedAt;
	LoadExisting();
	return Save(error);
}

bool
ProjectMemory::EnsureLayout(std::string& error) const
{
	if (!makeDirectory(HaikodeDirectory())
		|| !makeDirectory(HaikodeDirectory() + "/sessions")
		|| !makeDirectory(HaikodeDirectory() + "/notes")
		|| !makeDirectory(PatchesDirectory())
		|| !makeDirectory(LogsDirectory())
		|| !makeDirectory(BackupsDirectory())
		|| !makeDirectory(HaikodeDirectory() + "/cache")
		|| !makeDirectory(HaikodeDirectory() + "/tmp")) {
		error = "Could not create .haikode project memory folders.";
		return false;
	}
	return true;
}

bool
ProjectMemory::LoadExisting()
{
	std::ifstream file(ProjectJsonPath());
	if (!file)
		return false;
	std::string text((std::istreambuf_iterator<char>(file)),
		std::istreambuf_iterator<char>());
	if (text.empty())
		return false;

	const std::string created = extractString(text, "createdAt");
	if (!created.empty())
		fCreatedAt = created;
	fDefaultBuildCommand = extractString(text, "defaultBuildCommand");
	fDefaultTestCommand = extractString(text, "defaultTestCommand");

	const std::string filesMarker = "\"files\"";
	size_t pos = text.find(filesMarker);
	if (pos != std::string::npos)
		pos = text.find('{', pos + filesMarker.size());
	if (pos != std::string::npos) {
		while (pos < text.size()) {
			pos = text.find('"', pos + 1);
			if (pos == std::string::npos)
				break;
			const size_t keyStart = pos + 1;
			const size_t keyEnd = text.find('"', keyStart);
			if (keyEnd == std::string::npos)
				break;
			const std::string relativePath = text.substr(keyStart, keyEnd - keyStart);
			pos = text.find('{', keyEnd);
			if (pos == std::string::npos)
				break;
			int depth = 1;
			size_t end = pos + 1;
			while (end < text.size() && depth > 0) {
				if (text[end] == '{')
					++depth;
				else if (text[end] == '}')
					--depth;
				++end;
			}
			if (depth != 0)
				break;

			const std::string object = text.substr(pos, end - pos);
			ProjectFileMetadata metadata;
			metadata.relativePath = relativePath;
			metadata.language = extractObjectString(object, "language");
			metadata.summary = extractObjectString(object, "summary");
			metadata.riskLevel = extractObjectString(object, "risk");
			metadata.lastReviewed = extractObjectString(object, "lastReviewedAt");
			if (!relativePath.empty())
				fFiles[relativePath] = metadata;
			pos = end;
		}
	}
	return true;
}

bool
ProjectMemory::UpdateFile(const ProjectFileMetadata& metadata, std::string& error)
{
	fFiles[metadata.relativePath] = metadata;
	return Save(error);
}

bool
ProjectMemory::UpdateFiles(const std::vector<ProjectFileMetadata>& files,
	std::string& error)
{
	for (const ProjectFileMetadata& file : files)
		fFiles[file.relativePath] = file;
	return Save(error);
}

bool
ProjectMemory::SetDefaultBuildCommand(const std::string& command,
	std::string& error)
{
	fDefaultBuildCommand = command;
	return Save(error);
}

bool
ProjectMemory::SetDefaultTestCommand(const std::string& command,
	std::string& error)
{
	fDefaultTestCommand = command;
	return Save(error);
}

bool
ProjectMemory::SaveSession(const std::string& text, std::string& path,
	std::string& error) const
{
	path = HaikodeDirectory() + "/sessions/session-" + TimeUtils::CompactTimestamp()
		+ ".json";
	return WriteText(path, text, error);
}

bool
ProjectMemory::SaveCommandLog(const std::string& text, std::string& path,
	std::string& error) const
{
	path = LogsDirectory() + "/command-" + TimeUtils::CompactTimestamp() + ".log";
	return WriteText(path, text, error);
}

bool
ProjectMemory::WriteText(const std::string& path, const std::string& text,
	std::string& error) const
{
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file) {
		error = "Could not write project memory file.";
		return false;
	}
	file << text;
	return static_cast<bool>(file);
}

bool
ProjectMemory::Save(std::string& error) const
{
	std::ostringstream json;
	json << "{\n"
		<< "  \"schemaVersion\": 1,\n"
		<< "  \"projectName\": \"" << jsonEscape(fProjectName) << "\",\n"
		<< "  \"projectRoot\": \"" << jsonEscape(fProjectRoot) << "\",\n"
		<< "  \"createdAt\": \"" << jsonEscape(fCreatedAt) << "\",\n"
		<< "  \"updatedAt\": \"" << jsonEscape(TimeUtils::IsoTimestamp()) << "\",\n"
		<< "  \"defaultBuildCommand\": \"" << jsonEscape(fDefaultBuildCommand) << "\",\n"
		<< "  \"defaultTestCommand\": \"" << jsonEscape(fDefaultTestCommand) << "\",\n"
		<< "  \"provider\": {\"type\":\"openai-compatible\",\"baseUrl\":\"\",\"model\":\"\"},\n"
		<< "  \"ignore\": [";
	for (size_t i = 0; i < fIgnoreRules.Patterns().size(); ++i) {
		if (i != 0)
			json << ", ";
		json << "\"" << jsonEscape(fIgnoreRules.Patterns()[i]) << "\"";
	}
	json << "],\n  \"files\": {\n";
	size_t index = 0;
	for (const auto& item : fFiles) {
		const ProjectFileMetadata& file = item.second;
		if (index++ != 0)
			json << ",\n";
		json << "    \"" << jsonEscape(item.first) << "\": {"
			<< "\"language\":\"" << jsonEscape(file.language) << "\","
			<< "\"summary\":\"" << jsonEscape(file.summary) << "\","
			<< "\"risk\":\"" << jsonEscape(file.riskLevel) << "\","
			<< "\"reviewStatus\":\"unreviewed\","
			<< "\"lastReviewedAt\":\"" << jsonEscape(file.lastReviewed) << "\","
			<< "\"lastModifiedAt\":\"\","
			<< "\"todos\":[],"
			<< "\"tags\":[]"
			<< "}";
	}
	json << "\n  }\n}\n";

	return WriteText(ProjectJsonPath(), json.str(), error);
}
