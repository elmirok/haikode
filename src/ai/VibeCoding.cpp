/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "VibeCoding.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>

namespace Haikode::AI {

namespace fs = std::filesystem;

namespace {

std::string
Timestamp()
{
	static std::atomic<unsigned long long> sSequence {0};
	const auto now = std::chrono::system_clock::now().time_since_epoch();
	return std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(
		now).count()) + "-" + std::to_string(++sSequence);
}


std::string
EscapeJson(const std::string& value)
{
	std::string escaped;
	for (const char c : value) {
		switch (c) {
			case '\\':
				escaped += "\\\\";
				break;
			case '"':
				escaped += "\\\"";
				break;
			case '\n':
				escaped += "\\n";
				break;
			case '\t':
				escaped += "\\t";
				break;
			default:
				escaped.push_back(c);
				break;
		}
	}
	return escaped;
}


bool
IsSecretTokenChar(char c)
{
	return std::isalnum(static_cast<unsigned char>(c)) || c == '-' || c == '_'
		|| c == '.' || c == ':';
}


void
RedactTokenAfterMarker(std::string& value, const std::string& marker)
{
	static const std::string kRedaction = "[redacted-secret]";
	size_t pos = 0;
	while ((pos = value.find(marker, pos)) != std::string::npos) {
		const size_t tokenStart = pos + marker.size();
		size_t tokenEnd = tokenStart;
		while (tokenEnd < value.size() && IsSecretTokenChar(value[tokenEnd]))
			tokenEnd++;
		if (tokenEnd > tokenStart) {
			value.replace(tokenStart, tokenEnd - tokenStart, kRedaction);
			pos = tokenStart + kRedaction.size();
		} else {
			pos = tokenStart;
		}
	}
}


std::string
RedactSecrets(std::string value)
{
	static const std::string kRedaction = "[redacted-secret]";
	const std::vector<std::string> markers = {
		"Authorization: Bearer ",
		"authorization: bearer ",
		"Bearer ",
		"bearer ",
		"access_token=",
		"oauth_token=",
		"api_key="
	};
	for (const std::string& marker : markers)
		RedactTokenAfterMarker(value, marker);

	size_t pos = 0;
	while ((pos = value.find("sk-", pos)) != std::string::npos) {
		size_t end = pos + 3;
		while (end < value.size() && IsSecretTokenChar(value[end]))
			end++;
		value.replace(pos, end - pos, kRedaction);
		pos += kRedaction.size();
	}
	return value;
}


bool
IsInsideDirectory(const fs::path& child, const fs::path& parent)
{
	const fs::path relative = fs::relative(child, parent);
	for (const fs::path& part : relative) {
		if (part == "..")
			return false;
	}
	return !relative.empty();
}


bool
IsSafeRelativePath(const fs::path& relativePath)
{
	if (relativePath.empty() || relativePath.is_absolute())
		return false;
	for (const fs::path& part : relativePath) {
		if (part == "." || part == "..")
			return false;
	}
	return true;
}


std::string
Trim(const std::string& value)
{
	size_t start = 0;
	while (start < value.size()
		&& std::isspace(static_cast<unsigned char>(value[start]))) {
		start++;
	}
	size_t end = value.size();
	while (end > start
		&& std::isspace(static_cast<unsigned char>(value[end - 1]))) {
		end--;
	}
	return value.substr(start, end - start);
}


std::string
ToLower(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return value;
}


std::string
RecordTypeForDirectory(const std::string& directory)
{
	if (directory == "sessions")
		return "session";
	if (directory == "logs")
		return "log";
	if (directory == "patches")
		return "patch";
	if (directory == "commands")
		return "command";
	return directory;
}


bool
ContainsTodoMarker(const std::string& text)
{
	return text.find("TODO") != std::string::npos
		|| text.find("FIXME") != std::string::npos
		|| text.find("HACK") != std::string::npos;
}


size_t
LineCount(const std::string& text)
{
	if (text.empty())
		return 0;
	return static_cast<size_t>(std::count(text.begin(), text.end(), '\n'))
		+ (text.back() == '\n' ? 0 : 1);
}


bool
ShouldSkipPathPart(const std::string& name)
{
	return name == ".git" || name == ".hg" || name == ".svn"
		|| name == ".haikode" || name == "build" || name == "dist"
		|| name == "out" || name == "target" || name == "node_modules"
		|| name == "vendor" || name == ".cache";
}


bool
ShouldSkipRelativePath(const fs::path& relativePath)
{
	for (const fs::path& part : relativePath) {
		if (ShouldSkipPathPart(part.string()))
			return true;
	}
	const std::string filename = relativePath.filename().string();
	const std::string lower = ToLower(filename);
	return lower == ".ds_store" || lower.ends_with(".o")
		|| lower.ends_with(".a") || lower.ends_with(".so")
		|| lower.ends_with(".hpkg") || lower.ends_with(".png")
		|| lower.ends_with(".jpg") || lower.ends_with(".jpeg")
		|| lower.ends_with(".gif") || lower.ends_with(".ico")
		|| lower.ends_with(".zip") || lower.ends_with(".tar")
		|| lower.ends_with(".gz");
}


std::string
LanguageForPath(const fs::path& relativePath)
{
	const std::string filename = relativePath.filename().string();
	const std::string extension = ToLower(relativePath.extension().string());
	if (extension == ".cpp" || extension == ".cc" || extension == ".cxx"
		|| extension == ".h" || extension == ".hpp" || extension == ".hh") {
		return "C++";
	}
	if (extension == ".c")
		return "C";
	if (extension == ".md" || ToLower(filename).starts_with("readme"))
		return "Markdown";
	if (extension == ".json")
		return "JSON";
	if (extension == ".yml" || extension == ".yaml")
		return "YAML";
	if (filename == "Makefile" || extension == ".mk")
		return "Make";
	if (extension == ".sh")
		return "Shell";
	return "Text";
}


std::string
RoleForPath(const fs::path& relativePath)
{
	const std::string filename = relativePath.filename().string();
	const std::string lowerFilename = ToLower(filename);
	const std::string extension = ToLower(relativePath.extension().string());
	if (extension == ".md" || lowerFilename.starts_with("readme"))
		return "docs";
	if (lowerFilename.find("test") != std::string::npos)
		return "test";
	if (filename == "Makefile" || extension == ".mk" || extension == ".sh")
		return "build";
	return "source";
}


std::string
RiskForPath(const fs::path& relativePath)
{
	const std::string path = ToLower(relativePath.generic_string());
	if (path.find("auth") != std::string::npos
		|| path.find("oauth") != std::string::npos
		|| path.find("provider") != std::string::npos
		|| path.find("command") != std::string::npos
		|| path.find("patch") != std::string::npos
		|| path.find("makefile") != std::string::npos) {
		return "high";
	}
	return "normal";
}


bool
ReadSmallTextFile(const fs::path& path, std::string& text)
{
	constexpr uintmax_t kMaxProjectMapFileBytes = 128 * 1024;
	std::error_code error;
	if (fs::file_size(path, error) > kMaxProjectMapFileBytes)
		return false;
	std::ifstream file(path, std::ios::binary);
	if (!file)
		return false;
	text.assign(std::istreambuf_iterator<char>(file),
		std::istreambuf_iterator<char>());
	return text.find('\0') == std::string::npos;
}


bool
InferMakefileCommands(const fs::path& root, ProjectMemory& memory)
{
	const std::vector<std::string> makefileNames = {
		"Makefile", "makefile", "GNUmakefile"
	};
	for (const std::string& name : makefileNames) {
		const fs::path makefilePath = root / name;
		std::error_code fsError;
		if (!fs::is_regular_file(makefilePath, fsError))
			continue;

		memory.defaultBuildCommand = "make";
		std::string text;
		if (ReadSmallTextFile(makefilePath, text)
			&& (text.rfind("test:", 0) == 0
				|| text.find("\ntest:") != std::string::npos)) {
			memory.defaultTestCommand = "make test";
		}
		return true;
	}
	return false;
}


bool
ReadJsonString(const std::string& text, size_t& pos, std::string& value)
{
	value.clear();
	if (pos >= text.size() || text[pos] != '"')
		return false;
	pos++;
	while (pos < text.size()) {
		const char c = text[pos++];
		if (c == '"')
			return true;
		if (c == '\\' && pos < text.size()) {
			const char escaped = text[pos++];
			switch (escaped) {
				case 'n':
					value.push_back('\n');
					break;
				case 't':
					value.push_back('\t');
					break;
				default:
					value.push_back(escaped);
					break;
			}
		} else {
			value.push_back(c);
		}
	}
	return false;
}


bool
ExtractJsonStringField(const std::string& json, const std::string& key,
	std::string& value)
{
	const std::string marker = "\"" + key + "\"";
	size_t pos = json.find(marker);
	if (pos == std::string::npos)
		return false;
	pos = json.find(':', pos + marker.size());
	if (pos == std::string::npos)
		return false;
	pos++;
	while (pos < json.size()
		&& std::isspace(static_cast<unsigned char>(json[pos]))) {
		pos++;
	}
	return ReadJsonString(json, pos, value);
}


bool
ExtractJsonStringArrayField(const std::string& json, const std::string& key,
	std::vector<std::string>& values)
{
	values.clear();
	const std::string marker = "\"" + key + "\"";
	size_t pos = json.find(marker);
	if (pos == std::string::npos)
		return false;
	pos = json.find(':', pos + marker.size());
	if (pos == std::string::npos)
		return false;
	pos++;
	while (pos < json.size()
		&& std::isspace(static_cast<unsigned char>(json[pos]))) {
		pos++;
	}
	if (pos >= json.size() || json[pos] != '[')
		return false;
	pos++;
	while (pos < json.size()) {
		while (pos < json.size()
			&& std::isspace(static_cast<unsigned char>(json[pos]))) {
			pos++;
		}
		if (pos < json.size() && json[pos] == ']')
			return true;

		std::string value;
		if (!ReadJsonString(json, pos, value))
			return false;
		values.push_back(value);

		while (pos < json.size()
			&& std::isspace(static_cast<unsigned char>(json[pos]))) {
			pos++;
		}
		if (pos < json.size() && json[pos] == ',') {
			pos++;
			continue;
		}
		if (pos < json.size() && json[pos] == ']')
			return true;
		return false;
	}
	return false;
}


bool
ExtractJsonBoolField(const std::string& json, const std::string& key,
	bool& value)
{
	const std::string marker = "\"" + key + "\"";
	size_t pos = json.find(marker);
	if (pos == std::string::npos)
		return false;
	pos = json.find(':', pos + marker.size());
	if (pos == std::string::npos)
		return false;
	pos++;
	while (pos < json.size()
		&& std::isspace(static_cast<unsigned char>(json[pos]))) {
		pos++;
	}
	if (json.compare(pos, 4, "true") == 0) {
		value = true;
		return true;
	}
	if (json.compare(pos, 5, "false") == 0) {
		value = false;
		return true;
	}
	return false;
}


bool
ExtractJsonSizeField(const std::string& json, const std::string& key,
	size_t& value)
{
	const std::string marker = "\"" + key + "\"";
	size_t pos = json.find(marker);
	if (pos == std::string::npos)
		return false;
	pos = json.find(':', pos + marker.size());
	if (pos == std::string::npos)
		return false;
	pos++;
	while (pos < json.size()
		&& std::isspace(static_cast<unsigned char>(json[pos]))) {
		pos++;
	}
	size_t parsed = 0;
	bool sawDigit = false;
	while (pos < json.size()
		&& std::isdigit(static_cast<unsigned char>(json[pos]))) {
		sawDigit = true;
		parsed = parsed * 10 + static_cast<size_t>(json[pos] - '0');
		pos++;
	}
	if (!sawDigit)
		return false;
	value = parsed;
	return true;
}


uint32_t
RotateRight(uint32_t value, uint32_t bits)
{
	return (value >> bits) | (value << (32 - bits));
}


std::string
Sha256Hex(const std::string& text)
{
	static const std::array<uint32_t, 64> k = {
		0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5,
		0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
		0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3,
		0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
		0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc,
		0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
		0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7,
		0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
		0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13,
		0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
		0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3,
		0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
		0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5,
		0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
		0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208,
		0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2
	};

	std::vector<uint8_t> bytes(text.begin(), text.end());
	const uint64_t bitLength = static_cast<uint64_t>(bytes.size()) * 8;
	bytes.push_back(0x80);
	while ((bytes.size() % 64) != 56)
		bytes.push_back(0);
	for (int shift = 56; shift >= 0; shift -= 8)
		bytes.push_back(static_cast<uint8_t>((bitLength >> shift) & 0xff));

	uint32_t h0 = 0x6a09e667;
	uint32_t h1 = 0xbb67ae85;
	uint32_t h2 = 0x3c6ef372;
	uint32_t h3 = 0xa54ff53a;
	uint32_t h4 = 0x510e527f;
	uint32_t h5 = 0x9b05688c;
	uint32_t h6 = 0x1f83d9ab;
	uint32_t h7 = 0x5be0cd19;

	for (size_t chunk = 0; chunk < bytes.size(); chunk += 64) {
		std::array<uint32_t, 64> w {};
		for (size_t i = 0; i < 16; i++) {
			const size_t offset = chunk + i * 4;
			w[i] = (static_cast<uint32_t>(bytes[offset]) << 24)
				| (static_cast<uint32_t>(bytes[offset + 1]) << 16)
				| (static_cast<uint32_t>(bytes[offset + 2]) << 8)
				| static_cast<uint32_t>(bytes[offset + 3]);
		}
		for (size_t i = 16; i < 64; i++) {
			const uint32_t s0 = RotateRight(w[i - 15], 7)
				^ RotateRight(w[i - 15], 18) ^ (w[i - 15] >> 3);
			const uint32_t s1 = RotateRight(w[i - 2], 17)
				^ RotateRight(w[i - 2], 19) ^ (w[i - 2] >> 10);
			w[i] = w[i - 16] + s0 + w[i - 7] + s1;
		}

		uint32_t a = h0;
		uint32_t b = h1;
		uint32_t c = h2;
		uint32_t d = h3;
		uint32_t e = h4;
		uint32_t f = h5;
		uint32_t g = h6;
		uint32_t h = h7;
		for (size_t i = 0; i < 64; i++) {
			const uint32_t s1 = RotateRight(e, 6) ^ RotateRight(e, 11)
				^ RotateRight(e, 25);
			const uint32_t ch = (e & f) ^ ((~e) & g);
			const uint32_t temp1 = h + s1 + ch + k[i] + w[i];
			const uint32_t s0 = RotateRight(a, 2) ^ RotateRight(a, 13)
				^ RotateRight(a, 22);
			const uint32_t maj = (a & b) ^ (a & c) ^ (b & c);
			const uint32_t temp2 = s0 + maj;
			h = g;
			g = f;
			f = e;
			e = d + temp1;
			d = c;
			c = b;
			b = a;
			a = temp1 + temp2;
		}
		h0 += a;
		h1 += b;
		h2 += c;
		h3 += d;
		h4 += e;
		h5 += f;
		h6 += g;
		h7 += h;
	}

	std::ostringstream output;
	output << std::hex << std::setfill('0')
		<< std::setw(8) << h0 << std::setw(8) << h1
		<< std::setw(8) << h2 << std::setw(8) << h3
		<< std::setw(8) << h4 << std::setw(8) << h5
		<< std::setw(8) << h6 << std::setw(8) << h7;
	return output.str();
}


bool
ExtractJsonObjectAt(const std::string& json, size_t& pos, std::string& object)
{
	object.clear();
	if (pos >= json.size() || json[pos] != '{')
		return false;

	const size_t start = pos;
	int depth = 0;
	bool inString = false;
	bool escaping = false;
	for (; pos < json.size(); pos++) {
		const char c = json[pos];
		if (inString) {
			if (escaping) {
				escaping = false;
			} else if (c == '\\') {
				escaping = true;
			} else if (c == '"') {
				inString = false;
			}
			continue;
		}
		if (c == '"') {
			inString = true;
			continue;
		}
		if (c == '{') {
			depth++;
			continue;
		}
		if (c == '}') {
			depth--;
			if (depth == 0) {
				pos++;
				object = json.substr(start, pos - start);
				return true;
			}
		}
	}
	return false;
}


bool
ExtractJsonObjectArrayField(const std::string& json, const std::string& key,
	std::vector<std::string>& objects)
{
	objects.clear();
	const std::string marker = "\"" + key + "\"";
	size_t pos = json.find(marker);
	if (pos == std::string::npos)
		return false;
	pos = json.find(':', pos + marker.size());
	if (pos == std::string::npos)
		return false;
	pos++;
	while (pos < json.size()
		&& std::isspace(static_cast<unsigned char>(json[pos]))) {
		pos++;
	}
	if (pos >= json.size() || json[pos] != '[')
		return false;
	pos++;
	while (pos < json.size()) {
		while (pos < json.size()
			&& std::isspace(static_cast<unsigned char>(json[pos]))) {
			pos++;
		}
		if (pos < json.size() && json[pos] == ']')
			return true;
		std::string object;
		if (!ExtractJsonObjectAt(json, pos, object))
			return false;
		objects.push_back(object);
		while (pos < json.size()
			&& std::isspace(static_cast<unsigned char>(json[pos]))) {
			pos++;
		}
		if (pos < json.size() && json[pos] == ',') {
			pos++;
			continue;
		}
		if (pos < json.size() && json[pos] == ']')
			return true;
		return false;
	}
	return false;
}


std::string
JoinedArgv(const std::vector<std::string>& argv)
{
	std::string command;
	for (const std::string& arg : argv) {
		if (!command.empty())
			command += " ";
		command += arg;
	}
	return command;
}


bool
StartsWith(const std::string& value, const std::string& prefix)
{
	return value.size() >= prefix.size()
		&& value.compare(0, prefix.size(), prefix) == 0;
}


std::string
ExecutableName(const std::string& executable)
{
	const size_t slash = executable.find_last_of("/\\");
	if (slash == std::string::npos)
		return executable;
	return executable.substr(slash + 1);
}


bool
IsShellInterpreter(const std::string& executable)
{
	const std::string name = ExecutableName(executable);
	return name == "sh" || name == "bash" || name == "zsh"
		|| name == "ksh";
}


std::string
FenceLanguage(const std::string& fenceInfo)
{
	const std::string trimmed = Trim(fenceInfo);
	const size_t space = trimmed.find_first_of(" \t");
	if (space == std::string::npos)
		return ToLower(trimmed);
	return ToLower(trimmed.substr(0, space));
}


bool
IsShellCommandFence(const std::string& fenceInfo)
{
	const std::string language = FenceLanguage(fenceInfo);
	return language == "sh" || language == "shell" || language == "bash"
		|| language == "zsh" || language == "ksh" || language == "console";
}


std::string
NormalizeShellSuggestionLine(std::string line)
{
	line = Trim(line);
	if (line.rfind("$ ", 0) == 0 || line.rfind("> ", 0) == 0)
		line.erase(0, 2);
	return Trim(line);
}


bool
UsesShellCommandString(const CommandRequest& command)
{
	if (command.argv.size() < 2 || !IsShellInterpreter(command.argv[0]))
		return false;
	return std::find(command.argv.begin() + 1, command.argv.end(), "-c")
		!= command.argv.end();
}


bool
HasShellControlToken(const CommandRequest& command)
{
	for (const std::string& arg : command.argv) {
		if (arg == ";" || arg == "&&" || arg == "||" || arg == "|"
			|| arg == "<" || arg == ">" || arg == ">>" || arg == "2>"
			|| arg == "2>>" || arg == "&") {
			return true;
		}
	}
	return false;
}


bool
IsRmRecursiveForceCommand(const std::vector<std::string>& argv)
{
	if (argv.empty() || ExecutableName(argv[0]) != "rm")
		return false;

	bool recursive = false;
	bool force = false;
	for (size_t i = 1; i < argv.size(); i++) {
		const std::string& arg = argv[i];
		if (arg == "--")
			break;
		if (arg == "--recursive") {
			recursive = true;
			continue;
		}
		if (arg == "--force") {
			force = true;
			continue;
		}
		if (arg.size() > 1 && arg[0] == '-' && arg[1] != '-') {
			for (size_t flag = 1; flag < arg.size(); flag++) {
				if (arg[flag] == 'r' || arg[flag] == 'R')
					recursive = true;
				else if (arg[flag] == 'f')
					force = true;
			}
		}
	}
	return recursive && force;
}


bool
IsPrivilegedOrDestructiveCommand(const std::vector<std::string>& argv)
{
	if (argv.empty())
		return false;

	const std::string name = ExecutableName(argv[0]);
	return name == "sudo" || name == "dd" || StartsWith(name, "mkfs");
}


bool
NeedsShellQuotes(const std::string& value)
{
	if (value.empty())
		return true;
	for (const char c : value) {
		if (std::isspace(static_cast<unsigned char>(c)) || c == '\''
			|| c == '"' || c == '\\' || c == '|' || c == '&' || c == ';'
			|| c == '<' || c == '>' || c == '$' || c == '`') {
			return true;
		}
	}
	return false;
}


std::string
ShellQuote(const std::string& value)
{
	if (!NeedsShellQuotes(value))
		return value;

	std::string quoted("'");
	for (const char c : value) {
		if (c == '\'')
			quoted += "'\\''";
		else
			quoted.push_back(c);
	}
	quoted += "'";
	return quoted;
}


void
ClassifyCommand(CommandRequest& command)
{
	const std::string joined = JoinedArgv(command.argv);
	if (UsesShellCommandString(command)) {
		command.dangerous = true;
		command.runnable = false;
		command.warning = "Command uses a shell interpreter with -c; review and run it manually outside Haikode.";
		return;
	}
	if (HasShellControlToken(command)) {
		command.dangerous = true;
		command.warning = "Command contains shell control-looking tokens; Haikode will run approved commands argv-native without shell interpretation.";
		return;
	}
	if (IsRmRecursiveForceCommand(command.argv)) {
		command.dangerous = true;
		command.warning = "Dangerous command pattern: rm recursive force (rm -rf)";
		return;
	}
	if (IsPrivilegedOrDestructiveCommand(command.argv)) {
		command.dangerous = true;
		command.warning = "Dangerous privileged or destructive command.";
		return;
	}
	if (joined.find("curl") != std::string::npos
		&& joined.find("|") != std::string::npos
		&& joined.find("sh") != std::string::npos) {
		command.dangerous = true;
		command.warning = "Dangerous command pattern: network download pipe to shell";
		return;
	}
	if (joined.find("wget") != std::string::npos
		&& joined.find("|") != std::string::npos
		&& joined.find("sh") != std::string::npos) {
		command.dangerous = true;
		command.warning = "Dangerous command pattern: network download pipe to shell";
		return;
	}
	if (joined.find("chmod -R 777") != std::string::npos) {
		command.dangerous = true;
		command.warning = "Dangerous command pattern: chmod -R 777";
		return;
	}
}


void
AppendShellCommandSuggestions(const std::string& body,
	std::vector<CommandRequest>& commands)
{
	std::istringstream stream(body);
	std::string line;
	while (std::getline(stream, line)) {
		line = NormalizeShellSuggestionLine(line);
		if (line.empty() || line[0] == '#')
			continue;

		CommandRequest command;
		command.summary = "Review shell command suggestion";
		command.argv = {"sh", "-c", line};
		ClassifyCommand(command);
		commands.push_back(command);
	}
}


bool
ParseCommandRequestJson(const std::string& json, CommandRequest& command,
	std::string& error, bool requireSummary)
{
	command = CommandRequest();
	ExtractJsonStringField(json, "summary", command.summary);
	if (requireSummary && Trim(command.summary).empty())
		return false;
	if (!ExtractJsonStringArrayField(json, "argv", command.argv)
		|| command.argv.empty()) {
		error = "Command request argv must be a non-empty JSON string array.";
		return false;
	}
	ClassifyCommand(command);
	return true;
}


void
AppendCommandRequestsFromJsonArray(const std::string& json,
	std::vector<CommandRequest>& commands)
{
	std::vector<std::string> objects;
	if (!ExtractJsonObjectArrayField(json, "commands", objects))
		return;

	for (const std::string& object : objects) {
		CommandRequest command;
		std::string objectError;
		if (ParseCommandRequestJson(object, command, objectError, true))
			commands.push_back(command);
	}
}


void
AppendCommandRequestsFromJson(const std::string& json,
	std::vector<CommandRequest>& commands)
{
	std::vector<CommandRequest> arrayCommands;
	AppendCommandRequestsFromJsonArray(json, arrayCommands);
	if (!arrayCommands.empty()) {
		commands.insert(commands.end(), arrayCommands.begin(),
			arrayCommands.end());
		return;
	}

	CommandRequest command;
	std::string jsonError;
	if (ParseCommandRequestJson(json, command, jsonError, true))
		commands.push_back(command);
}

} // namespace

