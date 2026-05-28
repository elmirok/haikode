/*
 * Copyright 2026, Andrea Anzani
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#pragma once

#include <StringItem.h>

class FilterListItem : public BStringItem {
public:
	FilterListItem(const entry_ref& ref, const BString& relativePath)
		:
		BStringItem(ref.name),
		fRef(ref),
		fRelativePath(relativePath)
	{
	}

	const entry_ref& Ref() const { return fRef; }
	const BString& RelativePath() const { return fRelativePath; }

	void DrawItem(BView* owner, BRect frame, bool complete) override
	{
		if (IsSelected() || complete) {
			rgb_color color;
			if (IsSelected())
				color = ui_color(B_LIST_SELECTED_BACKGROUND_COLOR);
			else
				color = owner->ViewColor();
			owner->SetLowColor(color);
			owner->FillRect(frame, B_SOLID_LOW);
		}

		rgb_color textColor;
		if (IsSelected())
			textColor = ui_color(B_LIST_SELECTED_ITEM_TEXT_COLOR);
		else
			textColor = ui_color(B_LIST_ITEM_TEXT_COLOR);

		BFont font;
		owner->GetFont(&font);
		font_height fontHeight;
		font.GetHeight(&fontHeight);

		float lineHeight = fontHeight.ascent + fontHeight.descent + fontHeight.leading;
		float x = frame.left + be_control_look->DefaultLabelSpacing();

		owner->SetHighColor(textColor);
		owner->SetDrawingMode(B_OP_COPY);
		owner->MovePenTo(x, frame.top + fontHeight.ascent + 1.0f);
		owner->DrawString(Text());

		BFont smallFont(font);
		smallFont.SetSize(font.Size() * 0.85f);
		owner->SetFont(&smallFont);
		owner->SetHighColor(tint_color(textColor, B_LIGHTEN_1_TINT));
		owner->MovePenTo(x, frame.top + lineHeight + fontHeight.ascent + 1.0f);
		owner->DrawString(fRelativePath.String());

		owner->SetFont(&font);
	}

	void Update(BView* owner, const BFont* font) override
	{
		BStringItem::Update(owner, font);
		font_height fontHeight;
		font->GetHeight(&fontHeight);
		float lineHeight = fontHeight.ascent + fontHeight.descent + fontHeight.leading;
		SetHeight(lineHeight * 2.0f + 4.0f);
	}

private:
	entry_ref	fRef;
	BString		fRelativePath;
};

