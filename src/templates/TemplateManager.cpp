/*
 * Copyright The Genio Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "TemplateManager.h"

#include <algorithm>
#include <chrono>
#include <fstream>
#include <iostream>
#include <map>
#include <regex>

#include <Application.h>
#include <AppFileInfo.h>
#include <Catalog.h>
#include <CopyEngine.h>
#include <Debug.h>
#include <Directory.h>
#include <Entry.h>
#include <EntryOperationEngineBase.h>
#include <Path.h>
#include <Roster.h>

#include "ConfigManager.h"
#include "FSUtils.h"
#include "GenioApp.h"
#include "Log.h"
#include "Utils.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "TemplateManager"


const char* kTemplateDirectory = "templates";

using Entry = BPrivate::BEntryOperationEngineBase::Entry;

class CustomCopyEngineController: public BCopyEngine::BController {
public:
	CustomCopyEngineController(const char* projectName, const BPath& sourcePath,
		const BPath& destPath);

private:
	bool EntryStarted(const char* path) override;
	bool EntryFinished(const char* path, status_t error) override;

	BString fProjectName;
	BPath fSourcePath;
	BPath fDestPath;

	typedef std::map<std::string, std::string> string_map;
	string_map fReplacements;
};


// TemplateManager
TemplateManager* TemplateManager::sManager = nullptr;


TemplateManager::TemplateManager()
{
}


TemplateManager::~TemplateManager()
{
}


/* static */
status_t
TemplateManager::Initialize()
{
	if (sManager == nullptr)
		sManager = new (std::nothrow) TemplateManager();

	if (sManager == nullptr)
		return B_NO_MEMORY;

	return sManager->_LoadTemplates();
}


/* static */
void
TemplateManager::Dispose()
{
	delete sManager;
	sManager = nullptr;
}


/* static */
TemplateManager*
TemplateManager::Get()
{
	ASSERT(sManager != nullptr);
	return sManager;
}


status_t
TemplateManager::GetTemplatesList(entry_list& templates)
{
	templates = fTemplates;
	return B_OK;
}


status_t
TemplateManager::GetUserTemplatesList(entry_list &userTemplates)
{
	// TODO: Don't load every time
	status_t status = _LoadUserTemplates();
	if (status == B_OK)
		userTemplates = fUserTemplates;
	return status;
}


status_t
TemplateManager::CopyFileTemplate(const entry_ref* source, const entry_ref* destination, entry_ref* newFileRef)
{
	// Copy template file to destination
	BPath sourcePath(source);
	Entry sourceEntry(sourcePath.Path());

	BPath destPath(destination);
	destPath.Append(source->name);
	Entry destEntry(destPath.Path());

	// TODO: get the real project name, not the leaf
	BCopyEngine copyEngine;
	CustomCopyEngineController controller(destPath.Leaf(), sourcePath, destPath);
	copyEngine.SetController(&controller);

	status_t status = copyEngine.CopyEntry(sourceEntry, destEntry);
	if (status != B_OK) {
		BString err(strerror(status));
		LogError("Error creating new file %s in %s: %s", sourcePath.Path(), destPath.Path(),err.String());
	} else {
		FSMakeWritable(destPath.Path());
		if (newFileRef != nullptr) {
			get_ref_for_path(destPath.Path(), newFileRef);
		}
	}

	return status;
}


status_t
TemplateManager::CopyProjectTemplate(const entry_ref* source, const entry_ref* destination,
										const char* name)
{
	BPath sourcePath(source);
	Entry sourceEntry(sourcePath.Path());

	BPath destPath(destination);
	destPath.Append(name);
	Entry destEntry(destPath.Path());

	CustomCopyEngineController controller(name, sourcePath, destPath);
	BCopyEngine copyEngine(BCopyEngine::COPY_RECURSIVELY);
	copyEngine.SetController(&controller);
	status_t status = copyEngine.CopyEntry(sourceEntry, destEntry);
	if (status != B_OK) {
		BString err(strerror(status));
		LogError("Error creating new template %s in %s: %s", sourcePath.Path(), destPath.Path(),err.String());
	} else {
		// TODO: Default templates in a tipical HPKG installation are readonly
		// we are setting write permissions recursively here
		FSMakeWritable(destPath.Path(), true);
	}

	return status;
}


status_t
TemplateManager::CreateTemplate(const entry_ref* file)
{
	return B_OK;
}


status_t
TemplateManager::CreateNewFolder(const entry_ref* destination, entry_ref* newFolderRef)
{
	BDirectory dir(destination);
	BDirectory newDir;
	status_t status = dir.InitCheck();
	if (status != B_OK) {
		LogError("Invalid destination directory [%s] (%s)", destination->name, strerror(status));
		return status;
	}
	status = dir.CreateDirectory(B_TRANSLATE("New folder"), &newDir);
	if (status != B_OK) {
		LogError("Invalid destination directory [%s] (%s)", destination->name, strerror(status));
	} else if (newFolderRef != nullptr && newDir.InitCheck() == B_OK) {
		BEntry entry;
		newDir.GetEntry(&entry);
		entry.GetRef(newFolderRef);
	}
	return status;
}


