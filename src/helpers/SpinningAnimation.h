/*
 * Copyright 2026, Stefano Ceccherini
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once


#include <Rect.h>
#include <SupportDefs.h>

class BListItem;
class BView;
class SpinningAnimation {
public:
	static void		Draw(BView* owner, BRect bounds);
	static status_t	RegisterItem(BView* view, BListItem* item);
	static status_t UnregisterItem(BView* view, BListItem* item);

private:
	static status_t _LoadIcons();
	static int32	_AnimationThread(void*);
};

