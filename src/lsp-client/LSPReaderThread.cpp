/*
 * Copyright 2023-2026, Andrea Anzani 
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "LSPReaderThread.h"

#include "LSPPipeClient.h"

LSPReaderThread::LSPReaderThread(LSPPipeClient& looper)
	:
	GenericThread("LSPReaderThread", B_NORMAL_PRIORITY),
	fTransport(looper)
{
	 // TODO: Maybe experiment with other priorities
}


status_t
LSPReaderThread::ExecuteUnit()
{
	if (!fTransport.readStep()) {
		Quit();
		fTransport.Close();  //TODO: this should be performed by the Client itself not from the thread!
		return B_ERROR;
	}
	return B_OK;
}
