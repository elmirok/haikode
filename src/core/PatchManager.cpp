#include "core/PatchManager.h"

#include "core/IgnoreRules.h"
#include "core/TimeUtils.h"

#include <algorithm>
#include <cerrno>
#include <fstream>
#include <iterator>
#include <limits.h>
#include <map>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace {

struct HunkLine {
	char kind = ' ';
	std::string text;
};

struct FilePatch {
	std::string oldPath;
	std::string newPath;
	std::vector<HunkLine> lines;
	bool isNewFile = false;
	bool isDelete = false;
};

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
stripPrefix(std::string path)
{
	if (path.compare(0, 2, "a/") == 0 || path.compare(0, 2, "b/") == 0)
		path = path.substr(2);
	const size_t tab = path.find('\t');
	if (tab != std::string::npos)
		path = path.substr(0, tab);
	return path;
}

bool
hasParentComponent(const std::string& path)
{
	std::istringstream parts(path);
	std::string part;
	while (std::getline(parts, part, '/')) {
		if (part == "..")
			return true;
	}
	return false;
}

bool
isTextByte(unsigned char c)
{
	return c == '\n' || c == '\r' || c == '\t' || (c >= 32 && c != 127);
}

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

bool
isTextFile(const std::string& path)
{
	std::string text;
	if (!readFile(path, text))
		return false;
	for (unsigned char c : text) {
		if (!isTextByte(c))
			return false;
	}
	return true;
}

std::string
parentDirectory(const std::string& path)
{
	const size_t slash = path.find_last_of('/');
	if (slash == std::string::npos)
		return ".";
	return path.substr(0, slash);
}

std::string
baseName(const std::string& path)
{
	const size_t slash = path.find_last_of('/');
	if (slash == std::string::npos)
		return path;
	return path.substr(slash + 1);
}

bool
copyFile(const std::string& from, const std::string& to)
{
	std::string text;
	if (!readFile(from, text))
		return false;
	makeDirectory(parentDirectory(to));
	return writeFile(to, text);
}

bool
parsePatch(const std::string& diff, std::vector<FilePatch>& files)
{
	files.clear();
	std::istringstream lines(diff);
	std::string line;
	FilePatch current;
	bool haveFile = false;
	bool inHunk = false;

	while (std::getline(lines, line)) {
		if (line.compare(0, 4, "--- ") == 0) {
			if (haveFile)
				files.push_back(current);
			current = FilePatch();
			current.oldPath = stripPrefix(line.substr(4));
			haveFile = true;
			inHunk = false;
			continue;
		}
		if (haveFile && line.compare(0, 4, "+++ ") == 0) {
			current.newPath = stripPrefix(line.substr(4));
			current.isNewFile = current.oldPath == "/dev/null";
			current.isDelete = current.newPath == "/dev/null";
			continue;
		}
		if (!haveFile)
			continue;
		if (line.compare(0, 2, "@@") == 0) {
			inHunk = true;
			continue;
		}
		if (inHunk && !line.empty()) {
			const char kind = line[0];
			if (kind == ' ' || kind == '-' || kind == '+')
				current.lines.push_back({kind, line.substr(1)});
			else if (line[0] == '\\') {
				continue;
			}
		}
	}

	if (haveFile)
		files.push_back(current);
	return !files.empty();
}

bool
isSafeRelativePath(const std::string& path)
{
	return !path.empty() && path != "/dev/null" && path[0] != '/'
		&& !hasParentComponent(path);
}

bool
isSensitivePath(const std::string& path)
{
	const std::string name = baseName(path);
	return name == ".env" || name == "id_rsa" || name == "id_ed25519"
		|| name == "authorized_keys"
		|| (name.size() >= 4 && name.substr(name.size() - 4) == ".key")
		|| (name.size() >= 4 && name.substr(name.size() - 4) == ".pem")
		|| (name.size() >= 4 && name.substr(name.size() - 4) == ".crt");
}

std::vector<std::string>
splitLines(const std::string& text)
{
	std::vector<std::string> lines;
	std::istringstream input(text);
	std::string line;
	while (std::getline(input, line))
		lines.push_back(line);
	if (!text.empty() && text[text.size() - 1] == '\n') {
		// getline already returned the final content line; no extra sentinel needed.
	}
	return lines;
}

std::string
joinLines(const std::vector<std::string>& lines)
{
	std::ostringstream out;
	for (const std::string& line : lines)
		out << line << "\n";
	return out.str();
}

