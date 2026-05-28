/*
 * Copyright 2026, Andrea Anzani <andrea.anzani@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once


#include <String.h>
#include <SupportDefs.h>
#include <string>
#include <vector>

class PathFilters {
public:
		PathFilters(const char* filters);
		bool	IsFiltered(BString& fullPath);
private:
		std::vector<std::string>	fFilters;
};


