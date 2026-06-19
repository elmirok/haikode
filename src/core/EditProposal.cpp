#include "core/EditProposal.h"

#include "core/Crypto.h"
#include "core/ProjectModel.h"

#include <cctype>
#include <fstream>
#include <iterator>
#include <map>
#include <sstream>

namespace {

bool
readFile(const std::string& path, std::string& text)
{
	std::ifstream file(path, std::ios::binary);
	if (!file)
		return false;
	text.assign(std::istreambuf_iterator<char>(file),
		std::istreambuf_iterator<char>());
	return true;
}

bool
writeFile(const std::string& path, const std::string& text)
{
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file)
		return false;
	file << text;
	return static_cast<bool>(file);
}

void
skipWhitespace(const std::string& json, size_t& pos)
{
	while (pos < json.size()
		&& std::isspace(static_cast<unsigned char>(json[pos]))) {
		++pos;
	}
}

bool
parseJsonString(const std::string& json, size_t& pos, std::string& out)
{
	out.clear();
	if (pos >= json.size() || json[pos] != '"')
		return false;
	++pos;

	while (pos < json.size()) {
		const char c = json[pos++];
		if (c == '"')
			return true;
		if (c != '\\') {
			out.push_back(c);
			continue;
		}
		if (pos >= json.size())
			return false;
		const char escaped = json[pos++];
		switch (escaped) {
			case '"':
			case '\\':
			case '/':
				out.push_back(escaped);
				break;
			case 'b':
				out.push_back('\b');
				break;
			case 'f':
				out.push_back('\f');
				break;
			case 'n':
				out.push_back('\n');
				break;
			case 'r':
				out.push_back('\r');
				break;
			case 't':
				out.push_back('\t');
				break;
			default:
				return false;
		}
	}
	return false;
}

bool
parseFlatJsonObject(const std::string& json, std::map<std::string, std::string>& out)
{
	out.clear();
	size_t pos = 0;
	skipWhitespace(json, pos);
	if (pos >= json.size() || json[pos] != '{')
		return false;
	++pos;

	while (pos < json.size()) {
		skipWhitespace(json, pos);
		if (pos < json.size() && json[pos] == '}')
			return true;

		std::string key;
		if (!parseJsonString(json, pos, key))
			return false;
		skipWhitespace(json, pos);
		if (pos >= json.size() || json[pos] != ':')
			return false;
		++pos;
		skipWhitespace(json, pos);

		std::string value;
		if (!parseJsonString(json, pos, value))
			return false;
		out[key] = value;

		skipWhitespace(json, pos);
		if (pos < json.size() && json[pos] == ',') {
			++pos;
			continue;
		}
		if (pos < json.size() && json[pos] == '}')
			return true;
		return false;
	}
	return false;
}

bool
extractFence(const std::string& response, std::string& json)
{
	const std::string startMarker = "```haikode-edit";
	const size_t start = response.find(startMarker);
	if (start == std::string::npos)
		return false;

	size_t jsonStart = response.find('\n', start + startMarker.size());
	if (jsonStart == std::string::npos)
		return false;
	++jsonStart;

	const size_t end = response.find("```", jsonStart);
	if (end == std::string::npos)
		return false;

	json = response.substr(jsonStart, end - jsonStart);
	return true;
}

}

bool
EditProposal::ParseFromResponse(const std::string& response,
	EditProposal& proposal)
{
	std::string json;
	if (!extractFence(response, json))
		return false;

	std::map<std::string, std::string> fields;
	if (!parseFlatJsonObject(json, fields))
		return false;

	if (fields["path"].empty() || fields["summary"].empty()
		|| fields["original_sha256"].empty()
		|| fields.find("replacement") == fields.end()) {
		return false;
	}

	proposal.path = fields["path"];
	proposal.summary = fields["summary"];
	proposal.originalSha256 = fields["original_sha256"];
	proposal.replacement = fields["replacement"];
	return true;
}

bool
EditProposal::IsForSelectedFile(const ProjectModel& project) const
{
	return path == project.SelectedRelativePath();
}

bool
EditProposal::Apply(const ProjectModel& project, std::string& error) const
{
	error.clear();
	if (!IsForSelectedFile(project)) {
		error = "Edit proposal is not for the selected file.";
		return false;
	}

	std::string absolutePath;
	if (!project.ResolveRelativePath(path, absolutePath)) {
		error = "Edit proposal path is outside the project.";
		return false;
	}

	std::string current;
	if (!readFile(absolutePath, current)) {
		error = "Could not read selected file.";
		return false;
	}

	if (Crypto::Sha256Hex(current) != originalSha256) {
		error = "Selected file changed since the proposal was created.";
		return false;
	}

	if (!writeFile(absolutePath + ".haikode.bak", current)) {
		error = "Could not write backup file.";
		return false;
	}

	if (!writeFile(absolutePath, replacement)) {
		error = "Could not write replacement file.";
		return false;
	}

	return true;
}
