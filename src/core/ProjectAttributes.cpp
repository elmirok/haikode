#include "core/ProjectAttributes.h"

#ifdef __HAIKU__
#include <Node.h>
#include <TypeConstants.h>
#endif

#include <sstream>

bool
ProjectAttributes::WriteMetadata(const ProjectFileMetadata& metadata,
	std::string& error) const
{
	return WriteStringAttribute(metadata.path, "haikode:summary", metadata.summary,
			error)
		&& WriteStringAttribute(metadata.path, "haikode:language",
			metadata.language, error)
		&& WriteStringAttribute(metadata.path, "haikode:role", metadata.role,
			error)
		&& WriteStringAttribute(metadata.path, "haikode:risk_level",
			metadata.riskLevel, error)
		&& WriteStringAttribute(metadata.path, "haikode:test_status",
			metadata.testStatus, error)
		&& WriteStringAttribute(metadata.path, "haikode:last_reviewed",
			metadata.lastReviewed, error)
		&& WriteStringAttribute(metadata.path, "haikode:todo", metadata.todo,
			error)
		&& WriteStringAttribute(metadata.path, "haikode:ai_notes",
			metadata.aiNotesStale ? "stale" : "fresh", error);
}

bool
ProjectAttributes::WriteStringAttribute(const std::string& path, const char* name,
	const std::string& value, std::string& error) const
{
#ifdef __HAIKU__
	BNode node(path.c_str());
	if (node.InitCheck() != B_OK) {
		error = "Could not open file for BFS attributes.";
		return false;
	}

	const ssize_t written = node.WriteAttr(name, B_STRING_TYPE, 0, value.c_str(),
		value.size() + 1);
	if (written < 0) {
		std::ostringstream out;
		out << "Could not write BFS attribute " << name << ".";
		error = out.str();
		return false;
	}
	return true;
#else
	(void)path;
	(void)name;
	(void)value;
	error.clear();
	return true;
#endif
}

