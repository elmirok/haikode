/*
 * Copyright 2023-2026, Andrea Anzani
 *
 * Source code derived from AGMSScriptOCron
 * 	Copyright (c) 2018 by Alexander G. M. Smith.
 *
 * All rights reserved. Distributed under the terms of the MIT license.
 */


#include "LSPPipeClient.h"

#include "Log.h"


LSPPipeClient::LSPPipeClient()
	:
	fPipeStream(fPipeImage),
	fConnection(fPipeStream),
	fHandler(fConnection, 0),
	fRunning(false)
{
}

status_t
LSPPipeClient::Start(const char **argv, int32 argc)
{
	status_t image_status = fPipeImage.Init(argv, argc, false, true);
	if (image_status == B_OK) {
		fRunning.store(true);
		fReaderThread = std::thread(&LSPPipeClient::_ReaderLoop, this);
	} else {
		LogErrorF("%s", strerror(image_status));
	}
	return image_status;
}


void
LSPPipeClient::Close()
{
	fRunning.store(false);
	fPipeImage.Close();
}


LSPPipeClient::~LSPPipeClient()
{
	Close();
	//sleep(1);
	if (fReaderThread.joinable())
		fReaderThread.join();
}


// --- Reader loop (replaces LSPReaderThread + BLooper message forwarding) ---


void
LSPPipeClient::_ReaderLoop()
{
	try {
		while (fRunning.load()) {
			fHandler.processIncomingMessages();
		}
	} catch (const lsp::ConnectionError& e) {
		LogError("LSPPipeClient reader ConnectionError: %s", e.what());
	} catch (const lsp::io::Error& e) {
		LogError("LSPPipeClient reader io::Error: %s", e.what());
	} catch (const std::exception& e) {
		LogError("LSPPipeClient reader exception: %s", e.what());
	}
	fRunning.store(false);
}


// --- Lifecycle ---


pid_t
LSPPipeClient::GetChildPid() const
{
	return fPipeImage.GetChildPid();
}