std::string
SelectContextText(const std::string& selection, const std::string& fullFileText)
{
	if (!selection.empty())
		return selection;
	return fullFileText;
}


std::vector<std::string>
ParseProjectContextPathList(const std::string& text, size_t maxPaths)
{
	std::vector<std::string> paths;
	if (maxPaths == 0)
		return paths;

	size_t start = 0;
	while (start <= text.size()) {
		const size_t end = text.find_first_of(";\n\r,", start);
		const std::string path = Trim(text.substr(start,
			end == std::string::npos ? std::string::npos : end - start));
		if (!path.empty()
			&& std::find(paths.begin(), paths.end(), path) == paths.end()) {
			paths.push_back(path);
			if (paths.size() >= maxPaths)
				break;
		}
		if (end == std::string::npos)
			break;
		start = end + 1;
	}
	return paths;
}


std::string
Sha256HexForText(const std::string& text)
{
	return Sha256Hex(text);
}


bool
ExtractCommandRequests(const std::string& text,
	std::vector<CommandRequest>& commands, std::string& error)
{
	commands.clear();
	error.clear();
	const std::string trimmedText = Trim(text);
	if (trimmedText.size() >= 2 && trimmedText[0] == '{'
		&& trimmedText[trimmedText.size() - 1] == '}') {
		AppendCommandRequestsFromJson(trimmedText, commands);
		return true;
	}

	size_t pos = 0;
	while (true) {
		const size_t fence = text.find("```", pos);
		if (fence == std::string::npos)
			return true;
		const size_t bodyStart = text.find('\n', fence);
		if (bodyStart == std::string::npos) {
			return true;
		}
		const std::string fenceInfo = ToLower(Trim(text.substr(fence + 3,
			bodyStart - fence - 3)));
		const size_t bodyEnd = text.find("```", bodyStart + 1);
		if (bodyEnd == std::string::npos) {
			error = "Command suggestion fence is not closed.";
			return false;
		}

		const std::string body = text.substr(bodyStart + 1,
			bodyEnd - bodyStart - 1);
		if (fenceInfo == "haikode-command") {
			const std::string json = Trim(body);
			std::vector<CommandRequest> arrayCommands;
			AppendCommandRequestsFromJsonArray(json, arrayCommands);
			if (!arrayCommands.empty()) {
				commands.insert(commands.end(), arrayCommands.begin(),
					arrayCommands.end());
				pos = bodyEnd + 3;
				continue;
			}

			CommandRequest command;
			if (!ParseCommandRequestJson(json, command, error, false)) {
				return false;
			}
			commands.push_back(command);
		} else if (FenceLanguage(fenceInfo) == "json") {
			const std::string json = Trim(body);
			AppendCommandRequestsFromJson(json, commands);
		} else if (IsShellCommandFence(fenceInfo)) {
			AppendShellCommandSuggestions(body, commands);
		}
		pos = bodyEnd + 3;
	}
}


