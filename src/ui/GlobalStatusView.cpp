/*
 * Copyright 2013-2024, Stefano Ceccherini 
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include "GlobalStatusView.h"


#include <Catalog.h>
#include <ControlLook.h>
#include <GridLayoutBuilder.h>
#include <LayoutBuilder.h>
#include <LayoutUtils.h>
#include <Message.h>
#include <MessageRunner.h>
#include <StatusBar.h>
#include <StringView.h>
#include <Window.h>

#include "BuildProfileView.h"
#include "BuildStatusView.h"
#include "LSPStatusView.h"
#include "NoticeMessages.h"
#include "Utils.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "GlobalStatusView"


const bigtime_t kTextAutohideTimeout = 5000000ULL;

const uint32 kHideFindText = 'HIFE';

GlobalStatusView::GlobalStatusView()
	:
	BView("global_status_view", B_WILL_DRAW),
	fBuildStatusView(nullptr),
	fLSPStatusView(nullptr),
	fLastFindStatus(nullptr),
	fBuildProfileView(nullptr),
	fLastStatusChange(system_time()),
	fRunnerFind(nullptr)
{
	font_height fontHeight;
	be_plain_font->GetHeight(&fontHeight);
	float height = ::ceilf(fontHeight.ascent + fontHeight.descent
		+ fontHeight.leading + be_control_look->DefaultItemSpacing());

	fBuildStatusView = new BuildStatusView();
	fLSPStatusView = new LSPStatusView();
	fLastFindStatus = new BStringView("find_status", "");
	fBuildProfileView = new BuildProfileView();

	// TODO: Maybe this is wrong but it works
	SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, height));
	SetExplicitMinSize(BSize(B_SIZE_UNSET, height));

	BLayoutBuilder::Grid<>(this, B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING)
		.Add(fLSPStatusView, 0, 0)
		.AddGlue(1, 0)
		.Add(fLastFindStatus, 2, 0)
		.AddGlue(3, 0)
		.Add(fBuildProfileView, 4, 0)
		.AddGlue(5, 0)
		.Add(fBuildStatusView, 6, 0)
		.End()
		;
}


void
GlobalStatusView::AttachedToWindow()
{
	BView::AttachedToWindow();
	if (Window()->LockLooper()) {
		Window()->StartWatching(this, MSG_NOTIFY_FIND_STATUS);
		Window()->UnlockLooper();
	}
}


/* virtual */
void
GlobalStatusView::DetachedFromWindow()
{
	BView::DetachedFromWindow();
	if (Window()->LockLooper()) {
		Window()->StopWatching(this, MSG_NOTIFY_FIND_STATUS);
		Window()->UnlockLooper();
	}
}


void
GlobalStatusView::Draw(BRect updateRect)
{
	rgb_color baseColor = LowColor();
	BRect bounds = Bounds();
	be_control_look->DrawBorder(this, bounds, updateRect, baseColor, B_FANCY_BORDER,
		0, BControlLook::B_TOP_BORDER);
	BView::Draw(updateRect);
}


void
GlobalStatusView::MessageReceived(BMessage *message)
{
	switch (message->what) {
		case kHideFindText:
			DeleteMessageRunner(&fRunnerFind);
			fLastFindStatus->SetText("");
			break;
		case B_OBSERVER_NOTICE_CHANGE:
		{
			int32 what;
			message->FindInt32(B_OBSERVE_WHAT_CHANGE, &what);
			switch (what) {				
				case MSG_NOTIFY_FIND_STATUS:
				{
					DeleteMessageRunner(&fRunnerFind);
					fLastFindStatus->SetText(message->GetString("status", ""));
					StartMessageRunner(&fRunnerFind, this, kHideFindText, kTextAutohideTimeout);
					break;
				}
				default:
					BView::MessageReceived(message);
					break;
			}
			break;
		}

		default:
			BView::MessageReceived(message);
			break;
	}
}
