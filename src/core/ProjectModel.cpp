#include "core/ProjectModel.h"

#include <cerrno>
#include <climits>
#include <cstring>
#include <fstream>
#include <iterator>
#include <sstream>
#include <sys/stat.h>
#include <unistd.h>

namespace {

bool
canonicalPath(const std::string& path, std::string& out)
{
	char resolved[PATH_MAX];
	if (realpath(path.c_str(), resolved) == nullptr)
		return false;
	out = resolved;
	return true;
}

bool
isDirectory(const std::string& path)
{
	struct stat info;
	return stat(path.c_str(), &info) == 0 && S_ISDIR(info.st_mode);
}

bool
isRegularFile(const std::string& path)
{
	struct stat info;
	return stat(path.c_str(), &info) == 0 && S_ISREG(info.st_mode);
}

bool
hasPathPrefix(const std::string& path, const std::string& root)
{
	if (path == root)
		return true;
	if (path.size() <= root.size())
		return false;
	return path.compare(0, root.size(), root) == 0 && path[root.size()] == '/';
}

bool
hasParentComponent(const std::string& relativePath)
{
	std::istringstream parts(relativePath);
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

}

bool
ProjectModel::SetRoot(const std::string& path)
{
	std::string resolved;
	if (!canonicalPath(path, resolved) || !isDirectory(resolved))
		return false;

	fRootPath = resolved;
	fSelectedPath.clear();
	return true;
}

bool
ProjectModel::IsInsideRoot(const std::string& path) const
{
	if (fRootPath.empty())
		return false;

	std::string resolved;
	if (!canonicalPath(path, resolved))
		return false;

	return hasPathPrefix(resolved, fRootPath);
}

bool
ProjectModel::SelectFile(const std::string& path)
{
	std::string resolved;
	if (!canonicalPath(path, resolved) || !isRegularFile(resolved))
		return false;
	if (!hasPathPrefix(resolved, fRootPath))
		return false;

	fSelectedPath = resolved;
	return true;
}

std::string
ProjectModel::SelectedRelativePath() const
{
	if (fSelectedPath.empty() || !hasPathPrefix(fSelectedPath, fRootPath))
		return std::string();
	if (fSelectedPath == fRootPath)
		return std::string();
	return fSelectedPath.substr(fRootPath.size() + 1);
}

bool
ProjectModel::ReadSelectedText(size_t maxBytes, std::string& text) const
{
	text.clear();
	if (fSelectedPath.empty())
		return false;

	std::ifstream file(fSelectedPath, std::ios::binary);
	if (!file)
		return false;

	text.assign(std::istreambuf_iterator<char>(file),
		std::istreambuf_iterator<char>());
	if (text.size() > maxBytes)
		text.resize(maxBytes);

	for (unsigned char c : text) {
		if (!isTextByte(c))
			return false;
	}

	return true;
}

bool
ProjectModel::ResolveRelativePath(const std::string& relativePath,
	std::string& absolutePath) const
{
	absolutePath.clear();
	if (fRootPath.empty() || relativePath.empty() || relativePath[0] == '/')
		return false;
	if (hasParentComponent(relativePath))
		return false;

	const std::string candidate = fRootPath + "/" + relativePath;
	std::string resolved;
	if (!canonicalPath(candidate, resolved))
		return false;
	if (!hasPathPrefix(resolved, fRootPath))
		return false;

	absolutePath = resolved;
	return true;
}
