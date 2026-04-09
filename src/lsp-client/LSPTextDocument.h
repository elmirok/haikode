/*
 * Copyright 2023, Andrea Anzani
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#pragma once

#include "Log.h"
#include <Url.h>


class LSPTextDocument {
public:
    LSPTextDocument(BPath filePath, BString fileType)
		:
		fFilenameURI(BUrl(filePath)),
		fFileType(fileType)
	{
		fFilenameURI.SetAuthority("");
	}

	virtual ~LSPTextDocument() = default;

    const BString	GetFilenameURI() const { return fFilenameURI.UrlString();}
	const BString	GetFileStatus()	const { return fFileStatus; }

			void	SetFileStatus(BString newStatus) { fFileStatus = newStatus; }
			void	SetFileType(BString newType) { fFileType = newType; }

	const BString& FileType() const { return fFileType; }

			int32	Version() const { return fVersion; }
			bool 	IsStaleResponse(int32 requestVersion) const {
						if (Version() != requestVersion) {
							LogTrace("[%s] Discarding stale response (req=%ld, cur=%ld)",
								GetFilenameURI().String(), requestVersion, Version());
							return true;
					}
    return false;
}


protected:
			void	ResetVersion() { fVersion = 0; }
			int32	NextVersion() { return ++fVersion; }


private:
	BUrl 	fFilenameURI;
	BString	fFileStatus;
	BString fFileType;
	int32	fVersion{0};
};
