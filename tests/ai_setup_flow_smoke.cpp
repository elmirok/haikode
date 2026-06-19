/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "ai/AISetupFlow.h"

#include <cassert>
#include <iostream>

int
main()
{
	assert(Haikode::AI::ShouldDeferSetupOpenAfterPanelShow(false));
	assert(!Haikode::AI::ShouldDeferSetupOpenAfterPanelShow(true));
	assert(Haikode::AI::ShouldRunProviderTestAfterSetupSave(true, false));
	assert(!Haikode::AI::ShouldRunProviderTestAfterSetupSave(false, false));
	assert(!Haikode::AI::ShouldRunProviderTestAfterSetupSave(true, true));

	std::cout << "ai-setup-flow-smoke-ok\n";
	return 0;
}