std::string
CommandDisplayString(const CommandRequest& command)
{
	std::string display;
	for (const std::string& arg : command.argv) {
		if (!display.empty())
			display += " ";
		display += ShellQuote(arg);
	}
	return display;
}


std::string
CommandPrimaryActionLabel(const CommandRequest& command)
{
	if (!command.runnable)
		return "Acknowledge command";
	if (command.dangerous)
		return "Review command";
	return "Run command";
}


std::string
FormatCommandApprovalPrompt(const CommandRequest& command,
	const std::string& projectRoot)
{
	std::ostringstream prompt;
	prompt << "Run this AI-requested command?\n\n";
	if (!command.summary.empty())
		prompt << command.summary << "\n";
	prompt << CommandDisplayString(command) << "\n\n";
	if (!projectRoot.empty())
		prompt << "Project: " << projectRoot << "\n";
	prompt << "Haikode will execute argv directly without a shell.\n";
	prompt << "No files are changed unless this command changes them.\n";
	if (command.dangerous && !command.warning.empty())
		prompt << "\nWarning: " << command.warning << "\n";
	if (!command.runnable) {
		prompt
			<< "\nHaikode will not run this command.\n"
			<< "Manual shell review is required before running it outside Haikode.\n";
	}
	return prompt.str();
}


