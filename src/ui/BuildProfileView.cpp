/*
 * Copyright 2026, The Genio team
 * All rights reserved. Distributed under the terms of the MIT license.
 */


#include "BuildProfileView.h"

#include <Alignment.h>
#include <Catalog.h>
#include <ControlLook.h>
#include <LayoutBuilder.h>
#include <StatusBar.h>
#include <StringView.h>
#include <Window.h>

#include "GenioWindowMessages.h"
#include "NoticeMessages.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "BuildProfileView"

BuildProfileView::BuildProfileView()
	:
	BButton("build_status_view", "", new BMessage(MSG_BUILD_MODE_TOGGLE))
{
}


/* virtual */
void
BuildProfileView::AttachedToWindow()
{
	BButton::AttachedToWindow();

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
	BButton::DetachedFromWindow();
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
					fProject = message->GetString("active_project_name");
					_UpdateLabel();
					break;
				}
				case MSG_NOTIFY_PROJECT_BUILD_PROFILE_CHANGED:
				{
					fBuildProfile = message->GetString("build_profile_name");
					fBuildProfile.Prepend("(");
					fBuildProfile.Append(")");
					_UpdateLabel();
					break;
				}
				default:
					BButton::MessageReceived(message);
					break;
			}
		}
		default:
			BButton::MessageReceived(message);
			break;
	}
}


/* virtual */
void
BuildProfileView::Draw(BRect updateRect)
{
	BRect rect = Bounds();
	be_control_look->DrawButtonFrame(this, rect, updateRect, LowColor(), LowColor(), Flags());
	SetHighColor(ui_color(B_CONTROL_TEXT_COLOR));

	font_height fontHeight;
	GetFontHeight(&fontHeight);
	BPoint penLocation = PenLocation();
	penLocation.y += fontHeight.ascent + fontHeight.descent + fontHeight.leading;
	be_control_look->DrawLabel(this, Label(), Bounds(), updateRect, HighColor(), Flags(),
		BAlignment(B_ALIGN_HORIZONTAL_CENTER, B_ALIGN_VERTICAL_CENTER));
}


/* virtual */
void
BuildProfileView::MouseMoved(BPoint where, uint32 code,
					const BMessage* dragMessage)
{
	if (code == B_ENTERED_VIEW)
		fInside = true;
	else if (code == B_EXITED_VIEW)
		fInside = false;
	BButton::MouseMoved(where, code, dragMessage);
	Invalidate();
}


void
BuildProfileView::_UpdateLabel()
{
	BString label;
	if (!fProject.IsEmpty()) {
		label.Append(fProject.String());
		label.Append(" ");
		label.Append(fBuildProfile.String());
	}
	SetLabel(label.String());
}
