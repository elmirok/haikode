/*
 * Copyright 2025, Stefano Ceccherini
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

// TODO: file_ref vs ref
// TODO: Use these defines instead of the values
#define MSG_FIELD_FILENAME			"file_name"
#define MSG_FIELD_FILEREF			"file_ref"
#define MSG_FIELD_LINE				"line"
#define MSG_FIELD_NEEDSSAVE			"needs_save"
#define MSG_FIELD_REF				"ref"
#define MSG_FIELD_SYMBOLS			"symbols"
#define MSG_FIELD_STATUS			"status"
#define MSG_FIELD_BUILDING			"building"
#define MSG_FIELD_BUILDCMDTYPE		"cmd_type"
#define MSG_FIELD_BUILDPROFILENAME	"build_profile_name"
#define MSG_FIELD_PROJECTNAME		"project_name"
#define MSG_FIELD_PROJECTPATH		"project_path"
#define MSG_FIELD_CURRENTBRANCH		"current_branch"
#define MSG_FIELD_ACTIVEPROJECTNAME	"active_project_name"
#define MSG_FIELD_ACTIVEPROJECTPATH	"active_project_path"


// "notification" messages
enum {
	// editor
	MSG_NOTIFY_EDITOR_FILE_OPENED 		= 'efop',	// file_name (string)
													// file_ref (entry_ref) (optional)
	MSG_NOTIFY_EDITOR_FILE_CLOSED 		= 'efcx',	// file_name (string)
													// file_ref (entry_ref) (optional)
	MSG_NOTIFY_EDITOR_POSITION_CHANGED	= 'epch',	// file_name (string)
													// file_ref (entry_ref) (optional)
													// line (int32)

	MSG_NOTIFY_FILE_SAVE_STATUS_CHANGED = 'stch',	// file_name (string)
													// file_ref (entry_ref) (optional)
													// needs_save (bool)

	// tab selected
	MSG_NOTIFY_EDITOR_FILE_SELECTED		= 'efsl',	// ref (ref)

	MSG_NOTIFY_EDITOR_SYMBOLS_UPDATED	= 'esup',	// ref (ref)
													// symbols (BMessage)
													// status (int32)

	MSG_NOTIFY_BUILDING_PHASE			= 'blph',	// building (bool)
													// cmd_type (string)
													// project_name (string)
													// project_path (string)
													// status (int32)

	MSG_NOTIFY_LSP_INDEXING				= 'lsid',

	MSG_NOTIFY_PROJECT_LIST_CHANGED		= 'nplc',	// project_name (string array)
													// project_path (string array)
													// active_project_name (string)
													// active_project_path (string)

	// Project
	MSG_NOTIFY_PROJECT_SET_ACTIVE		= 'npsa',	// active_project_name (string)
													// active_project_path (string)
	MSG_NOTIFY_PROJECT_BUILD_PROFILE_CHANGED = 'nppc',	// project_name (string)
														// build_profile_name (string)

	MSG_NOTIFY_FIND_STATUS				= 'fist',

	// workspace
	MSG_NOTIFY_WORKSPACE_PREPARATION_STARTED = 'wkps',
	MSG_NOTIFY_WORKSPACE_PREPARATION_COMPLETED = 'wkpc',

	// git / source control
	MSG_NOTIFY_GIT_BRANCH_CHANGED = 'gbch'			// current_branch (string)
													// project_path (string)
};