std::string
FormatPendingActions(const PendingActionSummary& summary)
{
	std::ostringstream text;
	if (summary.patchFiles.empty() && summary.changedPaths.empty()
		&& summary.commands.empty()) {
		return "No pending AI actions.";
	}

	if (!summary.patchFiles.empty()) {
		size_t additions = 0;
		size_t deletions = 0;
		size_t hunks = 0;
		for (const PendingActionSummary::PatchFile& file : summary.patchFiles) {
			additions += file.additions;
			deletions += file.deletions;
			hunks += file.hunkCount;
		}
		text << "Patch: " << summary.patchFiles.size() << " file(s), "
			<< hunks << " hunk(s), +" << additions << " -" << deletions;
		for (const PendingActionSummary::PatchFile& file : summary.patchFiles) {
			text << "\n  " << file.path << " (+" << file.additions << " -"
				<< file.deletions << ", " << file.hunkCount << " hunk(s)";
			if (file.newFile)
				text << ", new file";
			text << ")";
		}
	} else if (!summary.changedPaths.empty()) {
		text << "Patch: " << summary.changedPaths.size() << " file(s), "
			<< summary.hunkCount << " hunk(s)";
		for (const std::string& path : summary.changedPaths)
			text << "\n  " << path;
	}

	if (!summary.commands.empty()) {
		if (text.tellp() > 0)
			text << "\n";
		text << "Commands: " << summary.commands.size() << " pending";
		size_t blockedCount = 0;
		size_t warningCount = 0;
		for (const CommandRequest& command : summary.commands) {
			if (!command.runnable)
				blockedCount++;
			if (command.dangerous)
				warningCount++;
		}
		if (blockedCount > 0)
			text << ", " << blockedCount << " blocked";
		if (warningCount > 0)
			text << ", " << warningCount << " warning";
		for (const CommandRequest& command : summary.commands) {
			text << "\n  "
				<< (command.summary.empty() ? "Command" : command.summary)
				<< ": " << CommandDisplayString(command);
			if (command.dangerous && !command.warning.empty())
				text << "\n    Warning: " << command.warning;
			if (!command.runnable)
				text << "\n    Not runnable inside Haikode.";
		}
	}

	return text.str();
}


