/*
 * Copyright 2018-2026, the Genio team
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#pragma once

#include <Node.h>

#include "PermanentStorageProvider.h"

class AttributePSP : public PermanentStorageProvider {
public:
	AttributePSP();

	status_t Open(const BPath& attributeFilePath, kPSPMode mode) override;
	status_t Close() override;
	status_t LoadKey(ConfigManager& manager, const char* key, GMessage& storage, GMessage& parameterConfig) override;
	status_t SaveKey(ConfigManager& manager, const char* key, GMessage& storage) override;

private:
	BNode fNodeAttr;
};
