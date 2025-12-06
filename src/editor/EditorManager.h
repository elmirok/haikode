/*
 * Copyright 2025 Andrea Anzani <andrea.anzani@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include <Messenger.h>
#include <String.h>
#include <SupportDefs.h>

#include <string>

class IEditor;
struct entry_ref;

/**
 * EditorManager - Factory for creating editor instances
 *
 * This class manages the creation of IEditor instances based on file type.
 */
class EditorManager {
public:
	static bool			IsFileSupported(const entry_ref* ref);
	static bool			IsFileSupported(const entry_ref* ref, BString& outFileType);
	static IEditor*		CreateEditor(entry_ref* ref, const BMessenger& target,
							const std::string& fileType = "");

private:
	// Private constructor - this is a utility class
	EditorManager() = delete;
	~EditorManager() = delete;
	EditorManager(const EditorManager&) = delete;
	EditorManager& operator=(const EditorManager&) = delete;
};