bool
SaveCommandRequests(const std::string& projectRoot,
	const std::vector<CommandRequest>& commands, std::string& savedPath,
	std::string& error)
{
	savedPath.clear();
	error.clear();
	try {
		if (projectRoot.empty()) {
			error = "No active project root.";
			return false;
		}
		if (commands.empty()) {
			error = "No command requests to save.";
			return false;
		}

		const fs::path root = fs::weakly_canonical(projectRoot);
		const fs::path commandsRoot = root / ".haikode" / "commands";
		fs::create_directories(commandsRoot);
		const fs::path commandPath = commandsRoot
			/ ("command-" + Timestamp() + ".json");
		if (!IsInsideDirectory(commandPath, root)) {
			error = "Unsafe command save path.";
			return false;
		}

		std::ofstream file(commandPath, std::ios::binary | std::ios::trunc);
		if (!file) {
			error = "Could not save command requests.";
			return false;
		}
		file << "{\n  \"commands\":[";
		for (size_t i = 0; i < commands.size(); i++) {
			const CommandRequest& command = commands[i];
			if (i > 0)
				file << ",";
			file << "\n    {\"summary\":\""
				<< EscapeJson(RedactSecrets(command.summary))
				<< "\",\"argv\":[";
			for (size_t arg = 0; arg < command.argv.size(); arg++) {
				if (arg > 0)
					file << ",";
				file << "\"" << EscapeJson(RedactSecrets(command.argv[arg]))
					<< "\"";
			}
			file << "],\"dangerous\":"
				<< (command.dangerous ? "true" : "false")
				<< ",\"runnable\":"
				<< (command.runnable ? "true" : "false")
				<< ",\"warning\":\""
				<< EscapeJson(RedactSecrets(command.warning)) << "\"}";
		}
		file << "\n  ]\n}\n";
		savedPath = commandPath.string();
		return true;
	} catch (const std::exception& exception) {
		error = exception.what();
		return false;
	}
}


bool
SaveAiSession(const std::string& projectRoot, const AiSessionRecord& session,
	std::string& savedPath, std::string& error)
{
	savedPath.clear();
	error.clear();
	try {
		if (projectRoot.empty()) {
			error = "No active project root.";
			return false;
		}

		const fs::path root = fs::weakly_canonical(projectRoot);
		const fs::path sessionsRoot = root / ".haikode" / "sessions";
		fs::create_directories(sessionsRoot);
		const fs::path sessionPath = sessionsRoot
			/ ("session-" + Timestamp() + ".json");
		if (!IsInsideDirectory(sessionPath, root)) {
			error = "Unsafe session save path.";
			return false;
		}

		std::ofstream file(sessionPath, std::ios::binary | std::ios::trunc);
		if (!file) {
			error = "Could not save AI session.";
			return false;
		}

		file
			<< "{\n"
			<< "  \"user_prompt\":\"" << EscapeJson(RedactSecrets(session.userPrompt))
			<< "\",\n"
			<< "  \"provider_base_url\":\""
			<< EscapeJson(RedactSecrets(session.providerBaseUrl)) << "\",\n"
			<< "  \"provider_model\":\"" << EscapeJson(RedactSecrets(session.providerModel))
			<< "\",\n"
			<< "  \"auth_mode\":\"" << EscapeJson(RedactSecrets(session.authMode))
			<< "\",\n"
			<< "  \"active_file\":\"" << EscapeJson(RedactSecrets(session.activeFile))
			<< "\",\n"
			<< "  \"response_text\":\"" << EscapeJson(RedactSecrets(session.responseText))
			<< "\",\n"
			<< "  \"pending_actions\":\"" << EscapeJson(RedactSecrets(session.pendingActions))
			<< "\",\n"
			<< "  \"saved_patch_path\":\""
			<< EscapeJson(RedactSecrets(session.savedPatchPath))
			<< "\"\n"
			<< "}\n";
		savedPath = sessionPath.string();
		return true;
	} catch (const std::exception& exception) {
		error = exception.what();
		return false;
	}
}


std::vector<ProjectRecordEntry>
ListProjectRecords(const std::string& projectRoot, size_t maxRecords)
{
	std::vector<ProjectRecordEntry> records;
	if (projectRoot.empty() || maxRecords == 0)
		return records;

	try {
		const fs::path root = fs::weakly_canonical(projectRoot);
		const fs::path haikodeRoot = root / ".haikode";
		const std::vector<std::string> directories = {
			"sessions", "logs", "patches", "commands"
		};

		struct RecordWithTime {
			ProjectRecordEntry record;
			fs::file_time_type modified;
		};
		std::vector<RecordWithTime> withTimes;
		for (const std::string& directory : directories) {
			const fs::path recordRoot = haikodeRoot / directory;
			std::error_code errorCode;
			if (!fs::is_directory(recordRoot, errorCode))
				continue;
			for (const fs::directory_entry& entry
					: fs::directory_iterator(recordRoot, errorCode)) {
				if (errorCode)
					break;
				if (!entry.is_regular_file(errorCode))
					continue;

				const fs::path relative = fs::relative(entry.path(), root,
					errorCode);
				if (errorCode || relative.empty())
					continue;
				ProjectRecordEntry record;
				record.type = RecordTypeForDirectory(directory);
				record.path = relative.string();
				record.sizeBytes = static_cast<size_t>(entry.file_size(
					errorCode));
				withTimes.push_back({
					record,
					entry.last_write_time(errorCode)
				});
			}
		}

		std::sort(withTimes.begin(), withTimes.end(),
			[](const RecordWithTime& a, const RecordWithTime& b) {
				return a.modified > b.modified;
			});
		for (const RecordWithTime& record : withTimes) {
			if (records.size() >= maxRecords)
				break;
			records.push_back(record.record);
		}
	} catch (...) {
	}
	return records;
}


