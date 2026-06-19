/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include "AIProvider.h"

#include <string>

namespace Haikode::AI {

enum class AIReadinessState {
	Ready,
	NetworkDisabled,
	MissingCredential,
	InvalidProvider
};

struct AIReadinessStatus {
	AIReadinessState state = AIReadinessState::InvalidProvider;
	bool ready = false;
	std::string summary;
	std::string action;
	std::string safeDetails;
};

AIReadinessStatus EvaluateAIReadiness(const ProviderSettings& provider,
	bool networkEnabled);

const char* ToString(AIReadinessState state);

} // namespace Haikode::AI
