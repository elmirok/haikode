#ifndef HAIKODE_CORE_CHAT_REQUEST_H
#define HAIKODE_CORE_CHAT_REQUEST_H

#include <string>

struct ChatRequest {
	std::string model;
	std::string projectRoot;
	std::string selectedPath;
	std::string selectedText;
	std::string userPrompt;

	std::string ToChatCompletionsJson() const;
};

#endif

