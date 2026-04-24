/*
 * Copyright The Genio Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "TemplateManager.h"

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

#include "FSUtils.h"
#include "Log.h"
#include "Utils.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "TemplateManager"


const char* kTemplateDirectory = "templates";

using Entry = BPrivate::BEntryOperationEngineBase::Entry;

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

	return B_OK;
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
TemplateManager::CopyFileTemplate(const entry_ref* source, const entry_ref* destination, entry_ref* newFileRef)
{
	// Copy template file to destination
	BPath sourcePath(source);
	Entry sourceEntry(sourcePath.Path());

	BPath destPath(destination);
	destPath.Append(source->name);
	Entry destEntry(destPath.Path());

	status_t status = BCopyEngine().CopyEntry(sourceEntry, destEntry);
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

	status_t status = BCopyEngine(BCopyEngine::COPY_RECURSIVELY).CopyEntry(sourceEntry, destEntry);
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
	// the templates folder
	
	BDirectory templatesDir(GetDefaultTemplateDirectory());
	entry_ref ref;
	while (templatesDir.GetNextRef(&ref) == B_OK) {
		fTemplates.push_back(ref);
/*		BNode node(&entry);
		BNodeInfo nodeInfo(&node);

		char fileName[B_FILE_NAME_LENGTH];
		entry.GetName(fileName);
		if (nodeInfo.InitCheck() == B_OK) {
			char mimeType[B_MIME_TYPE_LENGTH];
			nodeInfo.GetType(mimeType);

			BMimeType mime(mimeType);
			if (mime.IsValid()) {
				entry_ref ref;
				entry.GetRef(&ref);
				if (entry.IsDirectory())
					message->AddString("type", "new_folder_template");
				else
					message->AddString("type", "new_file_template");
				auto item = new IconMenuItem(fileName, message, &nodeInfo, B_MINI_ICON);
				item->SetEnabled(itemEnabled);
				AddItem(item);
				count++;
			}
		}*/
	}
	
	return B_OK;
}