bool
ReadProjectRecord(const std::string& projectRoot, const std::string& recordPath,
	size_t maxBytes, std::string& text, std::string& error)
{
	text.clear();
	error.clear();
	try {
		if (projectRoot.empty()) {
			error = "No active project root.";
			return false;
		}
		if (recordPath.empty()) {
			error = "No project record path selected.";
			return false;
		}

		const fs::path root = fs::weakly_canonical(projectRoot);
		fs::path relative(recordPath);
		if (relative.is_absolute()) {
			error = "Project record path must be relative.";
			return false;
		}
		for (const fs::path& part : relative) {
			if (part == "..") {
				error = "Unsafe project record path.";
				return false;
			}
		}

		const fs::path target = fs::weakly_canonical(root / relative);
		if (!IsInsideDirectory(target, root)) {
			error = "Project record path escapes the project.";
			return false;
		}
		const fs::path expectedRoot = root / ".haikode";
		if (!IsInsideDirectory(target, expectedRoot)) {
			error = "Project record must be under .haikode.";
			return false;
		}
		if (!fs::is_regular_file(target)) {
			error = "Project record is not a file.";
			return false;
		}

		std::ifstream file(target, std::ios::binary);
		if (!file) {
			error = "Could not open project record.";
			return false;
		}
		std::string contents((std::istreambuf_iterator<char>(file)),
			std::istreambuf_iterator<char>());
		if (contents.find('\0') != std::string::npos) {
			error = "Project record is binary.";
			return false;
		}
		if (maxBytes > 0 && contents.size() > maxBytes) {
			contents.resize(maxBytes);
			contents += "\n\n[Haikode truncated this project record preview.]\n";
		}
		text = contents;
		return true;
	} catch (const std::exception& exception) {
		error = exception.what();
		return false;
	}
}


std::vector<ProjectFileSummary>
BuildProjectMap(const std::string& projectRoot, size_t maxFiles,
	size_t* candidateCount)
{
	std::vector<ProjectFileSummary> files;
	if (candidateCount != nullptr)
		*candidateCount = 0;
	if (projectRoot.empty() || maxFiles == 0)
		return files;

	try {
		const fs::path root = fs::weakly_canonical(projectRoot);
		if (!fs::is_directory(root))
			return files;

		std::vector<fs::path> paths;
		std::error_code error;
		fs::recursive_directory_iterator it(root,
			fs::directory_options::skip_permission_denied, error);
		const fs::recursive_directory_iterator end;
		while (it != end) {
			error.clear();
			const fs::path relative = fs::relative(it->path(), root, error);
			if (error) {
				it.increment(error);
				continue;
			}
			if (ShouldSkipRelativePath(relative)) {
				error.clear();
				if (it->is_directory(error))
					it.disable_recursion_pending();
				it.increment(error);
				continue;
			}
			error.clear();
			const bool isRegular = it->is_regular_file(error);
			if (error || !isRegular) {
				it.increment(error);
				continue;
			}
			paths.push_back(relative);
			it.increment(error);
		}
		std::sort(paths.begin(), paths.end());

		for (const fs::path& relative : paths) {
			const fs::path absolute = root / relative;
			std::string text;
			if (!ReadSmallTextFile(absolute, text))
				continue;
			if (candidateCount != nullptr)
				(*candidateCount)++;
			if (files.size() >= maxFiles)
				continue;

			ProjectFileSummary summary;
			summary.path = relative.generic_string();
			summary.language = LanguageForPath(relative);
			summary.role = RoleForPath(relative);
			summary.risk = RiskForPath(relative);
			summary.hasTodo = ContainsTodoMarker(text);
			std::ostringstream details;
			details << LineCount(text) << " line(s)";
			if (summary.hasTodo)
				details << ", TODO marker";
			summary.summary = details.str();
			files.push_back(summary);
		}
	} catch (const std::exception&) {
		files.clear();
	}
	return files;
}


bool
RefreshProjectFileIndex(const std::string& projectRoot, size_t maxFiles,
	ProjectFileIndex& index, std::string& error)
{
	index = ProjectFileIndex();
	error.clear();

	size_t candidateCount = 0;
	index.files = BuildProjectMap(projectRoot, maxFiles, &candidateCount);
	index.candidateCount = candidateCount;

	if (projectRoot.empty()) {
		error = "No active project root.";
		return false;
	}
	if (index.files.empty()) {
		error = "No text/source files found in active project.";
		return false;
	}
	if (!SaveProjectMemory(projectRoot, index.files, index.candidateCount,
			index.memoryPath, error)) {
		return false;
	}
	return true;
}


std::string
FormatProjectFileIndexSummary(const ProjectFileIndex& index,
	size_t maxVisibleFiles)
{
	std::ostringstream summary;
	if (index.files.empty()) {
		summary << "Haikode cannot see any text/source files in this project "
			"yet.";
		return summary.str();
	}

	summary << "Haikode can see " << index.files.size();
	if (index.candidateCount > index.files.size())
		summary << "/" << index.candidateCount;
	summary << " text/source file(s).";

	const size_t visibleCount = std::min(maxVisibleFiles, index.files.size());
	for (size_t i = 0; i < visibleCount; i++) {
		const ProjectFileSummary& file = index.files[i];
		summary << "\n- " << file.path << " [" << file.language << ", "
			<< file.role << ", " << file.risk << "]";
		if (file.hasTodo)
			summary << " TODO";
		if (!file.summary.empty())
			summary << " - " << file.summary;
	}

	if (visibleCount < index.files.size()) {
		summary << "\n... " << (index.files.size() - visibleCount)
			<< " more file(s). Use Project files to browse the full "
			"Haikode index.";
	}

	return summary.str();
}


bool
SaveProjectMemory(const std::string& projectRoot,
	const std::vector<ProjectFileSummary>& files, size_t candidateCount,
	std::string& savedPath, std::string& error)
{
	savedPath.clear();
	error.clear();
	try {
		if (projectRoot.empty()) {
			error = "No active project root.";
			return false;
		}

		const fs::path root = fs::weakly_canonical(projectRoot);
		if (!fs::is_directory(root)) {
			error = "Active project root is not a directory.";
			return false;
		}

		const fs::path haikodeRoot = root / ".haikode";
		const std::vector<std::string> directories = {
			"sessions", "notes", "patches", "logs", "backups", "commands"
		};
		fs::create_directories(haikodeRoot);
		for (const std::string& directory : directories) {
			const fs::path directoryPath = haikodeRoot / directory;
			if (!IsInsideDirectory(directoryPath, root)) {
				error = "Unsafe project memory directory.";
				return false;
			}
			fs::create_directories(directoryPath);
		}

		const fs::path memoryPath = haikodeRoot / "project.json";
		if (!IsInsideDirectory(memoryPath, root)) {
			error = "Unsafe project memory path.";
			return false;
		}

		std::ofstream file(memoryPath, std::ios::binary | std::ios::trunc);
		if (!file) {
			error = "Could not save project memory.";
			return false;
		}

		ProjectMemory commandHints;
		InferMakefileCommands(root, commandHints);

		const std::string now = Timestamp();
		file
			<< "{\n"
			<< "  \"schema\":\"haikode.project.v1\",\n"
			<< "  \"name\":\"" << EscapeJson(root.filename().string())
			<< "\",\n"
			<< "  \"root\":\"" << EscapeJson(root.string()) << "\",\n"
			<< "  \"updated\":\"" << now << "\",\n"
			<< "  \"default_build_command\":\""
			<< EscapeJson(commandHints.defaultBuildCommand) << "\",\n"
			<< "  \"default_test_command\":\""
			<< EscapeJson(commandHints.defaultTestCommand) << "\",\n"
			<< "  \"candidate_count\":" << candidateCount << ",\n"
			<< "  \"files\":[";
		for (size_t i = 0; i < files.size(); i++) {
			const ProjectFileSummary& summary = files[i];
			if (i > 0)
				file << ",";
			file
				<< "\n    {\"path\":\"" << EscapeJson(summary.path)
				<< "\",\"language\":\"" << EscapeJson(summary.language)
				<< "\",\"role\":\"" << EscapeJson(summary.role)
				<< "\",\"risk\":\"" << EscapeJson(summary.risk)
				<< "\",\"todo\":" << (summary.hasTodo ? "true" : "false")
				<< ",\"summary\":\"" << EscapeJson(summary.summary)
				<< "\"}";
		}
		file << "\n  ]\n}\n";
		savedPath = memoryPath.string();
		return true;
	} catch (const std::exception& exception) {
		error = exception.what();
		return false;
	}
}


