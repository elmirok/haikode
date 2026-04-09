/*
 * Copyright 2023, Genio
 * All rights reserved. Distributed under the terms of the MIT license.
 */


#include "BranchItem.h"

#include <Font.h>

BranchItem::BranchItem(const char* branchName,
						const char* text,
						uint32 branchType,
						uint32 outlineLevel,
						bool expanded)
	:
	StyledItem(text, outlineLevel, expanded),
	fBranchType(branchType),
	fBranchName(branchName)
{
}


/* virtual */
BranchItem::~BranchItem()
{
}


uint32
BranchItem::BranchType() const
{
	return fBranchType;
}


const char*
BranchItem::BranchName() const
{
	return fBranchName.String();
}


void
BranchItem::SetCurrentBranch(bool selected)
{
	SetTextFontFace(selected ? B_UNDERSCORE_FACE : B_REGULAR_FACE);
}


bool
BranchItem::IsCurrentBranch() const
{
	return TextFontFace() == B_UNDERSCORE_FACE;
}

