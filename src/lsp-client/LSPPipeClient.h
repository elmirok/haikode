/*
 * Copyright 2023-2026, Andrea Anzani
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#pragma once

#include <atomic>
#include <thread>

#include "PipeImage.h"
#include "PipeStream.h"

#include <lsp/connection.h>
#include <lsp/messagehandler.h>

class LSPPipeClient {

public:

			 LSPPipeClient();
	virtual ~LSPPipeClient();

	status_t Start(const char **argv, int32 argc);

	void	Close();

	lsp::MessageHandler& Handler() { return fHandler; }

	pid_t	GetChildPid() const;
	bool	IsRunning() const { return fRunning.load(); }

private:
  void	_ReaderLoop();

  PipeImage				fPipeImage;
  PipeStream			fPipeStream;
  lsp::Connection		fConnection;
  lsp::MessageHandler	fHandler;
  std::thread			fReaderThread;
  std::atomic<bool>		fRunning;
};
