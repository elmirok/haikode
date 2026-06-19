#include "core/DiffProposal.h"

#include <limits.h>
#include <sstream>
#include <unistd.h>

namespace {

std::string
stripDiffPrefix(std::string path)
{
	if (path.compare(0, 2, "a/") == 0 || path.compare(0, 2, "b/") == 0)
		path = path.substr(2);
	const size_t tab = path.find('\t');
	if (tab != std::string::npos)
		path = path.substr(0, tab);
	return path;
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
isSafeRelativePath(const std::string& path)
{
	return !path.empty() && path != "/dev/null" && path[0] != '/'
		&& !hasParentComponent(path);
}

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
hasPathPrefix(const std::string& path, const std::string& root)
{
	if (path == root)
		return true;
	if (path.size() <= root.size())
		return false;
	return path.compare(0, root.size(), root) == 0 && path[root.size()] == '/';
}

}

bool
DiffProposal::ParseUnifiedDiff(const std::string& diff, DiffProposal& proposal)
{
	proposal.fFiles.clear();
	std::istringstream lines(diff);
	std::string line;
	DiffFileProposal current;
	bool haveOld = false;

	while (std::getline(lines, line)) {
		if (line.compare(0, 4, "--- ") == 0) {
			current = DiffFileProposal();
			current.oldPath = stripDiffPrefix(line.substr(4));
			haveOld = true;
			continue;
		}
		if (haveOld && line.compare(0, 4, "+++ ") == 0) {
			current.newPath = stripDiffPrefix(line.substr(4));
			proposal.fFiles.push_back(current);
			haveOld = false;
		}
	}

	return !proposal.fFiles.empty();
}

bool
DiffProposal::ValidatePaths(const std::string& projectRoot,
	std::string& error) const
{
	error.clear();
	for (const DiffFileProposal& file : fFiles) {
		const std::string path = file.newPath == "/dev/null" ? file.oldPath
			: file.newPath;
		if (!isSafeRelativePath(path)) {
			error = "Diff proposal contains a path outside the project.";
			return false;
		}
	}

	std::string root;
	if (!canonicalPath(projectRoot, root)) {
		error = "Could not resolve project root.";
		return false;
	}

	for (const DiffFileProposal& file : fFiles) {
		const std::string path = file.newPath == "/dev/null" ? file.oldPath
			: file.newPath;

		std::string resolvedParent;
		const size_t slash = path.find_last_of('/');
		const std::string parent = slash == std::string::npos ? root
			: root + "/" + path.substr(0, slash);
		if (!canonicalPath(parent, resolvedParent)
			|| !hasPathPrefix(resolvedParent, root)) {
			error = "Diff proposal contains a path outside the project.";
			return false;
		}
	}

	return true;
}
