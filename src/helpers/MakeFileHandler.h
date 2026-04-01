/*
 * Copyright 2024-2026, the Genio team
 * All rights reserved. Distributed under the terms of the MIT license.
 */
 
#pragma once

#include <String.h>

class MakeFileHandler {
public:
	MakeFileHandler();
	MakeFileHandler(const char* path);
	virtual ~MakeFileHandler();
	
	status_t SetTo(const char* path);
	
	void GetTargetName(BString &outName) const;
	void SetTargetName(const BString& inName);

	void GetTargetDirectory(BString& outDir) const;
	void SetTargetDirectory(const BString& outDir);

	void GetFullTargetName(BString& fullName) const;

private:
	BString fTargetName;
	BString fTargetDir;
};
