/*
 * Copyright 2023, Andrea Anzani 
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#pragma once

#include "LSPCompat.h"

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

    virtual void onNotify(std::string method, value &params) {}
    virtual void onResponse(RequestID ID, value &result) {}
    virtual void onError(RequestID ID, value &error) {}
    virtual void onRequest(std::string method, value &params, value &ID) {}

private:
	BUrl 	fFilenameURI;
	BString	fFileStatus;
	BString fFileType;
};
