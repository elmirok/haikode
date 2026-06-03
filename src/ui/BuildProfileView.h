/*
 * Copyright 2026, The Genio team
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once


#include <Button.h>


class BStringView;
class BuildProfileView : public BButton {
public:
	BuildProfileView();

	void AttachedToWindow() override;
	void DetachedFromWindow() override;
	void MessageReceived(BMessage* message) override;
	void Draw(BRect updateRect) override;

	void MouseMoved(BPoint where, uint32 code,
					const BMessage* dragMessage) override;

private:
	void _UpdateLabel();

	BString fProject;
	BString fBuildProfile;
	bool fInside;
};

