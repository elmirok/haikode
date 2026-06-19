/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "UnifiedDiff.h"

#include <algorithm>
#include <array>
#include <atomic>
#include <cctype>
#include <chrono>
#include <cstdint>
#include <cstdlib>
#include <cstring>
#include <exception>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iterator>
#include <sstream>

namespace Haikode::AI {

namespace fs = std::filesystem;

namespace {

std::vector<std::string>
SplitLines(const std::string& text)
{
	std::vector<std::string> lines;
	std::string line;
	std::istringstream in(text);
	while (std::getline(in, line)) {
		if (!line.empty() && line[line.size() - 1] == '\r')
			line.resize(line.size() - 1);
		lines.push_back(line);
	}
	return lines;
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

	std::ostringstream out;
	out << std::hex << std::setfill('0');
	for (uint32_t value : {h0, h1, h2, h3, h4, h5, h6, h7})
		out << std::setw(8) << value;
	return out.str();
}


std::string
TrimPatchPath(std::string path)
{
	const size_t tab = path.find('\t');
	if (tab != std::string::npos)
		path.resize(tab);
	const size_t space = path.find(' ');
	if (space != std::string::npos)
		path.resize(space);
	if (path.rfind("a/", 0) == 0 || path.rfind("b/", 0) == 0)
		path.erase(0, 2);
	return path;
}

bool
IsSafeRelativePath(const std::string& path)
{
	if (path.empty() || path[0] == '/' || path == "/dev/null")
		return false;

	fs::path fsPath(path);
	for (const fs::path& part : fsPath) {
		if (part == "..")
			return false;
	}
	return true;
}

bool
IsSensitivePatchPath(const std::string& path)
{
	fs::path fsPath(path);
	for (const fs::path& part : fsPath) {
		const std::string name = part.string();
		if (name == ".git" || name == ".hg" || name == ".svn"
			|| name == ".haikode") {
			return true;
		}
	}

	const std::string filename = fsPath.filename().string();
	return filename == ".genio" || filename == "Haikode.settings"
		|| filename == "Genio.settings";
}


std::string
Lowercase(std::string value)
{
	std::transform(value.begin(), value.end(), value.begin(),
		[](unsigned char c) { return static_cast<char>(std::tolower(c)); });
	return value;
}


bool
IsIgnoredGeneratedPatchPath(const std::string& path)
{
	fs::path fsPath(path);
	for (const fs::path& part : fsPath) {
		const std::string name = Lowercase(part.string());
		if (name == "build" || name == "dist" || name == "out"
			|| name == "target" || name == "node_modules"
			|| name == "vendor" || name == ".cache") {
			return true;
		}
	}

	const std::string filename = Lowercase(fsPath.filename().string());
	return filename == ".ds_store" || filename.ends_with(".o")
		|| filename.ends_with(".a") || filename.ends_with(".so")
		|| filename.ends_with(".hpkg") || filename.ends_with(".png")
		|| filename.ends_with(".jpg") || filename.ends_with(".jpeg")
		|| filename.ends_with(".gif") || filename.ends_with(".ico")
		|| filename.ends_with(".zip") || filename.ends_with(".tar")
		|| filename.ends_with(".gz");
}


bool
ValidatePatchPath(const std::string& path, bool allowDevNull, std::string& error)
{
	if (allowDevNull && path == "/dev/null")
		return true;

	if (!IsSafeRelativePath(path)) {
		error = "Unsafe patch path: " + path;
		return false;
	}
	if (IsSensitivePatchPath(path)) {
		error = "Refusing to patch sensitive project metadata: " + path;
		return false;
	}
	if (IsIgnoredGeneratedPatchPath(path)) {
		error = "Refusing to patch ignored/generated path: " + path;
		return false;
	}
	return true;
}

bool
RejectSymlinkPathComponents(const fs::path& root, const std::string& path,
	std::string& error)
{
	fs::path probe = root;
	for (const fs::path& part : fs::path(path)) {
		probe /= part;
		std::error_code statusError;
		const fs::file_status status = fs::symlink_status(probe, statusError);
		if (statusError && status.type() != fs::file_type::not_found) {
			error = "Could not inspect patch path: " + path;
			return false;
		}
		if (!statusError && fs::is_symlink(status)) {
			error = "Refusing to patch symbolic link path: " + path;
			return false;
		}
	}
	return true;
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
ReadTextFile(const fs::path& path, std::vector<std::string>& lines,
	std::string& error)
{
	std::ifstream file(path, std::ios::binary);
	if (!file) {
		error = "Could not open file to patch.";
		return false;
	}
	std::string text((std::istreambuf_iterator<char>(file)),
		std::istreambuf_iterator<char>());
	if (text.find('\0') != std::string::npos) {
		error = "Refusing to patch binary file.";
		return false;
	}
	lines = SplitLines(text);
	return true;
}


bool
ReadTextFileContents(const fs::path& path, std::string& text,
	std::string& error)
{
	std::ifstream file(path, std::ios::binary);
	if (!file) {
		error = "Could not open file for edit proposal.";
		return false;
	}
	text.assign(std::istreambuf_iterator<char>(file),
		std::istreambuf_iterator<char>());
	if (text.find('\0') != std::string::npos) {
		error = "Refusing to edit binary file.";
		return false;
	}
	return true;
}


bool
WriteTextFile(const fs::path& path, const std::vector<std::string>& lines,
	std::string& error)
{
	std::ofstream file(path, std::ios::binary | std::ios::trunc);
	if (!file) {
		error = "Could not write patched file.";
		return false;
	}
	for (const std::string& line : lines)
		file << line << "\n";
	return true;
}

std::string
Timestamp()
{
	static std::atomic<unsigned long long> sSequence {0};
	const auto now = std::chrono::system_clock::now().time_since_epoch();
	return std::to_string(std::chrono::duration_cast<std::chrono::nanoseconds>(
		now).count()) + "-" + std::to_string(++sSequence);
}

std::string
PatchTimestamp()
{
	return Timestamp();
}

bool
ApplyHunks(const std::vector<std::string>& original,
	const std::vector<PatchHunk>& hunks, std::vector<std::string>& patched,
	std::string& error)
{
	patched.clear();
	size_t cursor = 0;
	for (const PatchHunk& hunk : hunks) {
		const size_t target = hunk.oldStart <= 0
			? 0 : static_cast<size_t>(hunk.oldStart - 1);
		if (target < cursor || target > original.size()) {
			error = "Patch hunk points outside the current file.";
			return false;
		}

		while (cursor < target)
			patched.push_back(original[cursor++]);

		for (const PatchHunkLine& line : hunk.lines) {
			if (line.kind == ' ' || line.kind == '-') {
				if (cursor >= original.size() || original[cursor] != line.text) {
					error = "Patch context does not match the current file.";
					return false;
				}
				if (line.kind == ' ')
					patched.push_back(original[cursor]);
				++cursor;
			} else if (line.kind == '+') {
				patched.push_back(line.text);
			}
		}
	}

	while (cursor < original.size())
		patched.push_back(original[cursor++]);
	return true;
}

bool
BuildNewFileContents(const std::vector<PatchHunk>& hunks,
	std::vector<std::string>& contents, std::string& error)
{
	contents.clear();
	for (const PatchHunk& hunk : hunks) {
		for (const PatchHunkLine& line : hunk.lines) {
			if (line.kind == '-') {
				error = "New-file patch cannot delete existing lines.";
				return false;
			}
			if (line.kind == '+' || line.kind == ' ')
				contents.push_back(line.text);
		}
	}
	return true;
}


bool
BuildHunkBlocks(const PatchHunk& hunk, std::vector<std::string>& oldBlock,
	std::vector<std::string>& newBlock, std::string& error)
{
	oldBlock.clear();
	newBlock.clear();
	for (const PatchHunkLine& line : hunk.lines) {
		if (line.kind == ' ' || line.kind == '-') {
			oldBlock.push_back(line.text);
		}
		if (line.kind == ' ' || line.kind == '+') {
			newBlock.push_back(line.text);
		} else if (line.kind != '-') {
			error = "Unsupported patch line in hunk.";
			return false;
		}
	}
	if (oldBlock.empty() && newBlock.empty()) {
		error = "Patch hunk is empty.";
		return false;
	}
	return true;
}


bool
BlockMatchesAt(const std::vector<std::string>& lines,
	const std::vector<std::string>& block, size_t offset)
{
	if (offset + block.size() > lines.size())
		return false;
	for (size_t index = 0; index < block.size(); index++) {
		if (lines[offset + index] != block[index])
			return false;
	}
	return true;
}


bool
FindBlock(const std::vector<std::string>& lines,
	const std::vector<std::string>& block, size_t preferredOffset,
	size_t& offset)
{
	if (BlockMatchesAt(lines, block, preferredOffset)) {
		offset = preferredOffset;
		return true;
	}

	for (size_t index = 0; index <= lines.size(); index++) {
		if (index == preferredOffset)
			continue;
		if (BlockMatchesAt(lines, block, index)) {
			offset = index;
			return true;
		}
	}
	return false;
}


bool
ApplySingleHunkToLines(const std::vector<std::string>& original,
	const PatchHunk& hunk, std::vector<std::string>& patched,
	std::string& error)
{
	std::vector<std::string> oldBlock;
	std::vector<std::string> newBlock;
	if (!BuildHunkBlocks(hunk, oldBlock, newBlock, error))
		return false;

	const size_t preferred = hunk.oldStart <= 0
		? 0 : static_cast<size_t>(hunk.oldStart - 1);
	size_t offset = 0;
	if (!FindBlock(original, oldBlock, preferred, offset)) {
		error = "Patch hunk context does not match the current file.";
		return false;
	}

	patched.clear();
	patched.insert(patched.end(), original.begin(), original.begin() + offset);
	patched.insert(patched.end(), newBlock.begin(), newBlock.end());
	patched.insert(patched.end(), original.begin() + offset + oldBlock.size(),
		original.end());
	return true;
}


PatchFileStats
StatsForFile(const PatchFile& file)
{
	PatchFileStats fileStats;
	fileStats.path = file.newPath;
	fileStats.hunkCount = file.hunks.size();
	fileStats.newFile = file.oldPath == "/dev/null";
	for (const PatchHunk& hunk : file.hunks) {
		for (const PatchHunkLine& line : hunk.lines) {
			if (line.kind == '+')
				fileStats.additions++;
			else if (line.kind == '-')
				fileStats.deletions++;
		}
	}
	return fileStats;
}


std::string
FileSummaryText(const PatchFileStats& fileStats)
{
	std::ostringstream summary;
	summary << fileStats.path << " (+" << fileStats.additions
		<< " -" << fileStats.deletions << ", " << fileStats.hunkCount
		<< " hunk(s)";
	if (fileStats.newFile)
		summary << ", new file";
	summary << ")";
	return summary.str();
}


std::string
HunkHeaderText(const PatchHunk& hunk)
{
	std::ostringstream header;
	header << "@@ -" << hunk.oldStart << "," << hunk.oldCount << " +"
		<< hunk.newStart << "," << hunk.newCount << " @@";
	return header.str();
}


std::string
TrimTrailingWhitespace(std::string value)
{
	while (!value.empty()) {
		const char c = value[value.size() - 1];
		if (c != '\n' && c != '\r' && c != '\t' && c != ' ')
			break;
		value.pop_back();
	}
	return value;
}


std::string
TrimWhitespace(std::string value)
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
				case 'r':
					value.push_back('\r');
					break;
				case 't':
					value.push_back('\t');
					break;
				default:
					value.push_back(escaped);
					break;
			}
		} else
			value.push_back(c);
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


struct EditProposalJson {
	std::string path;
	std::string originalSha256;
	std::string replacement;
};


bool
ExtractEditProposalFields(const std::string& json, EditProposalJson& proposal)
{
	proposal = EditProposalJson();
	return ExtractJsonStringField(json, "path", proposal.path)
		&& ExtractJsonStringField(json, "original_sha256",
			proposal.originalSha256)
		&& ExtractJsonStringField(json, "replacement",
			proposal.replacement);
}


bool
ExtractEditProposalJsonBody(const std::string& text, std::string& json)
{
	const std::string trimmed = TrimWhitespace(text);
	EditProposalJson ignored;
	if (!trimmed.empty() && trimmed[0] == '{'
		&& ExtractEditProposalFields(trimmed, ignored)) {
		json = trimmed;
		return true;
	}

	size_t search = 0;
	while (search < text.size()) {
		const size_t fenceStart = text.find("```", search);
		if (fenceStart == std::string::npos)
			return false;

		const size_t infoStart = fenceStart + 3;
		const size_t lineEnd = text.find('\n', infoStart);
		if (lineEnd == std::string::npos)
			return false;

		std::string info = text.substr(infoStart, lineEnd - infoStart);
		info.erase(std::remove_if(info.begin(), info.end(),
			[](unsigned char c) { return std::isspace(c); }), info.end());
		std::transform(info.begin(), info.end(), info.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

		const size_t bodyStart = lineEnd + 1;
		const size_t fenceEnd = text.find("```", bodyStart);
		if (fenceEnd == std::string::npos)
			return false;

		const bool editLike = info == "haikode-edit" || info == "json";
		if (editLike) {
			const std::string body = TrimWhitespace(
				text.substr(bodyStart, fenceEnd - bodyStart));
			if (ExtractEditProposalFields(body, ignored)) {
				json = body;
				return true;
			}
		}
		search = fenceEnd + 3;
	}
	return false;
}


bool
ExtractJsonObjectAt(const std::string& json, size_t start, std::string& object)
{
	object.clear();
	if (start >= json.size() || json[start] != '{')
		return false;

	bool inString = false;
	bool escaping = false;
	int depth = 0;
	for (size_t pos = start; pos < json.size(); pos++) {
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
		} else if (c == '{') {
			depth++;
		} else if (c == '}') {
			depth--;
			if (depth == 0) {
				object = json.substr(start, pos - start + 1);
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
		if (pos >= json.size())
			return false;
		if (json[pos] == ']')
			return true;
		if (json[pos] != '{')
			return false;

		std::string object;
		if (!ExtractJsonObjectAt(json, pos, object))
			return false;
		objects.push_back(object);
		pos += object.size();

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
LooksLikeUnifiedDiff(const std::string& value)
{
	return value.find("--- ") != std::string::npos
		|| value.find("diff --git ") != std::string::npos;
}


std::string
BuildFullReplacementDiff(const std::string& path, const std::string& original,
	const std::string& replacement)
{
	const std::vector<std::string> oldLines = SplitLines(original);
	const std::vector<std::string> newLines = SplitLines(replacement);
	std::ostringstream diff;
	diff
		<< "diff --git a/" << path << " b/" << path << "\n"
		<< "--- a/" << path << "\n"
		<< "+++ b/" << path << "\n"
		<< "@@ -" << (oldLines.empty() ? 0 : 1) << ","
		<< oldLines.size() << " +" << (newLines.empty() ? 0 : 1)
		<< "," << newLines.size() << " @@\n";
	for (const std::string& line : oldLines)
		diff << "-" << line << "\n";
	for (const std::string& line : newLines)
		diff << "+" << line << "\n";
	return diff.str();
}


bool
ExtractJsonDiffBody(const std::string& text, std::string& rawDiff)
{
	const std::string trimmed = TrimTrailingWhitespace(text);
	if (trimmed.size() < 2 || trimmed[0] != '{'
		|| trimmed[trimmed.size() - 1] != '}') {
		return false;
	}

	std::vector<std::string> patchObjects;
	ExtractJsonObjectArrayField(trimmed, "patches", patchObjects);

	std::vector<std::string> diffs;
	for (const std::string& object : patchObjects) {
		for (const std::string& key : {"unified_diff", "diff", "patch"}) {
			std::string value;
			if (ExtractJsonStringField(object, key, value)
				&& LooksLikeUnifiedDiff(value)) {
				diffs.push_back(TrimTrailingWhitespace(value));
				break;
			}
		}
	}
	if (!diffs.empty()) {
		std::ostringstream joined;
		for (size_t i = 0; i < diffs.size(); i++) {
			if (i > 0)
				joined << "\n";
			joined << diffs[i];
		}
		rawDiff = joined.str();
		return true;
	}

	for (const std::string& key : {"unified_diff", "diff", "patch"}) {
		std::string value;
		if (ExtractJsonStringField(trimmed, key, value)
			&& LooksLikeUnifiedDiff(value)) {
			rawDiff = TrimTrailingWhitespace(value);
			return true;
		}
	}
	return false;
}


bool
ExtractFencedDiffBody(const std::string& text, std::string& rawDiff)
{
	size_t search = 0;
	while (search < text.size()) {
		const size_t fenceStart = text.find("```", search);
		if (fenceStart == std::string::npos)
			return false;

		const size_t infoStart = fenceStart + 3;
		const size_t lineEnd = text.find('\n', infoStart);
		if (lineEnd == std::string::npos)
			return false;

		std::string info = text.substr(infoStart, lineEnd - infoStart);
		info.erase(std::remove_if(info.begin(), info.end(),
			[](unsigned char c) { return std::isspace(c); }), info.end());
		std::transform(info.begin(), info.end(), info.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

		const size_t bodyStart = lineEnd + 1;
		const size_t fenceEnd = text.find("```", bodyStart);
		if (fenceEnd == std::string::npos)
			return false;

		const std::string body = TrimTrailingWhitespace(
			text.substr(bodyStart, fenceEnd - bodyStart));
		if ((info.empty() || info == "diff" || info == "patch")
			&& (body.find("--- ") != std::string::npos
				|| body.find("diff --git ") != std::string::npos)) {
			rawDiff = body;
			return true;
		}

		search = fenceEnd + 3;
	}
	return false;
}


bool
ExtractFencedJsonDiffBody(const std::string& text, std::string& rawDiff)
{
	size_t search = 0;
	while (search < text.size()) {
		const size_t fenceStart = text.find("```", search);
		if (fenceStart == std::string::npos)
			return false;

		const size_t infoStart = fenceStart + 3;
		const size_t lineEnd = text.find('\n', infoStart);
		if (lineEnd == std::string::npos)
			return false;

		std::string info = text.substr(infoStart, lineEnd - infoStart);
		info.erase(std::remove_if(info.begin(), info.end(),
			[](unsigned char c) { return std::isspace(c); }), info.end());
		std::transform(info.begin(), info.end(), info.begin(),
			[](unsigned char c) { return static_cast<char>(std::tolower(c)); });

		const size_t bodyStart = lineEnd + 1;
		const size_t fenceEnd = text.find("```", bodyStart);
		if (fenceEnd == std::string::npos)
			return false;

		const bool jsonLike = info == "json" || info == "haikode-patch"
			|| info == "haikode-diff" || info == "haikode-edit";
		if (jsonLike) {
			const std::string body = TrimTrailingWhitespace(
				text.substr(bodyStart, fenceEnd - bodyStart));
			if (ExtractJsonDiffBody(body, rawDiff))
				return true;
		}

		search = fenceEnd + 3;
	}
	return false;
}

} // namespace

bool
UnifiedDiff::Parse(const std::string& text, UnifiedDiff& diff, std::string& error)
{
	diff.fFiles.clear();
	error.clear();

	PatchFile* currentFile = nullptr;
	PatchHunk* currentHunk = nullptr;
	const std::vector<std::string> lines = SplitLines(text);

	for (size_t index = 0; index < lines.size(); index++) {
		const std::string& line = lines[index];
		const bool followedByNewPath = index + 1 < lines.size()
			&& lines[index + 1].rfind("+++ ", 0) == 0;
		if (line.rfind("--- ", 0) == 0 && followedByNewPath) {
			diff.fFiles.push_back(PatchFile());
			currentFile = &diff.fFiles.back();
			currentFile->oldPath = TrimPatchPath(line.substr(4));
			currentHunk = nullptr;
			continue;
		}

		if (line.rfind("+++ ", 0) == 0 && currentFile != nullptr) {
			currentFile->newPath = TrimPatchPath(line.substr(4));
			continue;
		}

		if (line.rfind("@@ ", 0) == 0 && currentFile != nullptr) {
			currentFile->hunks.push_back(PatchHunk());
			currentHunk = &currentFile->hunks.back();
			if (!ParseHunkHeader(line, *currentHunk)) {
				error = "Invalid unified diff hunk header.";
				return false;
			}
			continue;
		}

		if (currentHunk != nullptr && !line.empty()) {
			const char kind = line[0];
			if (kind == ' ' || kind == '-' || kind == '+')
				currentHunk->lines.push_back({kind, line.substr(1)});
		}
	}

	diff.fFiles.erase(std::remove_if(diff.fFiles.begin(), diff.fFiles.end(),
		[](const PatchFile& file) {
			return file.newPath.empty() || file.hunks.empty();
		}), diff.fFiles.end());

	if (diff.fFiles.empty()) {
		error = "No unified diff found.";
		return false;
	}
	return true;
}


bool
UnifiedDiff::ExtractFromText(const std::string& text, UnifiedDiff& diff,
	std::string& rawDiff, std::string& error)
{
	if (ExtractJsonDiffBody(text, rawDiff))
		return Parse(rawDiff, diff, error);

	if (ExtractFencedJsonDiffBody(text, rawDiff))
		return Parse(rawDiff, diff, error);

	if (ExtractFencedDiffBody(text, rawDiff))
		return Parse(rawDiff, diff, error);

	const size_t diffPos = text.find("diff --git ");
	const size_t oldPos = text.find("--- ");
	size_t start = std::string::npos;
	if (diffPos != std::string::npos)
		start = diffPos;
	else if (oldPos != std::string::npos)
		start = oldPos;

	if (start == std::string::npos) {
		error = "No unified diff found.";
		return false;
	}

	rawDiff = text.substr(start);
	return Parse(rawDiff, diff, error);
}


bool
UnifiedDiff::ExtractEditProposalFromText(const std::string& text,
	const std::string& projectRoot, UnifiedDiff& diff, std::string& rawDiff,
	std::string& error)
{
	diff.fFiles.clear();
	rawDiff.clear();
	error.clear();
	try {
		if (projectRoot.empty()) {
			error = "No active project root.";
			return false;
		}

		std::string json;
		if (!ExtractEditProposalJsonBody(text, json)) {
			error = "No haikode-edit proposal found.";
			return false;
		}

		EditProposalJson proposal;
		if (!ExtractEditProposalFields(json, proposal)) {
			error = "haikode-edit proposal is missing required fields.";
			return false;
		}

		const std::string path = TrimPatchPath(proposal.path);
		if (!ValidatePatchPath(path, false, error))
			return false;

		const fs::path root = fs::weakly_canonical(projectRoot);
		if (!RejectSymlinkPathComponents(root, path, error))
			return false;

		const fs::path target = fs::weakly_canonical(root / path);
		if (!IsInsideDirectory(target, root)) {
			error = "Unsafe patch path: " + path;
			return false;
		}

		std::string original;
		if (!ReadTextFileContents(target, original, error))
			return false;
		const std::string actualHash = Sha256Hex(original);
		if (Lowercase(TrimWhitespace(proposal.originalSha256))
			!= actualHash) {
			error = "haikode-edit proposal is stale: original_sha256 does not match current file.";
			return false;
		}
		if (proposal.replacement.find('\0') != std::string::npos) {
			error = "haikode-edit replacement is binary.";
			return false;
		}

		rawDiff = BuildFullReplacementDiff(path, original,
			proposal.replacement);
		return Parse(rawDiff, diff, error);
	} catch (const std::exception& exception) {
		error = exception.what();
		return false;
	}
}


bool
UnifiedDiff::SavePatchText(const std::string& projectRoot,
	const std::string& rawDiff, std::string& savedPath, std::string& error)
{
	savedPath.clear();
	error.clear();
	try {
		if (projectRoot.empty()) {
			error = "No active project root.";
			return false;
		}
		if (rawDiff.empty()) {
			error = "Cannot save an empty patch.";
			return false;
		}

		const fs::path root = fs::weakly_canonical(projectRoot);
		const fs::path patchesRoot = root / ".haikode" / "patches";
		fs::create_directories(patchesRoot);

		const fs::path patchPath = patchesRoot / ("patch-" + PatchTimestamp() + ".diff");
		if (!IsInsideDirectory(patchPath, root)) {
			error = "Unsafe patch save path.";
			return false;
		}

		std::ofstream file(patchPath, std::ios::binary | std::ios::trunc);
		if (!file) {
			error = "Could not save patch.";
			return false;
		}
		file << rawDiff;
		savedPath = patchPath.string();
		return true;
	} catch (const std::exception& exception) {
		error = exception.what();
		return false;
	}
}


bool
UnifiedDiff::ParseHunkHeader(const std::string& line, PatchHunk& hunk)
{
	size_t oldPos = line.find('-');
	size_t newPos = line.find('+', oldPos == std::string::npos ? 0 : oldPos);
	if (oldPos == std::string::npos || newPos == std::string::npos)
		return false;

	hunk.oldStart = std::atoi(line.c_str() + oldPos + 1);
	const char* oldComma = std::strchr(line.c_str() + oldPos, ',');
	hunk.oldCount = oldComma == nullptr ? 1 : std::atoi(oldComma + 1);

	hunk.newStart = std::atoi(line.c_str() + newPos + 1);
	const char* newComma = std::strchr(line.c_str() + newPos, ',');
	hunk.newCount = newComma == nullptr ? 1 : std::atoi(newComma + 1);
	return hunk.oldStart >= 0 && hunk.newStart >= 0;
}


std::vector<std::string>
UnifiedDiff::ChangedPaths() const
{
	std::vector<std::string> paths;
	paths.reserve(fFiles.size());
	for (const PatchFile& file : fFiles)
		paths.push_back(file.newPath);
	return paths;
}


std::vector<PatchFileStats>
UnifiedDiff::FileStats() const
{
	std::vector<PatchFileStats> stats;
	stats.reserve(fFiles.size());
	for (const PatchFile& file : fFiles)
		stats.push_back(StatsForFile(file));
	return stats;
}


int
UnifiedDiff::HunkCount() const
{
	int count = 0;
	for (const PatchFile& file : fFiles)
		count += static_cast<int>(file.hunks.size());
	return count;
}


std::string
BuildReviewText(const std::vector<PatchFile>& files)
{
	std::vector<PatchFileStats> stats;
	stats.reserve(files.size());
	for (const PatchFile& file : files)
		stats.push_back(StatsForFile(file));

	size_t additions = 0;
	size_t deletions = 0;
	size_t hunkCount = 0;
	for (const PatchFileStats& fileStats : stats) {
		additions += fileStats.additions;
		deletions += fileStats.deletions;
		hunkCount += fileStats.hunkCount;
	}

	std::ostringstream preview;
	preview << "Patch preview: " << files.size() << " file(s), "
		<< hunkCount << " hunk(s), +" << additions << " -" << deletions;
	for (size_t fileIndex = 0; fileIndex < files.size(); ++fileIndex) {
		const PatchFile& file = files[fileIndex];
		const PatchFileStats& fileStats = stats[fileIndex];
		preview << "\n\n" << FileSummaryText(fileStats);

		for (const PatchHunk& hunk : file.hunks) {
			preview << "\n" << HunkHeaderText(hunk);
			for (const PatchHunkLine& line : hunk.lines)
				preview << "\n" << line.kind << line.text;
		}
	}
	return preview.str();
}

std::string
UnifiedDiff::ReviewText() const
{
	return BuildReviewText(fFiles);
}


std::string
UnifiedDiff::ReviewTextForFile(const std::string& path) const
{
	for (const PatchFile& file : fFiles) {
		if (file.newPath == path)
			return BuildReviewText({file});
	}
	return "";
}


size_t
UnifiedDiff::HunkCountForFile(const std::string& path) const
{
	for (const PatchFile& file : fFiles) {
		if (file.newPath == path)
			return file.hunks.size();
	}
	return 0;
}


std::string
UnifiedDiff::ReviewTextForHunk(const std::string& path, size_t hunkIndex) const
{
	for (const PatchFile& file : fFiles) {
		if (file.newPath == path && hunkIndex < file.hunks.size()) {
			PatchFile hunkFile = file;
			hunkFile.hunks = {file.hunks[hunkIndex]};
			return BuildReviewText({hunkFile});
		}
	}
	return "";
}


std::vector<PatchReviewRow>
UnifiedDiff::ReviewRowsForFile(const std::string& path) const
{
	for (const PatchFile& file : fFiles) {
		if (file.newPath != path)
			continue;

		std::vector<PatchReviewRow> rows;
		rows.push_back({
			PatchReviewRowKind::File,
			file.newPath,
			0,
			0,
			FileSummaryText(StatsForFile(file))
		});

		for (const PatchHunk& hunk : file.hunks) {
			rows.push_back({
				PatchReviewRowKind::Hunk,
				file.newPath,
				hunk.oldStart,
				hunk.newStart,
				HunkHeaderText(hunk)
			});

			int oldLine = hunk.oldStart;
			int newLine = hunk.newStart;
			for (const PatchHunkLine& line : hunk.lines) {
				PatchReviewRow row;
				row.path = file.newPath;
				row.text = line.text;
				if (line.kind == '+') {
					row.kind = PatchReviewRowKind::Addition;
					row.newLine = newLine++;
				} else if (line.kind == '-') {
					row.kind = PatchReviewRowKind::Removal;
					row.oldLine = oldLine++;
				} else {
					row.kind = PatchReviewRowKind::Context;
					row.oldLine = oldLine++;
					row.newLine = newLine++;
				}
				rows.push_back(row);
			}
		}
		return rows;
	}
	return {};
}


bool
UnifiedDiff::Apply(const std::string& projectRoot, PatchApplyResult& result,
	std::string& error) const
{
	result = PatchApplyResult();
	error.clear();
	try {
		if (projectRoot.empty()) {
			error = "No active project root.";
			return false;
		}

		const fs::path root = fs::weakly_canonical(projectRoot);
		const fs::path backupRoot = root / ".haikode" / "backups" / Timestamp();
		bool wroteBackup = false;

		for (const PatchFile& file : fFiles) {
			if (!ValidatePatchPath(file.oldPath, true, error))
				return false;
			if (file.newPath == "/dev/null") {
				error = "File deletion patches are not enabled yet.";
				return false;
			}
			if (!ValidatePatchPath(file.newPath, false, error))
				return false;
		}

		for (const PatchFile& file : fFiles) {
			if (!RejectSymlinkPathComponents(root, file.newPath, error))
				return false;
			const fs::path target = file.oldPath == "/dev/null"
				? fs::weakly_canonical((root / file.newPath).parent_path())
					/ fs::path(file.newPath).filename()
				: fs::weakly_canonical(root / file.newPath);
			if (!IsInsideDirectory(target, root)) {
				error = "Unsafe patch path: " + file.newPath;
				return false;
			}

			std::vector<std::string> patched;
			if (file.oldPath == "/dev/null") {
				if (fs::exists(target)) {
					error = "New-file patch target already exists: " + file.newPath;
					return false;
				}
				if (!BuildNewFileContents(file.hunks, patched, error))
					return false;
			} else {
				std::vector<std::string> original;
				if (!ReadTextFile(target, original, error))
					return false;

				if (!ApplyHunks(original, file.hunks, patched, error))
					return false;

				const fs::path backup = backupRoot / file.newPath;
				fs::create_directories(backup.parent_path());
				fs::copy_file(target, backup, fs::copy_options::overwrite_existing);
				wroteBackup = true;
			}
			fs::create_directories(target.parent_path());
			if (!WriteTextFile(target, patched, error))
				return false;

			result.changedFiles.push_back(file.newPath);
		}

		if (wroteBackup)
			result.backupDirectory = backupRoot.string();
		return true;
	} catch (const std::exception& exception) {
		error = exception.what();
		return false;
	}
}


bool
UnifiedDiff::ApplyFile(const std::string& projectRoot, const std::string& path,
	PatchApplyResult& result, std::string& error) const
{
	for (const PatchFile& file : fFiles) {
		if (file.newPath == path) {
			UnifiedDiff singleFileDiff;
			singleFileDiff.fFiles.push_back(file);
			return singleFileDiff.Apply(projectRoot, result, error);
		}
	}

	result = PatchApplyResult();
	error = "Patch does not contain file: " + path;
	return false;
}


bool
UnifiedDiff::ApplyHunk(const std::string& projectRoot, const std::string& path,
	size_t hunkIndex, PatchApplyResult& result, std::string& error) const
{
	result = PatchApplyResult();
	error.clear();
	try {
		if (projectRoot.empty()) {
			error = "No active project root.";
			return false;
		}
		if (!IsSafeRelativePath(path)) {
			error = "Unsafe patch path: " + path;
			return false;
		}
		if (IsSensitivePatchPath(path)) {
			error = "Refusing to patch sensitive project metadata: " + path;
			return false;
		}

		const PatchFile* patchFile = nullptr;
		for (const PatchFile& file : fFiles) {
			if (file.newPath == path) {
				patchFile = &file;
				break;
			}
		}
		if (patchFile == nullptr) {
			error = "Patch does not contain file: " + path;
			return false;
		}
		if (hunkIndex >= patchFile->hunks.size()) {
			error = "Patch hunk index is outside the selected file.";
			return false;
		}
		if (!ValidatePatchPath(patchFile->oldPath, true, error))
			return false;
		if (patchFile->newPath == "/dev/null") {
			error = "File deletion patches are not enabled yet.";
			return false;
		}
		if (!ValidatePatchPath(patchFile->newPath, false, error))
			return false;
		if (patchFile->oldPath == "/dev/null" && patchFile->hunks.size() > 1) {
			error = "Apply the whole selected file to create a multi-hunk new file.";
			return false;
		}

		const fs::path root = fs::weakly_canonical(projectRoot);
		const fs::path backupRoot = root / ".haikode" / "backups" / Timestamp();
		if (!RejectSymlinkPathComponents(root, path, error))
			return false;
		const fs::path target = patchFile->oldPath == "/dev/null"
			? fs::weakly_canonical((root / path).parent_path())
				/ fs::path(path).filename()
			: fs::weakly_canonical(root / path);
		if (!IsInsideDirectory(target, root)) {
			error = "Unsafe patch path: " + path;
			return false;
		}

		std::vector<std::string> patched;
		if (patchFile->oldPath == "/dev/null") {
			if (fs::exists(target)) {
				error = "New-file patch target already exists: " + path;
				return false;
			}
			if (!BuildNewFileContents({patchFile->hunks[hunkIndex]}, patched,
					error)) {
				return false;
			}
		} else {
			std::vector<std::string> original;
			if (!ReadTextFile(target, original, error))
				return false;
			if (!ApplySingleHunkToLines(original, patchFile->hunks[hunkIndex],
					patched, error)) {
				return false;
			}

			const fs::path backup = backupRoot / path;
			fs::create_directories(backup.parent_path());
			fs::copy_file(target, backup, fs::copy_options::overwrite_existing);
		}

		fs::create_directories(target.parent_path());
		if (!WriteTextFile(target, patched, error))
			return false;

		result.changedFiles.push_back(path);
		if (patchFile->oldPath != "/dev/null")
			result.backupDirectory = backupRoot.string();
		return true;
	} catch (const std::exception& exception) {
		error = exception.what();
		return false;
	}
}


bool
UnifiedDiff::RemoveFile(const std::string& path)
{
	const auto oldSize = fFiles.size();
	fFiles.erase(std::remove_if(fFiles.begin(), fFiles.end(),
		[&path](const PatchFile& file) {
			return file.newPath == path;
		}), fFiles.end());
	return fFiles.size() != oldSize;
}


bool
UnifiedDiff::RemoveHunk(const std::string& path, size_t hunkIndex)
{
	for (auto file = fFiles.begin(); file != fFiles.end(); ++file) {
		if (file->newPath != path)
			continue;
		if (hunkIndex >= file->hunks.size())
			return false;

		file->hunks.erase(file->hunks.begin() + hunkIndex);
		if (file->hunks.empty())
			fFiles.erase(file);
		return true;
	}
	return false;
}


bool
UnifiedDiff::RemoveFirstFile(std::string* removedPath)
{
	if (fFiles.empty())
		return false;

	if (removedPath != nullptr)
		*removedPath = fFiles.front().newPath;
	fFiles.erase(fFiles.begin());
	return true;
}

} // namespace Haikode::AI
