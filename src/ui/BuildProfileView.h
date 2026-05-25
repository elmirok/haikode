/*
 * Copyright 2026, The Genio team 
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once


#include <View.h>


class BStringView;
class BuildProfileView : public BView {
public:
	BuildProfileView();
	void AttachedToWindow() override;
	void DetachedFromWindow() override;
	void MessageReceived(BMessage* message) override;
private:
	BStringView*	fProjectString;
	BStringView*	fBuildProfileString;

};

