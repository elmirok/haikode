#ifndef HAIKODE_CORE_IGNORE_RULES_H
#define HAIKODE_CORE_IGNORE_RULES_H

#include <string>
#include <vector>

class IgnoreRules {
public:
	IgnoreRules();

	bool ShouldIgnore(const std::string& relativePath) const;
	const std::vector<std::string>& Patterns() const { return fPatterns; }
	void SetPatterns(const std::vector<std::string>& patterns);

private:
	std::vector<std::string> fPatterns;
};

#endif
