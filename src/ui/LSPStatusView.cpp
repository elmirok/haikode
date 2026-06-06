/*
 * Copyright 2026, The Genio team
 * All rights reserved. Distributed under the terms of the MIT license.
 */


#include "LSPStatusView.h"

#include <Catalog.h>
#include <LayoutBuilder.h>
#include <StatusBar.h>
#include <StringView.h>
#include <Window.h>

#include "NoticeMessages.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "LSPStatusView"

LSPStatusView::LSPStatusView()
	:
	BView("build_status_view", B_WILL_DRAW),
	fLSPStringView(nullptr),
	fLSPStatusBar(nullptr)
{
	fLSPStringView = new BStringView("LSP_text", "");
	fLSPStatusBar = new BStatusBar("LSP_progressbar");

	BLayoutBuilder::Group<>(this, B_HORIZONTAL)
		.SetInsets(0, 2)
		.Add(fLSPStringView)
		.Add(fLSPStatusBar)
		.End();

	fLSPStatusBar->Hide();

	fLSPStringView->SetExplicitMinSize(BSize(100, B_SIZE_UNSET));
	fLSPStringView->SetExplicitAlignment(BAlignment(B_ALIGN_RIGHT, B_ALIGN_VERTICAL_UNSET));

	fLSPStatusBar->SetExplicitMaxSize(BSize(150, B_SIZE_UNSET));
	fLSPStatusBar->SetExplicitAlignment(BAlignment(B_ALIGN_LEFT, B_ALIGN_VERTICAL_CENTER));
}


/* virtual */
void
LSPStatusView::AttachedToWindow()
{
	BView::AttachedToWindow();

	if (Window()->LockLooper()) {
		Window()->StartWatching(this, MSG_NOTIFY_LSP_INDEXING);
		Window()->UnlockLooper();
	}
}


/* virtual */
void
LSPStatusView::DetachedFromWindow()
{
	if (Window()->LockLooper()) {
		Window()->StopWatching(this, MSG_NOTIFY_LSP_INDEXING);
		Window()->UnlockLooper();
	}
	BView::DetachedFromWindow();
}


/* virtual */
void
LSPStatusView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case B_OBSERVER_NOTICE_CHANGE:
		{
			int32 what;
			message->FindInt32(B_OBSERVE_WHAT_CHANGE, &what);
			switch (what) {
				case MSG_NOTIFY_LSP_INDEXING:
				{
					BString kind = message->GetString("kind", "end");
					if (kind.Compare("end") == 0) {
						fLSPStringView->SetText("");
						if (!fLSPStatusBar->IsHidden())
							fLSPStatusBar->Hide();
						return;
					}

					// TODO: translate ?
					BString text;
					const char* str = nullptr;
					if (message->FindString("title", &str) == B_OK) {
						text << str << " ";
					}
					if (message->FindString("message", &str) == B_OK) {
						text << str << " ";
					}
					int32 percentage = 0;
					if (message->FindInt32("percentage", &percentage) == B_OK) {
						text << "(" << percentage << "%)";
						if (fLSPStatusBar->IsHidden())
							fLSPStatusBar->Show();

						fLSPStatusBar->Update(percentage - fLSPStatusBar->CurrentValue());
					}

					fLSPStringView->SetText(text.String());
					break;
				}
				default:
					BView::MessageReceived(message);
					break;
			}
		default:
			BView::MessageReceived(message);
			break;
		}
	}
}


/* virtual */
BSize
LSPStatusView::MinSize()
{
	return BSize(300, B_SIZE_UNSET);
}
