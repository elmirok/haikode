#ifndef HAIKODE_CORE_PROMPT_BUILDER_H
#define HAIKODE_CORE_PROMPT_BUILDER_H

#include <string>
#include <vector>

enum class PromptMode {
	ExplainSelectedFile,
	SummarizeProject,
	GeneratePatch
};

struct PromptFileContext {
	std::string relativePath;
	std::string text;
	bool ignored = false;
};

struct PromptContext {
	PromptMode mode = PromptMode::ExplainSelectedFile;
	std::string projectRoot;
	std::string userPrompt;
	std::vector<PromptFileContext> files;
};

struct PromptBuildResult {
	std::string prompt;
	bool truncated = false;
	std::vector<std::string> warnings;
};

class PromptBuilder {
public:
	PromptBuildResult Build(const PromptContext& context,
		size_t maxBytesPerFile, size_t maxFiles) const;
};

#endif
