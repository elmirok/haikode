#include "core/IgnoreRules.h"

#include <algorithm>

namespace {

std::string
trimLeadingSlash(std::string path)
{
	while (!path.empty() && path[0] == '/')
		path.erase(path.begin());
	return path;
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
startsWith(const std::string& value, const std::string& prefix)
{
	return value.compare(0, prefix.size(), prefix) == 0;
}

bool
endsWith(const std::string& value, const std::string& suffix)
{
	return value.size() >= suffix.size()
		&& value.compare(value.size() - suffix.size(), suffix.size(), suffix) == 0;
}

bool
matchesPattern(const std::string& path, const std::string& pattern)
{
	if (pattern.empty())
		return false;

	if (pattern[0] == '*')
		return endsWith(baseName(path), pattern.substr(1));

	if (path == pattern || startsWith(path, pattern + "/"))
		return true;

	return path.find("/" + pattern + "/") != std::string::npos
		|| endsWith(path, "/" + pattern);
}

}

IgnoreRules::IgnoreRules()
	:
	fPatterns({
		".git",
		".haikode/cache",
		".haikode/tmp",
		"node_modules",
		"vendor",
		"build",
		"dist",
		"out",
		"target",
		".cache",
		".DS_Store",
		"*.o",
		"*.a",
		"*.so",
		"*.hpkg"
	})
{
}

void
IgnoreRules::SetPatterns(const std::vector<std::string>& patterns)
{
	fPatterns = patterns;
}

bool
IgnoreRules::ShouldIgnore(const std::string& relativePath) const
{
	const std::string path = trimLeadingSlash(relativePath);
	for (const std::string& pattern : fPatterns) {
		if (matchesPattern(path, pattern))
			return true;
	}
	return false;
}