BString
TemplateManager::GetDefaultTemplateDirectory()
{
	// Default template directory
	BPath templatePath = GetNearbyDataDirectory();
	templatePath.Append(kTemplateDirectory);
	return templatePath.Path();
}


BString
TemplateManager::GetUserTemplateDirectory()
{
	// User template directory
	BPath userPath = GetUserSettingsDirectory();
	userPath.Append(kTemplateDirectory);
	create_directory(userPath.Path(), 0777);

	return userPath.Path();
}


status_t
TemplateManager::_LoadTemplates()
{
	fTemplates.clear();

	BDirectory templatesDir(GetDefaultTemplateDirectory());
	entry_ref ref;
	while (templatesDir.GetNextRef(&ref) == B_OK) {
		fTemplates.push_back(ref);
	}

	return B_OK;
}


status_t
TemplateManager::_LoadUserTemplates()
{
	fUserTemplates.clear();

	entry_ref ref;
	BDirectory userTemplatesDir(GetUserTemplateDirectory());
	while (userTemplatesDir.GetNextRef(&ref) == B_OK) {
		fUserTemplates.push_back(ref);
	}

	return B_OK;
}


static void
ReplaceStringsInFile(const char* filePath, std::map<std::string, std::string> replacements)
{
	std::ifstream inputFile(filePath);
	std::regex pattern(R"(\$\{([^}]+)\})");
	std::string line;
	std::string fileContent;
	while (std::getline(inputFile, line)) {
		std::smatch match;
		std::string processedLine;
		std::string::const_iterator searchStart(line.cbegin());
		while (std::regex_search(searchStart, line.cend(), match, pattern)) {
			std::string key = match[1];
			processedLine += std::string(searchStart, match[0].first);
			std::map<std::string, std::string>::iterator replacement = replacements.find(key);
			if (replacement != replacements.end()) {
				LogDebug("Replacing ${%s} with %s", key.c_str(), replacement->second.c_str());
				processedLine += replacement->second;
			} else {
				LogDebug("Warning: No replacement for ${%s}", key.c_str());
				processedLine += match[0];
			}
			searchStart = match.suffix().first;
		}
		processedLine += std::string(searchStart, line.cend());
		fileContent += processedLine + "\n";
	}

	inputFile.close();

	std::ofstream outputFile(filePath);

	outputFile << fileContent;
	outputFile.close();
}


// CustomCopyEngineController
CustomCopyEngineController::CustomCopyEngineController(const char* projectName,
	const BPath& sourcePath, const BPath& destPath)
	:
	BCopyEngine::BController(),
	fProjectName(projectName),
	fSourcePath(sourcePath),
	fDestPath(destPath)
{
	std::string authorNameWithoutSpaces = (const char*)gCFG["author_name"];
	authorNameWithoutSpaces.erase(std::remove(authorNameWithoutSpaces.begin(),
					authorNameWithoutSpaces.end(), ' '), authorNameWithoutSpaces.end());

	auto now = std::chrono::system_clock::now();
	std::chrono::year_month_day ymd{std::chrono::floor<std::chrono::days>(now)};
	std::ostringstream s;
	s << ymd.year();
	std::string year = s.str();

	// TODO: Put these into documentation
	std::map<std::string, std::string> replacements = {
		{ "project.name", std::string(fProjectName.String()) },
		{ "author.name", (const char*)(gCFG["author_name"]) },
		{ "author.email", (const char*)(gCFG["author_email"]) },
		{ "author.name_without_spaces", authorNameWithoutSpaces },
		{ "date.year", year }
	};

	if (Logger::IsTraceEnabled()) {
		for (auto r: replacements) {
			std::cout << r.first << ": " << r.second << std::endl;
		}
	}
	fReplacements = replacements;
}


/* virtual */
bool
CustomCopyEngineController::EntryStarted(const char* path)
{
	LogDebug("Start copying %s", path);
	return BCopyEngine::BController::EntryStarted(path);
}


/* virtual */
bool
CustomCopyEngineController::EntryFinished(const char* path, status_t error)
{
	BString destination(path);
	destination.ReplaceFirst(fSourcePath.Path(), fDestPath.Path());
	LogDebug("Finished copying %s to %s: %s", path, destination.String(), ::strerror(error));

	BPath filePath(destination.String());
	if (BEntry(filePath.Path()).IsFile()) {
		ReplaceStringsInFile(filePath.Path(), fReplacements);
	}

	return BCopyEngine::BController::EntryFinished(path, error);
}