bool
applyFilePatch(const std::string& path, const FilePatch& patch,
	std::string& error)
{
	std::string currentText;
	if (!patch.isNewFile && !readFile(path, currentText)) {
		error = "Could not read file while applying patch.";
		return false;
	}

	std::vector<std::string> current = patch.isNewFile
		? std::vector<std::string>() : splitLines(currentText);
	std::vector<std::string> output;
	size_t cursor = 0;

	for (const HunkLine& line : patch.lines) {
		if (line.kind == ' ') {
			while (cursor < current.size() && current[cursor] != line.text)
				output.push_back(current[cursor++]);
			if (cursor >= current.size()) {
				error = "Patch context did not match the current file.";
				return false;
			}
			output.push_back(current[cursor++]);
		} else if (line.kind == '-') {
			while (cursor < current.size() && current[cursor] != line.text)
				output.push_back(current[cursor++]);
			if (cursor >= current.size()) {
				error = "Patch removal did not match the current file.";
				return false;
			}
			++cursor;
		} else if (line.kind == '+') {
			output.push_back(line.text);
		}
	}

	while (cursor < current.size())
		output.push_back(current[cursor++]);

	makeDirectory(parentDirectory(path));
	if (!writeFile(path, joinLines(output))) {
		error = "Could not write patched file.";
		return false;
	}
	return true;
}

}

bool
PatchManager::SavePatch(const std::string& projectRoot, const std::string& diff,
	std::string& patchPath, std::string& error) const
{
	error.clear();
	const std::string directory = projectRoot + "/.haikode/patches";
	if (!makeDirectory(projectRoot + "/.haikode") || !makeDirectory(directory)) {
		error = "Could not create patch directory.";
		return false;
	}

	patchPath = directory + "/patch-" + TimeUtils::CompactTimestamp() + ".diff";
	if (!writeFile(patchPath, diff)) {
		error = "Could not save patch file.";
		return false;
	}
	return true;
}

bool
PatchManager::Validate(const std::string& projectRoot, const std::string& diff,
	PatchValidation& validation, std::string& error) const
{
	validation = PatchValidation();
	error.clear();

	std::vector<FilePatch> patches;
	if (!parsePatch(diff, patches)) {
		error = "Patch did not contain a valid unified diff.";
		return false;
	}

	IgnoreRules rules;
	for (const FilePatch& patch : patches) {
		const std::string path = patch.newPath == "/dev/null" ? patch.oldPath
			: patch.newPath;
		if (!isSafeRelativePath(path)) {
			error = "Patch contains a path outside the project.";
			return false;
		}
		if (rules.ShouldIgnore(path)) {
			error = "Patch touches an ignored file.";
			return false;
		}
		if (patch.isDelete) {
			error = "Deleting files through patches is not enabled in this MVP.";
			return false;
		}

		const std::string absolutePath = projectRoot + "/" + path;
		if (!patch.isNewFile && access(absolutePath.c_str(), F_OK) != 0) {
			error = "Patch touches a missing file.";
			return false;
		}
		if (!patch.isNewFile && !isTextFile(absolutePath)) {
			error = "Patch touches a binary or unsupported file.";
			return false;
		}

		if (isSensitivePath(path))
			validation.warnings.push_back("Patch touches sensitive file: " + path);
		validation.files.push_back(path);
	}

	if (validation.files.size() > 5)
		validation.warnings.push_back("Patch touches many files.");
	return true;
}

bool
PatchManager::Apply(const std::string& projectRoot, const std::string& diff,
	PatchApplyResult& result, std::string& error) const
{
	result = PatchApplyResult();
	PatchValidation validation;
	if (!Validate(projectRoot, diff, validation, error))
		return false;

	std::vector<FilePatch> patches;
	parsePatch(diff, patches);

	result.backupDirectory = projectRoot + "/.haikode/backups/"
		+ TimeUtils::CompactTimestamp();
	if (!makeDirectory(projectRoot + "/.haikode")
		|| !makeDirectory(projectRoot + "/.haikode/backups")
		|| !makeDirectory(result.backupDirectory)) {
		error = "Could not create backup directory.";
		return false;
	}

	for (const FilePatch& patch : patches) {
		const std::string path = patch.newPath == "/dev/null" ? patch.oldPath
			: patch.newPath;
		const std::string absolutePath = projectRoot + "/" + path;
		if (!patch.isNewFile) {
			const std::string backupPath = result.backupDirectory + "/" + path;
			if (!copyFile(absolutePath, backupPath)) {
				error = "Could not create patch backup.";
				return false;
			}
		}
	}

	for (const FilePatch& patch : patches) {
		const std::string path = patch.newPath == "/dev/null" ? patch.oldPath
			: patch.newPath;
		if (!applyFilePatch(projectRoot + "/" + path, patch, error))
			return false;
		result.changedFiles.push_back(path);
	}

	return true;
}
