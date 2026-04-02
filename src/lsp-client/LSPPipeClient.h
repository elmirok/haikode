/*
 * Copyright 2023-2026, Andrea Anzani
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#pragma once

#include <unistd.h>

#include "PipeImage.h"
#include "PipeStream.h"
#include "MessageHandler.h"

#include <Looper.h>
#include <Messenger.h>

#include <lsp/connection.h>

class LSPReaderThread;

class LSPPipeClient : public BLooper {

public:

			 LSPPipeClient(uint32 what, BMessenger& msgr);
	virtual ~LSPPipeClient();

	status_t Start(const char **argv, int32 argc);

	void	Close();

	void	notify(std::string_view method, value params);
	void	request(std::string_view method, value params, RequestID &id);

	bool	readStep();

	pid_t	GetChildPid() const;

	void	ForceQuit(); //quite the looper and the kill the thread
	bool	HasQuitBeenRequested();
	void	KillThread();

private:
  bool	_PostMessage(lsp::jsonrpc::Message&& msg);
  void	MessageReceived(BMessage* msg) override;
  void	Quit() override;
  thread_id	Run() override;

  uint32			fWhat;
  BMessenger		fMessenger;
  LSPReaderThread*	fReaderThread;
  PipeImage			fPipeImage;
  PipeStream		fPipeStream;
  lsp::Connection	fConnection;
};
