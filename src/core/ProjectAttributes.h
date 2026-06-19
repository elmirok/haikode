#ifndef HAIKODE_CORE_PROJECT_ATTRIBUTES_H
#define HAIKODE_CORE_PROJECT_ATTRIBUTES_H

#include "core/ProjectScanner.h"

#include <string>

class ProjectAttributes {
public:
	bool WriteMetadata(const ProjectFileMetadata& metadata,
		std::string& error) const;

private:
	bool WriteStringAttribute(const std::string& path, const char* name,
		const std::string& value, std::string& error) const;
};

#endif

