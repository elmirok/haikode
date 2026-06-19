/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include <atomic>

namespace Haikode::AI {

class CancellationToken {
public:
	void Cancel()
	{
		fCancelled.store(true);
	}

	bool IsCancelled() const
	{
		return fCancelled.load();
	}

private:
	std::atomic_bool fCancelled { false };
};

} // namespace Haikode::AI
