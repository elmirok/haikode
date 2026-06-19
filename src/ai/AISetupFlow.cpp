/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "AISetupFlow.h"

namespace Haikode::AI {

bool
ShouldDeferSetupOpenAfterPanelShow(bool aiPanelHasWindow)
{
	return !aiPanelHasWindow;
}


bool
ShouldRunProviderTestAfterSetupSave(bool requested, bool requestRunning)
{
	return requested && !requestRunning;
}


bool
ShouldStartOAuthAfterSetupSave(bool requested, bool requestRunning)
{
	return requested && !requestRunning;
}


bool
ShouldExchangeOAuthAfterSetupSave(bool requested, bool requestRunning)
{
	return requested && !requestRunning;
}

} // namespace Haikode::AI