bool
LoadProjectMemory(const std::string& projectRoot, size_t maxFiles,
	ProjectMemory& memory, std::string& error)
{
	memory = ProjectMemory();
	error.clear();
	try {
		if (projectRoot.empty()) {
			error = "No active project root.";
			return false;
		}
		if (maxFiles == 0) {
			error = "Project memory file limit is zero.";
			return false;
		}

		const fs::path root = fs::weakly_canonical(projectRoot);
		if (!fs::is_directory(root)) {
			error = "Active project root is not a directory.";
			return false;
		}

		const fs::path memoryPath = root / ".haikode" / "project.json";
		if (!IsInsideDirectory(memoryPath, root)) {
			error = "Unsafe project memory path.";
			return false;
		}
		std::ifstream file(memoryPath, std::ios::binary);
		if (!file) {
			error = "Project memory has not been created yet.";
			return false;
		}
		std::string json((std::istreambuf_iterator<char>(file)),
			std::istreambuf_iterator<char>());
		if (json.find('\0') != std::string::npos) {
			error = "Project memory is not text JSON.";
			return false;
		}

		ExtractJsonStringField(json, "default_build_command",
			memory.defaultBuildCommand);
		ExtractJsonStringField(json, "default_test_command",
			memory.defaultTestCommand);
		ExtractJsonSizeField(json, "candidate_count", memory.candidateCount);
		std::vector<std::string> objects;
		if (!ExtractJsonObjectArrayField(json, "files", objects)) {
			error = "Project memory has no readable files list.";
			return false;
		}

		for (const std::string& object : objects) {
			ProjectFileSummary summary;
			if (!ExtractJsonStringField(object, "path", summary.path))
				continue;
			fs::path relative(summary.path);
			if (!IsSafeRelativePath(relative) || ShouldSkipRelativePath(relative))
				continue;
			ExtractJsonStringField(object, "language", summary.language);
			ExtractJsonStringField(object, "role", summary.role);
			ExtractJsonStringField(object, "risk", summary.risk);
			ExtractJsonStringField(object, "summary", summary.summary);
			ExtractJsonBoolField(object, "todo", summary.hasTodo);
			if (memory.files.size() < maxFiles)
				memory.files.push_back(summary);
		}
		if (memory.candidateCount == 0)
			memory.candidateCount = objects.size();
		return true;
	} catch (const std::exception& exception) {
		error = exception.what();
		return false;
	}
}


bool
LoadProjectMemory(const std::string& projectRoot, size_t maxFiles,
	std::vector<ProjectFileSummary>& files, size_t& candidateCount,
	std::string& error)
{
	ProjectMemory memory;
	if (!LoadProjectMemory(projectRoot, maxFiles, memory, error))
		return false;
	files = memory.files;
	candidateCount = memory.candidateCount;
	return true;
}


bool
InferProjectCommands(const std::string& projectRoot, ProjectMemory& memory,
	std::string& error)
{
	error.clear();
	try {
		if (projectRoot.empty()) {
			error = "No active project root.";
			return false;
		}

		const fs::path root = fs::weakly_canonical(projectRoot);
		if (!fs::is_directory(root)) {
			error = "Active project root is not a directory.";
			return false;
		}

		if (InferMakefileCommands(root, memory))
			return true;

		error = "No known build command was inferred.";
		return false;
	} catch (const std::exception& exception) {
		error = exception.what();
		return false;
	}
}


bool
LoadProjectContextFile(const std::string& projectRoot,
	const std::string& relativePath, size_t maxBytes, ContextFile& file,
	std::string& error)
{
	file = ContextFile();
	error.clear();
	if (projectRoot.empty()) {
		error = "No active project root.";
		return false;
	}
	if (relativePath.empty()) {
		error = "No project file path selected.";
		return false;
	}
	if (maxBytes == 0) {
		error = "Context file byte limit is zero.";
		return false;
	}

	try {
		const fs::path relative(relativePath);
		if (!IsSafeRelativePath(relative)) {
			error = "Unsafe project file path: " + relativePath;
			return false;
		}
		if (ShouldSkipRelativePath(relative)) {
			error = "Selected project file is ignored by Haikode: "
				+ relativePath;
			return false;
		}

		std::error_code fsError;
		const fs::path root = fs::weakly_canonical(projectRoot, fsError);
		if (fsError || !fs::is_directory(root, fsError)) {
			error = "Active project root is not a directory.";
			return false;
		}

		fsError.clear();
		const fs::path target = fs::weakly_canonical(root / relative, fsError);
		if (fsError) {
			error = "Could not resolve selected project file: " + relativePath;
			return false;
		}
		if (!IsInsideDirectory(target, root)) {
			error = "Unsafe project file path: " + relativePath;
			return false;
		}
		if (!fs::is_regular_file(target, fsError)) {
			error = "Selected project path is not a regular file: "
				+ relativePath;
			return false;
		}

		fsError.clear();
		const uintmax_t size = fs::file_size(target, fsError);
		if (fsError) {
			error = "Could not inspect selected project file: " + relativePath;
			return false;
		}

		const size_t bytesToRead = static_cast<size_t>(
			std::min<uintmax_t>(size, maxBytes));
		std::ifstream stream(target, std::ios::binary);
		if (!stream) {
			error = "Could not open selected project file: " + relativePath;
			return false;
		}

		std::string text(bytesToRead, '\0');
		if (bytesToRead > 0)
			stream.read(text.data(), static_cast<std::streamsize>(bytesToRead));
		text.resize(static_cast<size_t>(stream.gcount()));
		if (text.find('\0') != std::string::npos) {
			error = "Refusing to load binary project file: " + relativePath;
			return false;
		}

		file.path = relative.generic_string();
		file.text = text;
		file.truncated = size > maxBytes;
		if (!file.truncated)
			file.sha256 = Sha256HexForText(text);
		return true;
	} catch (const std::exception& exception) {
		error = exception.what();
		return false;
	}
}


