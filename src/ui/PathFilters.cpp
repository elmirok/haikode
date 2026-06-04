/*
 * Copyright 2026, Andrea Anzani
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "PathFilters.h"
#include "Log.h"
#include <sstream>

PathFilters::PathFilters()
{
}

PathFilters::PathFilters(const char* filters)
{
	SetFilters(filters);
}

void
PathFilters::SetFilters(const char* filters)
{
	std::stringstream ss(filters);
	std::string item;
	while (std::getline(ss, item, ',')) {
		if (!item.empty()) {
			fFilters.push_back(item);
			LogTraceF("Using filter [%s]\n", item.c_str());
		}
	}
}

bool
PathFilters::IsFiltered(BString& fullPath)
{
	for (auto& filter : fFilters) {
		if (fullPath.FindFirst(filter.c_str()) != B_ERROR) {
			LogTraceF("Filtering %s", fullPath.String());
			return true;
		}
	}
	return false;
}
