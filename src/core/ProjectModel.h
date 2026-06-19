#ifndef HAIKODE_CORE_PROJECT_MODEL_H
#define HAIKODE_CORE_PROJECT_MODEL_H

#include <string>

class ProjectModel {
public:
	bool SetRoot(const std::string& path);
	const std::string& RootPath() const { return fRootPath; }

	bool IsInsideRoot(const std::string& path) const;
	bool SelectFile(const std::string& path);
	const std::string& SelectedPath() const { return fSelectedPath; }
	std::string SelectedRelativePath() const;
	bool ReadSelectedText(size_t maxBytes, std::string& text) const;
	bool ResolveRelativePath(const std::string& relativePath,
		std::string& absolutePath) const;

private:
	std::string fRootPath;
	std::string fSelectedPath;
};

#endif

