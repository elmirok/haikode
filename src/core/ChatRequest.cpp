#include "core/ChatRequest.h"

#include <sstream>

namespace {

std::string
jsonEscape(const std::string& value)
{
	std::ostringstream out;
	for (unsigned char c : value) {
		switch (c) {
			case '"':
				out << "\\\"";
				break;
			case '\\':
				out << "\\\\";
				break;
			case '\n':
				out << "\\n";
				break;
			case '\r':
				out << "\\r";
				break;
			case '\t':
				out << "\\t";
				break;
			default:
				if (c < 32) {
					static const char* digits = "0123456789abcdef";
					out << "\\u00" << digits[(c >> 4) & 0xf] << digits[c & 0xf];
				} else {
					out << c;
				}
				break;
		}
	}
	return out.str();
}

}

std::string
ChatRequest::ToChatCompletionsJson() const
{
	std::ostringstream user;
	user << "Project root: " << projectRoot << "\n";
	if (!selectedPath.empty()) {
		user << "Selected file: " << selectedPath << "\n\n"
			<< "Selected file content:\n```text\n"
			<< selectedText << "\n```\n\n";
	}
	user << "User request:\n" << userPrompt;

	std::ostringstream json;
	json << "{"
		<< "\"model\":\"" << jsonEscape(model) << "\","
		<< "\"messages\":["
		<< "{\"role\":\"system\",\"content\":\""
		<< "You are Haikode, a manual coding assistant for Haiku OS developers. "
		<< "Never claim to run shell commands. If you propose an edit, return at "
		<< "most one fenced haikode-edit JSON block for the selected file only."
		<< "\"},"
		<< "{\"role\":\"user\",\"content\":\"" << jsonEscape(user.str()) << "\"}"
		<< "],"
		<< "\"stream\":false"
		<< "}";
	return json.str();
}

