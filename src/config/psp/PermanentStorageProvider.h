/*
 * Copyright 2018-2026, the Genio team
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#pragma once

#include <SupportDefs.h>

class BPath;
class GMessage;
class ConfigManager;

class PermanentStorageProvider {
public:
	enum kPSPMode { kPSPReadMode, kPSPWriteMode };

						PermanentStorageProvider() {};
						virtual ~PermanentStorageProvider() {};

	virtual status_t	Open(const BPath& destination, kPSPMode mode) = 0;
	virtual status_t	Close() = 0;
	virtual status_t	LoadKey(ConfigManager& manager, const char* key, GMessage& storage, GMessage& parConfig) = 0;
	virtual status_t	SaveKey(ConfigManager& manager, const char* key, GMessage& storage) = 0;
};
