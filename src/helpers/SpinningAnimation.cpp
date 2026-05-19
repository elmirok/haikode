/*
 * Copyright 2025, Stefano Ceccherini
 * All rights reserved. Distributed under the terms of the MIT license.
 */


#include "SpinningAnimation.h"

#include <Bitmap.h>
#include <set>
#include <vector>

#include <Application.h>
#include <Autolock.h>
#include <MessageRunner.h>
#include <Resources.h>
#include <TranslationUtils.h>
#include <View.h>

#include "Log.h"


static int32 sBuildAnimationIndex = 0;
static std::vector<BBitmap*> sBuildAnimationFrames;
static BLocker sLocker("SpinningAnimation locker");
static thread_id sThread = -1;
static sem_id sSemaphore = -1;
static std::set<BMessenger> sMessengers;

/* static */
void
SpinningAnimation::Draw(BView* owner, BRect bounds)
{
	try {
		const BBitmap* frame = sBuildAnimationFrames.at(sBuildAnimationIndex);
		if (frame != nullptr) {
			owner->SetDrawingMode(B_OP_ALPHA);
			bounds.left = owner->PenLocation().x + 5;
			bounds.right = bounds.left + bounds.Height();
			bounds.OffsetBy(-1, 1);
			owner->DrawBitmap(frame, frame->Bounds(), bounds);
		}
	} catch (...) {
		// nothing to do
	}
}


/* static */
status_t
SpinningAnimation::Initialize(BView* view)
{
	BAutolock _(sLocker);

	// fail if the view is already there
	if (sMessengers.find(view) != sMessengers.end())
		return B_ERROR;

	sMessengers.insert(view);

	if (sThread < 0) {
		// first time called, initialize
		status_t status = _LoadIcons();
		if (status == B_OK) {
			sSemaphore = create_sem(1, "SpinningAnimation semaphore");
			sThread = spawn_thread(_AnimationThread, "SpinningAnimation thread", B_NORMAL_PRIORITY, nullptr);
			resume_thread(sThread);
		} else
			return B_ERROR;
	}

	return B_OK;
}


/* static */
status_t
SpinningAnimation::Dispose(BView* view)
{
	BAutolock _(sLocker);

	sMessengers.erase(view);

	// Only dispose things if there aren't any connected views
	if (sMessengers.size() != 0)
		return B_OK;

	delete_sem(sSemaphore);
	sSemaphore = -1;

	status_t status;
	wait_for_thread(sThread, &status);
	sThread = -1;

	for (std::vector<BBitmap*>::iterator i = sBuildAnimationFrames.begin();
		i != sBuildAnimationFrames.end(); i++) {
		delete *i;
	}
	sBuildAnimationFrames.clear();

	return B_OK;
}


/* static */
status_t
SpinningAnimation::_LoadIcons()
{
	// icon names are "waiting-N" where N is the index 1 to 6
	BString iconNamePrefix = "waiting-";
	const int32 numIcons = 6;

	BResources* resources = BApplication::AppResources();
	for (int32 i = 1; i <= numIcons; i++) {
		BString name(iconNamePrefix);
		const int32 kBrightnessBreakValue = 126;
		const rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);
		if (base.Brightness() >= kBrightnessBreakValue)
			name.Append("light-");
		else
			name.Append("dark-");
		name << i;
		size_t size;
		const void* rawData = resources->LoadResource(B_RAW_TYPE, name.String(), &size);
		if (rawData == nullptr) {
			LogError("InitAnimationIcons: Cannot load resource");
			break;
		}
		BMemoryIO mem(rawData, size);
		BBitmap* frame = BTranslationUtils::GetBitmap(&mem);

		sBuildAnimationFrames.push_back(frame);
	}

	return B_OK;
}


/* static */
int32
SpinningAnimation::_AnimationThread(void* castToThis)
{
	while (true) {
		status_t status;
		do {
			status = acquire_sem(sSemaphore);
		} while (status == B_INTERRUPTED);

		if (status != B_OK)
			break;

		sLocker.Lock();

		if (++sBuildAnimationIndex >= (int32)sBuildAnimationFrames.size())
			sBuildAnimationIndex = 0;

		for (BMessenger messenger : sMessengers) {
			messenger.SendMessage(B_INVALIDATE);
		}

		sLocker.Unlock();

		release_sem(sSemaphore);

		snooze(100000);
	}

	return 0;
}
