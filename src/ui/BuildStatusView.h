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
private:
	void	_ResetRunner(BMessageRunner**  runner);
	void	_StartRunner(BMessageRunner** runner, uint32 what);

	BarberPole*		fBuildBarberPole;
	BStringView*	fBuildStringView;
	BMessageRunner*	fRunnerBuild;
};

