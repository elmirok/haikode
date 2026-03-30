/*
 * Copyright 2026, Stefano Ceccherini
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include <vector>

#include <Bitmap.h>
#include <Locker.h>

#include <set>

class SpinningAnimation {
public:
	static void		Draw(BView* owner, BRect bounds);
	static status_t	Initialize(BView* view);
	static status_t Dispose(BView* view);

private:
	static status_t _LoadIcons();
	static int32	_AnimationThread(void*);

	static int32 sBuildAnimationIndex;
	static std::vector<BBitmap*> sBuildAnimationFrames;
	static thread_id sThread;
	static BLocker sLocker;
	static std::set<BMessenger> sMessengers;
	static sem_id sSemaphore;
};

