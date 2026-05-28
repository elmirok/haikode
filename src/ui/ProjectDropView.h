/*
 * Copyright 2026, Andrea Anzani
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#pragma once

#include <Catalog.h>
#include <LayoutBuilder.h>
#include <StringView.h>
#include <View.h>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "ProjectsFolderBrowser"

const pattern kStripePattern = {
	0xcc, 0x66, 0x33, 0x99,
	0xcc, 0x66, 0x33, 0x99
};


class ProjectDropView : public BView {
public:
	ProjectDropView()
		:
		BView("ProjectDropView", B_WILL_DRAW|B_FRAME_EVENTS|B_FULL_UPDATE_ON_RESIZE)
	{
		BString dropLabel = B_TRANSLATE("Drop folder here");
		BStringView* stringView = new BStringView("drop", dropLabel.String());
		BLayoutBuilder::Group<>(this, B_VERTICAL, 0)
			.AddGroup(B_VERTICAL, 3)
				.AddGlue()
				.AddStrut(1)
				.Add(stringView)
				.AddGlue()
			.End();

		stringView->SetAlignment(B_ALIGN_CENTER);

		BFont font;
		font.SetFace(B_CONDENSED_FACE);
		stringView->SetFont(&font, B_FONT_FACE);

		// TODO: These should not be needed, but without them the
		// splitview which separates editor from the left pane doesn't move at all
		SetExplicitMinSize(BSize(0, B_SIZE_UNSET));
		SetExplicitMaxSize(BSize(B_SIZE_UNLIMITED, B_SIZE_UNSET));
	}

	void Draw(BRect updateRect) override
	{
		SetDrawingMode(B_OP_ALPHA);
		SetLowColor(0, 0, 0);
		float tint = B_DARKEN_2_TINT;
		const int32 kBrightnessBreakValue = 126;
		const rgb_color base = ui_color(B_PANEL_BACKGROUND_COLOR);
		if (base.Brightness() >= kBrightnessBreakValue)
			tint = B_LIGHTEN_2_TINT;

		SetHighColor(tint_color(ui_color(B_PANEL_BACKGROUND_COLOR),	tint));
		BRect innerRect = Bounds().InsetByCopy(10, 10);
		FillRect(innerRect, B_SOLID_LOW);
		StrokeRect(innerRect);
		FillRect(innerRect.InsetBySelf(3, 3), kStripePattern);
	}
};
