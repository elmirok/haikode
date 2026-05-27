/*
 * Copyright 2026, The Genio team
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once


#include <View.h>

class BMessageRunner;
class BStatusBar;
class BStringView;
class LSPStatusView : public BView {
public:
	LSPStatusView();
	void AttachedToWindow() override;
	void DetachedFromWindow() override;
	void MessageReceived(BMessage* message) override;
	BSize MinSize() override;
private:

	BStringView*	fLSPStringView;
	BStatusBar*		fLSPStatusBar;
};

