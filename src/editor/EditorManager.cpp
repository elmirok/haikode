/*
 * Copyright 2025 Andrea Anzani <andrea.anzani@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "EditorManager.h"

#include "Editor.h"
#include "ImageEditor.h"
#include "EmptyEditor.h"
#include "IEditor.h"
#include "Languages.h"
#include "Log.h"
#include "Utils.h"

#include <Entry.h>
#include <Node.h>
#include <NodeInfo.h>
#include <Path.h>
#include <String.h>

#include <cstring>

static bool
IsImageFile(const char* filename)
{
	if (filename == nullptr)
		return false;

	BString name(filename);
	name.ToLower();

	// Check common image extensions
	return name.EndsWith(".png") || name.EndsWith(".jpg") ||
	       name.EndsWith(".jpeg") || name.EndsWith(".gif") ||
	       name.EndsWith(".bmp") || name.EndsWith(".tga") ||
	       name.EndsWith(".tiff") || name.EndsWith(".tif") ||
	       name.EndsWith(".webp") || name.EndsWith(".ico");
}

/* static */ bool
EditorManager::IsFileSupported(const entry_ref* ref)
{
	BString fileType;
	return IsFileSupported(ref, fileType);
}

/* static */ bool
EditorManager::IsFileSupported(const entry_ref* ref, BString& outFileType)
{
	outFileType.SetTo("");

	if (ref == nullptr)
		return false;

	BNode entry(ref);
	if (entry.InitCheck() != B_OK || entry.IsDirectory())
		return false;

	// Check if it's an image file
	if (IsImageFile(ref->name)) {
		outFileType = "image";
		return true;
	}

	// Check if it's a text file supported by Languages
	BPath path(ref);
	std::string stdFileType;
	if (Languages::GetLanguageForExtension(GetFileExtension(path.Path()), stdFileType)) {
		outFileType = stdFileType.c_str();
		return true;
	}

	// Check MIME type for text or image files
	BNodeInfo info(&entry);
	if (info.InitCheck() == B_OK) {
		char mime[B_MIME_TYPE_LENGTH + 1];
		if (info.GetType(mime) != B_OK) {
			LogError("Error in getting mime type from file [%s]", path.Path());
			mime[0] = '\0';
		}
		if (mime[0] == '\0' || ::strcmp(mime, "application/octet-stream") == 0) {
			if (update_mime_info(path.Path(), false, true, B_UPDATE_MIME_INFO_FORCE_UPDATE_ALL) == B_OK) {
				if (info.GetType(mime) != B_OK) {
					LogError("Error in getting mime type from file [%s]", path.Path());
					mime[0] = '\0';
				}
			}
		}

		if (::strncmp(mime, "text/", 5) == 0) {
			// Generic text file, leave outFileType empty if not already set
			return true;
		}

		if (::strncmp(mime, "image/", 6) == 0) {
			outFileType = "image";
			return true;
		}
	}

	return false;
}

/* static */ IEditor*
EditorManager::CreateEditor(entry_ref* ref, const BMessenger& target,
	const std::string& fileType)
{
	if (ref == nullptr) {
		return new EmptyEditor();
	}

	// Check if file is supported and get detected type
	BString detectedType;
	if (!IsFileSupported(ref, detectedType)) {
		return nullptr;
	}

	// Use provided fileType if given, otherwise use detected type
	BString editorType = fileType.empty() ? detectedType : BString(fileType.c_str());

	// Determine the editor type based on detected or provided file type
	IEditor* editor = nullptr;

	// Check if it's an image file
	if (editorType == "image") {
		editor = new ImageEditor(ref, target);
	} else {
		// Default to standard text editor
		editor = new Editor(ref, target);
	}

	// Set the file type on the editor
	if (editorType.Length() > 0 && editor != nullptr) {
		editor->SetFileType(editorType.String());
	}

	return editor;
}
