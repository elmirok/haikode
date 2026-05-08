/*
 * Copyright 2018-2026, the Genio team
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#pragma once

#include <File.h>

#include "GMessage.h"
#include "PermanentStorageProvider.h"

class BMessagePSP : public PermanentStorageProvider {
public:
	BMessagePSP();

	status_t Open(const BPath& dest, kPSPMode mode) override;
	status_t Close() override;
	status_t LoadKey(ConfigManager& manager, const char* key, GMessage& storage, GMessage& parameterConfig) override;
	status_t SaveKey(ConfigManager& manager, const char* key, GMessage& storage) override;

private:
	BFile fFile;
	GMessage fFromFile;

	static bool _SameTypeAndFixedSize(BMessage* msgL, const char* keyL, BMessage* msgR, const char* keyR);
};
