/*
 * Copyright 2026, The Genio team
 * All rights reserved. Distributed under the terms of the MIT license.
 */


#include "BuildStatusView.h"

#include <BarberPole.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
#include <MessageRunner.h>
#include <StringView.h>
#include <Window.h>

#include "NoticeMessages.h"
#include "Utils.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "BuildStatusView"

const bigtime_t kTextAutohideTimeout = 5000000ULL;
const uint32 kHideBuildingText = 'HIDE';

BuildStatusView::BuildStatusView()
	:
	BView("build_status_view", B_WILL_DRAW),
	fBuildBarberPole(nullptr),
	fBuildStringView(nullptr),
	fRunnerBuild(nullptr)
{
	fBuildBarberPole = new BarberPole("build_barberpole");
	fBuildStringView = new BStringView("build_text", "");

	BLayoutBuilder::Group<>(this, B_HORIZONTAL)
		.SetInsets(2, 2)
		.Add(fBuildStringView)
		.Add(fBuildBarberPole)
		.End();

	fBuildBarberPole->Hide();

	fBuildStringView->SetExplicitMinSize(BSize(200, B_SIZE_UNSET));
	fBuildStringView->SetExplicitAlignment(BAlignment(B_ALIGN_RIGHT, B_ALIGN_VERTICAL_UNSET));

	fBuildBarberPole->SetExplicitMaxSize(BSize(250, B_SIZE_UNSET));
	fBuildBarberPole->SetExplicitAlignment(BAlignment(B_ALIGN_LEFT, B_ALIGN_VERTICAL_CENTER));
}


/* virtual */
void
BuildStatusView::AttachedToWindow()
{
	BView::AttachedToWindow();

	if (Window()->LockLooper()) {
		Window()->StartWatching(this, MSG_NOTIFY_BUILDING_PHASE);
		Window()->UnlockLooper();
	}
}


/* virtual */
void
BuildStatusView::DetachedFromWindow()
{
	if (Window()->LockLooper()) {
		Window()->StopWatching(this, MSG_NOTIFY_BUILDING_PHASE);
		Window()->UnlockLooper();
	}
	BView::DetachedFromWindow();
}


/* virtual */
void
BuildStatusView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kHideBuildingText:
			DeleteMessageRunner(&fRunnerBuild);
			fBuildStringView->SetText("");
			fBuildBarberPole->Hide();
			break;
		case B_OBSERVER_NOTICE_CHANGE:
		{
			int32 what;
			message->FindInt32(B_OBSERVE_WHAT_CHANGE, &what);
			switch (what) {
				case MSG_NOTIFY_BUILDING_PHASE:
				{
					DeleteMessageRunner(&fRunnerBuild);

					if (fBuildBarberPole->IsHidden())
						fBuildBarberPole->Show();

					// TODO: Instead of doing this here, put the string into the message
					// from the caller and just retrieve it and display it here
					bool building = message->GetBool("building", false);
					BString projectName = message->GetString("project_name");
					BString cmdType = message->GetString("cmd_type");
					status_t status = message->GetInt32("status", B_OK);
					BString text;
					if (building) {
						if (cmdType.Compare("build") == 0)
							text = B_TRANSLATE("Building project '\"%project%\"'" B_UTF8_ELLIPSIS);
						else if (cmdType.Compare("clean") == 0)
							text = B_TRANSLATE("Cleaning project '\"%project%\"'" B_UTF8_ELLIPSIS);
						fBuildBarberPole->Start();
					} else {
						if (cmdType.Compare("build") == 0) {
							if (status == B_OK)
								text = B_TRANSLATE("Finished building project '\"%project%\"'");
							else
								text = B_TRANSLATE("Failed building project '\"%project%\"'");
						} else if (cmdType.Compare("clean") == 0) {
							if (status == B_OK)
								text = B_TRANSLATE("Finished cleaning project '\"%project%\"'");
							else
								text = B_TRANSLATE("Failed cleaning project '\"%project%\"'");
						}
						fBuildBarberPole->Stop();
						StartMessageRunner(&fRunnerBuild, this, kHideBuildingText, kTextAutohideTimeout);
					}
					text.ReplaceFirst("\"%project%\"", projectName);
					fBuildStringView->SetText(text.String());

					if (status != B_OK) {
						// On fail
						fBuildStringView->SetHighColor(ui_color(B_FAILURE_COLOR));
						// beep();
					} else
						fBuildStringView->SetHighColor(ui_color(B_CONTROL_TEXT_COLOR));
					break;
				}
			}
		default:
			BView::MessageReceived(message);
			break;
		}
	}
}


/* virtual */
BSize
BuildStatusView::MinSize()
{
	return BSize(300, B_SIZE_UNSET);
}
