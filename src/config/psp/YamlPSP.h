/*
 * Copyright 2018-2026, the Genio team
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#pragma once

#include <File.h>
#include <yaml-cpp/yaml.h>

#include "PermanentStorageProvider.h"

class BRect;
class BMessagePSP;

class YamlPSP : public PermanentStorageProvider {
public:
	YamlPSP();

	status_t Open(const BPath& _dest, kPSPMode mode) override;
	status_t Close() override;
	status_t LoadKey(ConfigManager& manager, const char* key, GMessage& storage, GMessage& parameterConfig) override;
	status_t SaveKey(ConfigManager& manager, const char* key, GMessage& storage) override;

private:
	YAML::Node yaml;
	BFile fFile;

    BMessagePSP* fBMsgPSP; //pointer to BMessagePSP for legacy format handling. To be removed in future versions

	status_t _LoadSingleValue(const YAML::Node& node, const char* key, GMessage& storage, type_code expectedType);
	bool _ParseRectFromString(const std::string& rectStr, BRect& rect);
	status_t _LoadMessage(const YAML::Node& node, GMessage& message);
	status_t _LoadMessageValue(const char* key, const YAML::Node& node, GMessage& message);
	status_t _SaveKey(YAML::Node& yaml, const char* key, GMessage& storage, int32 keyIndex);
};
