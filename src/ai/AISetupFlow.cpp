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

} // namespace Haikode::AI
