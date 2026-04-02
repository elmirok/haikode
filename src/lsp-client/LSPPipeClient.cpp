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
#include "LSPReaderThread.h"

#include <lsp/jsonrpc/jsonrpc.h>


enum {
	kReadResult = 'read'
};


LSPPipeClient::LSPPipeClient(uint32 what, BMessenger& msgr)
	:
	BLooper("LSPPipeClient"),
	fWhat(what),
	fMessenger(msgr),
	fReaderThread(nullptr),
	fPipeStream(fPipeImage),
	fConnection(fPipeStream)
{
}


status_t
LSPPipeClient::Start(const char **argv, int32 argc)
{
	status_t image_status = fPipeImage.Init(argv, argc, false, true);
	if (image_status == B_OK)
		LSPPipeClient::Run();
	return image_status;
}


void
LSPPipeClient::Close()
{
	fPipeImage.Close();
}


LSPPipeClient::~LSPPipeClient()
{
	Close();
	ForceQuit();
}


// --- Read path (via lsp::Connection::readMessage) ---


bool
LSPPipeClient::_PostMessage(lsp::jsonrpc::Message&& msg)
{
	auto jsonObj = lsp::jsonrpc::messageToJson(std::move(msg));
	std::string json = lsp::json::stringify(lsp::json::Value(std::move(jsonObj)));
	LogTrace("Client - rcv %d:\n%s\n", (int)json.size(), json.c_str());
	BMessage req(kReadResult);
	req.AddString("data", json.c_str());
	return BLooper::PostMessage(&req) == B_OK;
}


bool
LSPPipeClient::readStep()
{
	try {
		auto message = fConnection.readMessage();
		//single message or batch?
		if (auto* msg = std::get_if<lsp::jsonrpc::Message>(&message)) {
			return _PostMessage(std::move(*msg));
		}
		if (auto* batch = std::get_if<lsp::jsonrpc::MessageBatch>(&message)) {
			for (auto& msg : *batch) {
				if (!_PostMessage(std::move(msg)))
					return false;
			}
			return true;
		}
		return false;
	} catch (const lsp::ConnectionError&) {
		return false;
	} catch (const std::exception& e) {
		LogTrace("LSPPipeClient::readStep error: %s", e.what());
		return false;
	}
}


void
LSPPipeClient::MessageReceived(BMessage* msg)
{
	switch (msg->what) {
		case kReadResult:
		{
			const char* data;
			if (msg->FindString("data", &data) == B_OK) {
				msg->what = fWhat;
				fMessenger.SendMessage(msg);
			}
			break;
		}
		default:
			BLooper::MessageReceived(msg);
			break;
	}
}


// --- Write path (via lsp::Connection) ---


void
LSPPipeClient::notify(std::string_view method, value params)
{
	try {
		auto notification = lsp::jsonrpc::createNotification(method, std::move(params));
		lsp::jsonrpc::Message msg{std::move(notification)};
		fConnection.writeMessage(std::move(msg));
	} catch (const std::exception& e) {
		LogTrace("LSPPipeClient::notify write error: %s", e.what());
	}
}


void
LSPPipeClient::request(std::string_view method, value params, RequestID &id)
{
	try {
		auto req = lsp::jsonrpc::createRequest(
			lsp::jsonrpc::MessageId{lsp::json::String(id)},
			method,
			std::move(params));
		lsp::jsonrpc::Message msg{std::move(req)};
		fConnection.writeMessage(std::move(msg));
	} catch (const std::exception& e) {
		LogTrace("LSPPipeClient::request write error: %s", e.what());
	}
}


// --- Lifecycle ---


pid_t
LSPPipeClient::GetChildPid() const
{
	return fPipeImage.GetChildPid();
}


void
LSPPipeClient::ForceQuit()
{
	if (fReaderThread)
		fReaderThread->Suspend();

	Close();
	PostMessage(B_QUIT_REQUESTED);
}


void
LSPPipeClient::KillThread()
{
	if (fReaderThread)
		fReaderThread->Kill();
}


bool
LSPPipeClient::HasQuitBeenRequested()
{
	return fReaderThread && fReaderThread->HasQuitBeenRequested();
}


thread_id
LSPPipeClient::Run()
{
	fReaderThread = new LSPReaderThread(*this);
	fReaderThread->Start();
	return BLooper::Run();
}


void
LSPPipeClient::Quit()
{
	KillThread();
	return BLooper::Quit();
}
