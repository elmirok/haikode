/*
 * Copyright 2023, Andrea Anzani
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef GenioWatchingFilter_H
#define GenioWatchingFilter_H


#include <Directory.h>
#include <Path.h>
#include <PathMonitor.h>
#include <cstdio>
#include <string>
#include <vector>
#include <sstream>
#include "Log.h"

// This is attached to the PathMonitor class to avoid watching too many (useless) files.
// In the ProjectFolderBrowser we care only on directories and related files events.
// We can't use the B_WATCH_DIRECTORIES_ONLY flag as PathMonitor won't notify for file related events.

class GenioWatchingFilter : public BPrivate::BPathMonitor::BWatchingInterface {
public:
			GenioWatchingFilter(const char* watch_nodes_filters) {
				std::stringstream ss(watch_nodes_filters);
				std::string item;
				while (std::getline(ss, item, ',')) {
					if (!item.empty()) {
						fFilters.push_back(item);
						LogTraceF("Using filter [%s]\n", item.c_str());
					}
				}
			}

	status_t WatchNode(const node_ref* node, uint32 flags, const BHandler* handler,
							  		const BLooper* looper = NULL)

	{
		status_t status;
		BDirectory dir(node);
		if ((status = dir.InitCheck()) != B_OK) {
			// Typically the reason for this failure is "Not a directory".
			// As we want to avoid to use a watch_node for every standard file,
			// we quit here.
			return B_OK;
		}

		BEntry entry;
		dir.GetEntry(&entry);
		BPath path;
		entry.GetPath(&path);

		BString fullPath = path.Path();

		//Let's filter useless directories.
		for(auto& filter : fFilters) {
			if (fullPath.FindFirst(filter.c_str()) != B_ERROR) {
				LogTraceF("Filtering %s", fullPath.String());
				return B_OK;
			}
		}

		status = BPrivate::BPathMonitor::BWatchingInterface::WatchNode(node, flags, handler, looper);

		if (status != B_OK && flags != B_STOP_WATCHING) {

			LogErrorF("Can't watch_node for directory [%s](%d) (%s) handler (%p) looper (%p) %d",
						fullPath.String(),
						node->node,
						strerror(status),
						handler,
						looper,
						status == B_BAD_VALUE);
			//TODO: maybe we should notify the user that the PathMonitor is not working?
		} else {
			fWatchedNodes++;
		}
		return status;
	}

	uint32	WatchedNodesCount() const { return fWatchedNodes; }

private:

		uint32	fWatchedNodes = 0L;
		std::vector<std::string>	fFilters;
};

#endif // GenioWatchingFilter_H
