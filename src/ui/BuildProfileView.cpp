/*
 * Copyright 2026, The Genio team 
 * All rights reserved. Distributed under the terms of the MIT license.
 */


#include "BuildProfileView.h"

#include <Catalog.h>
#include <LayoutBuilder.h>
#include <StatusBar.h>
#include <StringView.h>
#include <Window.h>

#include "NoticeMessages.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "BuildProfileView"

BuildProfileView::BuildProfileView()
	:
	BView("build_status_view", B_WILL_DRAW),
	fProjectString(nullptr),
	fBuildProfileString(nullptr)
{
	fProjectString = new BStringView("Project", "");
	fBuildProfileString = new BStringView("BuildProfile", "");

	BLayoutBuilder::Group<>(this, B_HORIZONTAL)
		.SetInsets(2, 0)
		.Add(fProjectString)
		.Add(fBuildProfileString)
		.End();

	fProjectString->SetExplicitMinSize(BSize(100, B_SIZE_UNSET));
	fProjectString->SetExplicitAlignment(BAlignment(B_ALIGN_LEFT, B_ALIGN_VERTICAL_UNSET));

	fBuildProfileString->SetExplicitMaxSize(BSize(150, B_SIZE_UNSET));
	fBuildProfileString->SetExplicitAlignment(BAlignment(B_ALIGN_LEFT, B_ALIGN_VERTICAL_CENTER));
}


/* virtual */
void
BuildProfileView::AttachedToWindow()
{
	BView::AttachedToWindow();

	if (Window()->LockLooper()) {
		Window()->UnlockLooper();
	}
}


/* virtual */
void
BuildProfileView::DetachedFromWindow()
{
	if (Window()->LockLooper()) {
		Window()->UnlockLooper();
	}
	BView::DetachedFromWindow();
}


/* virtual */
void
BuildProfileView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		default:
			BView::MessageReceived(message);
			break;
	}
}

