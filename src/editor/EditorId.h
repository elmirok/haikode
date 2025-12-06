/*
 * Copyright 2025, Andrea Anzani 
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once


#include <SupportDefs.h>

typedef uint64	editor_id;
#define kEditorId "editor:id"

/**
 * Generate a unique editor ID
 * Thread-safe global ID generator for all editor types
 */
inline editor_id GenerateEditorId() {
	static editor_id g_editor_id = 0;
	return ++g_editor_id;
}
