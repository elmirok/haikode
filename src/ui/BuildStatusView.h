/*
 * Copyright 2026, The Genio team
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once


#include <View.h>

class BarberPole;
class BMessageRunner;
class BStringView;
class BuildStatusView : public BView {
public:
	BuildStatusView();
	void AttachedToWindow() override;
	void DetachedFromWindow() override;
	void MessageReceived(BMessage* message) override;
	BSize MinSize() override;
private:
	BarberPole*		fBuildBarberPole;
	BStringView*	fBuildStringView;
	BMessageRunner*	fRunnerBuild;
};

