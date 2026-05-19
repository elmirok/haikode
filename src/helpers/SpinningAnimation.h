/*
 * Copyright 2026, Stefano Ceccherini
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once



#include <Rect.h>
#include <SupportDefs.h>

class BView;
class SpinningAnimation {
public:
	static void		Draw(BView* owner, BRect bounds);
	static status_t	Initialize(BView* view);
	static status_t Dispose(BView* view);

private:
	static status_t _LoadIcons();
	static int32	_AnimationThread(void*);
};