bool
NormalizeProjectContextPath(const std::string& projectRoot,
	const std::string& path, std::string& normalizedPath, std::string& error)
{
	normalizedPath.clear();
	error.clear();
	if (path.empty()) {
		error = "Context file path is empty.";
		return false;
	}

	try {
		const fs::path input(path);
		if (!input.is_absolute()) {
			if (!IsSafeRelativePath(input)) {
				error = "Unsafe context file path: " + path;
				return false;
			}
			if (ShouldSkipRelativePath(input)) {
				error = "Context file path is ignored by Haikode: " + path;
				return false;
			}
			normalizedPath = input.generic_string();
			return true;
		}

		if (projectRoot.empty()) {
			error = "Cannot make absolute context file path project-relative without an active project.";
			return false;
		}

		std::error_code fsError;
		const fs::path root = fs::weakly_canonical(projectRoot, fsError);
		if (fsError || !fs::is_directory(root, fsError)) {
			error = "Active project root is not a directory.";
			return false;
		}

		fsError.clear();
		const fs::path target = fs::weakly_canonical(input, fsError);
		if (fsError) {
			error = "Could not resolve context file path: " + path;
			return false;
		}
		if (!IsInsideDirectory(target, root)) {
			error = "Context file is outside the active project: " + path;
			return false;
		}

		fsError.clear();
		const fs::path relative = fs::relative(target, root, fsError);
		if (fsError || !IsSafeRelativePath(relative)) {
			error = "Could not make context file path project-relative: " + path;
			return false;
		}
		if (ShouldSkipRelativePath(relative)) {
			error = "Context file path is ignored by Haikode: "
				+ relative.generic_string();
			return false;
		}
		normalizedPath = relative.generic_string();
		return true;
	} catch (const std::exception& exception) {
		error = exception.what();
		return false;
	}
}


std::string
PromptBuilder::ModeInstruction(PromptMode mode) const
{
	switch (mode) {
		case PromptMode::Ask:
			return "Answer the user's question about this Haiku project. If the user asks for follow-up build/test work, return explicit haikode-command blocks instead of saying you ran anything.";
		case PromptMode::ExplainSelection:
			return "Explain the selected code or active file and mention Haiku API details when relevant.";
		case PromptMode::SummarizeProject:
			return "Summarize this Haiku project using the project map. Identify the main components, likely build/test entry points, TODO hotspots, and high-risk files.";
		case PromptMode::ProposePatch:
			return "Propose a small unified diff only. Do not return haikode-edit blocks. Do not include command requests unless the user explicitly asks for a follow-up command.";
		case PromptMode::ReviewDiff:
			return "Review the diff for correctness, safety, and Haiku-native style.";
	}
	return "Answer the user's question about this Haiku project.";
}


PromptBuildResult
PromptBuilder::Build(const VibeCodingRequest& request, size_t maxBytesPerFile,
	size_t maxFiles, size_t maxProjectFiles) const
{
	PromptBuildResult result;
	result.warnings = request.contextWarnings;
	std::ostringstream prompt;
	const bool allowSingleFileEditBlock
		= request.mode != PromptMode::ProposePatch;

	prompt
		<< "You are Haikode, a native Haiku OS AI coding assistant built as a "
		<< "Genio IDE fork.\n"
		<< "Prefer BeAPI and native C++ patterns. Do not suggest Electron or "
		<< "webview rewrites.\n"
		<< "Never assume permission to run commands or write files. File edits "
		<< "must be proposed as unified diffs for explicit review. If a command "
		<< "would help, propose it in a fenced haikode-command JSON block with "
		<< "summary and argv fields; Haikode will not run it automatically.\n\n"
		<< "Response contract:\n"
		<< "- For explanations, answer normally and cite relevant file paths.\n"
		<< "- For edits, output one unified diff using project-relative paths. "
		<< "Do not describe edits that are not present in the diff.\n"
		<< "- Patch paths must not be absolute, must not contain .., and must "
		<< "not modify .git, .hg, .svn, .haikode, .genio, Haikode.settings, "
		<< "or Genio.settings.\n"
		<< "- Return at most one unified diff per response. Keep patches small "
		<< "and focused on files shown in the project map or file context.\n";
	if (allowSingleFileEditBlock) {
		prompt
			<< "- For one complete file with sha256=..., you may instead output "
			<< "a fenced haikode-edit JSON block with path, original_sha256, "
			<< "and replacement fields; Haikode will verify the hash before "
			<< "showing a reviewable patch.\n"
			<< "```haikode-edit\n"
			<< "{\"path\":\"src/main.cpp\",\"original_sha256\":\"...\","
			<< "\"replacement\":\"full new file text\"}\n"
			<< "```\n";
	} else {
		prompt
			<< "- Do not return haikode-edit blocks in patch proposal mode; "
			<< "Haikode's primary patch workflow expects a unified diff.\n";
	}
	prompt
		<< "- For commands, output fenced JSON exactly like:\n"
		<< "```haikode-command\n"
		<< "{\"summary\":\"Run tests\",\"argv\":[\"make\",\"test\"]}\n"
		<< "```\n"
		<< "- Command argv must be a JSON string array. Do not use shell "
		<< "strings, pipes, redirection, or sh -c.\n"
		<< "- Never claim that a command was run or a file was changed; Haikode "
		<< "will ask the user to approve each action.\n\n"
		<< "Project root: " << request.projectRoot << "\n"
		<< "Mode: " << ModeInstruction(request.mode) << "\n\n"
		<< "User request:\n" << request.userPrompt << "\n\n";

	const size_t projectFileCount = std::min(maxProjectFiles,
		request.projectFiles.size());
	if (request.projectFiles.size() > maxProjectFiles) {
		result.warnings.push_back("Some project-map entries were omitted from AI context.");
	}
	if (request.projectMapCandidateCount > request.projectFiles.size()) {
		std::ostringstream warning;
		warning << "project-map context included " << request.projectFiles.size()
			<< " of " << request.projectMapCandidateCount << " discovered file(s).";
		result.warnings.push_back(warning.str());
	}
	if (projectFileCount > 0) {
		prompt << "Project map:\n";
		for (size_t i = 0; i < projectFileCount; ++i) {
			const ProjectFileSummary& file = request.projectFiles[i];
			prompt << "- " << file.path;
			if (!file.language.empty())
				prompt << " [" << file.language << "]";
			if (!file.role.empty())
				prompt << " role=" << file.role;
			if (!file.risk.empty())
				prompt << " risk=" << file.risk;
			if (file.hasTodo)
				prompt << " todo=true";
			if (!file.summary.empty())
				prompt << " -- " << file.summary;
			prompt << "\n";
		}
		prompt << "\n";
	}

	if (!request.defaultBuildCommand.empty()
		|| !request.defaultTestCommand.empty()) {
		prompt << "Project commands:\n";
		if (!request.defaultBuildCommand.empty())
			prompt << "- Build: " << request.defaultBuildCommand << "\n";
		if (!request.defaultTestCommand.empty())
			prompt << "- Test: " << request.defaultTestCommand << "\n";
		prompt << "\n";
	}

	if (!request.pendingDiff.empty()) {
		if (!request.pendingDiffPath.empty())
			prompt << "Selected patch file: " << request.pendingDiffPath << "\n";
		prompt
			<< "Pending unified diff:\n"
			<< "```diff\n"
			<< request.pendingDiff
			<< "\n```\n\n";
	}

	const size_t fileCount = std::min(maxFiles, request.files.size());
	if (request.files.size() > maxFiles) {
		result.warnings.push_back("Some files were omitted from AI context.");
	}

	for (size_t i = 0; i < fileCount; ++i) {
		const ContextFile& file = request.files[i];
		std::string text = file.text;
		bool contentTruncated = false;
		if (text.size() > maxBytesPerFile) {
			text.resize(maxBytesPerFile);
			contentTruncated = true;
			result.warnings.push_back(file.path + " was truncated for AI context.");
		} else if (file.truncated) {
			contentTruncated = true;
			result.warnings.push_back(file.path + " was truncated for AI context.");
		}

		prompt << "File: " << file.path;
		if (!contentTruncated && !file.sha256.empty())
			prompt << " sha256=" << file.sha256;
		prompt
			<< "\n"
			<< "```text\n"
			<< text
			<< "\n```\n\n";
	}

	result.prompt = prompt.str();
	return result;
}

} // namespace Haikode::AI
