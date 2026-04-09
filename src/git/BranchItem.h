/*
 * Copyright 2023, Genio
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include "StyledItem.h"

// TODO: Once GitRepository is decoupled from libgit, include
// it and use GitRepository::branch_type definitions from there

class BranchItem : public StyledItem {
public:
	BranchItem(const char *branchName,
				const char* text,
				uint32 branchType = -1,
				uint32 outlineLevel = 0,
				bool expanded = true);
	virtual 	~BranchItem();
	uint32 		BranchType() const;
	const char*	BranchName() const;

	void		SetCurrentBranch(bool selected);
	bool		IsCurrentBranch() const;

private:
	uint32 		fBranchType;
	BString 	fBranchName;
};
