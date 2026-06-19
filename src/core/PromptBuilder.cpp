#include "core/PromptBuilder.h"

#include <sstream>

PromptBuildResult
PromptBuilder::Build(const PromptContext& context, size_t maxBytesPerFile,
	size_t maxFiles) const
{
	PromptBuildResult result;
	std::ostringstream prompt;
	prompt << "You are helping with a native Haiku OS project.\n"
		<< "Prefer native Haiku APIs and simple C++.\n"
		<< "Avoid Electron unless the user explicitly asks.\n"
		<< "Keep UI consistent with Haiku desktop conventions.\n"
		<< "Do not assume Linux-only paths or dependencies.\n"
		<< "Prefer small, reviewable patches.\n";

	if (context.mode == PromptMode::GeneratePatch) {
		prompt << "When changing files, output a unified diff only, with paths "
			<< "relative to the project root.\n";
	} else if (context.mode == PromptMode::SummarizeProject) {
		prompt << "Summarize the project and suggest build/test commands if clear.\n";
	} else {
		prompt << "Explain selected code clearly and do not modify files.\n";
	}

	prompt << "\nProject root: " << context.projectRoot << "\n\n";

	size_t included = 0;
	for (const PromptFileContext& file : context.files) {
		if (file.ignored)
			continue;
		if (included >= maxFiles) {
			result.truncated = true;
			result.warnings.push_back("Some selected files were omitted by the context file limit.");
			break;
		}

		std::string text = file.text;
		if (text.size() > maxBytesPerFile) {
			text.resize(maxBytesPerFile);
			result.truncated = true;
			result.warnings.push_back(file.relativePath + " was truncated by the context size limit.");
		}

		prompt << "File: " << file.relativePath << "\n```text\n"
			<< text << "\n```\n\n";
		++included;
	}

	prompt << "User request:\n" << context.userPrompt << "\n";
	result.prompt = prompt.str();
	return result;
}
