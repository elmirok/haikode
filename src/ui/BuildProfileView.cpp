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
		Window()->StartWatching(this, MSG_NOTIFY_PROJECT_BUILD_PROFILE_CHANGED);
		Window()->StartWatching(this, MSG_NOTIFY_PROJECT_SET_ACTIVE);
		Window()->UnlockLooper();
	}
}


/* virtual */
void
BuildProfileView::DetachedFromWindow()
{
	if (Window()->LockLooper()) {
		Window()->StopWatching(this, MSG_NOTIFY_PROJECT_BUILD_PROFILE_CHANGED);
		Window()->StopWatching(this, MSG_NOTIFY_PROJECT_SET_ACTIVE);
		Window()->UnlockLooper();
	}
	BView::DetachedFromWindow();
}


/* virtual */
void
BuildProfileView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case B_OBSERVER_NOTICE_CHANGE:
		{
			int32 what;
			message->FindInt32(B_OBSERVE_WHAT_CHANGE, &what);
			switch (what) {				
				case MSG_NOTIFY_PROJECT_SET_ACTIVE:
				{
					BString projectString(B_TRANSLATE("Project: \%project\%"));
					BString projectName = message->GetString("active_project_name");
					projectString.ReplaceFirst("\%project\%", projectName);
					fProjectString->SetText(projectString.String());
					break;
				}
				case MSG_NOTIFY_PROJECT_BUILD_PROFILE_CHANGED:
				{
					BString profileName = message->GetString("build_profile_name");
					profileName.Prepend("(");
					profileName.Append(")");
					fBuildProfileString->SetText(profileName.String());
					break;
				}
				default:
					BView::MessageReceived(message);
					break;
			}
		}
		default:
			BView::MessageReceived(message);
			break;
	}
}

