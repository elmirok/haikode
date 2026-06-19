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
	assert(Haikode::AI::ShouldStartOAuthAfterSetupSave(true, false));
	assert(!Haikode::AI::ShouldStartOAuthAfterSetupSave(false, false));
	assert(!Haikode::AI::ShouldStartOAuthAfterSetupSave(true, true));
	assert(Haikode::AI::ShouldExchangeOAuthAfterSetupSave(true, false));
	assert(!Haikode::AI::ShouldExchangeOAuthAfterSetupSave(false, false));
	assert(!Haikode::AI::ShouldExchangeOAuthAfterSetupSave(true, true));

	std::cout << "ai-setup-flow-smoke-ok\n";
	return 0;
}
