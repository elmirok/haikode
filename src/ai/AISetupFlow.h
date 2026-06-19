/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

namespace Haikode::AI {

bool ShouldDeferSetupOpenAfterPanelShow(bool aiPanelHasWindow);
bool ShouldRunProviderTestAfterSetupSave(bool requested, bool requestRunning);
bool ShouldStartOAuthAfterSetupSave(bool requested, bool requestRunning);
bool ShouldExchangeOAuthAfterSetupSave(bool requested, bool requestRunning);

} // namespace Haikode::AI
