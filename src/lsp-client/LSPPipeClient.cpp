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


// --- Read path (hand-rolled Content-Length framing, unchanged) ---


bool
LSPPipeClient::ReadHeaderLine(char* header, size_t maxlen)
{
	ssize_t hasRead = 0;
	size_t length = 0;
	while ((hasRead = fPipeImage.Read(&header[length], 1)) != -1) {
		if (hasRead == 0 || length >= maxlen - 1) // pipe eof or protection
			return false;

		if (header[length] == '\n') {
			break;
		}
		length++;
	}
	return true;
}


int
LSPPipeClient::ReadMessageHeader()
{
	char szReadBuffer[255];
	int len = 0;
	while (ReadHeaderLine(szReadBuffer, 255)) {
		if (::strncmp(szReadBuffer, "Content-Length: ", 16) == 0) {
			len = ::strtol(szReadBuffer + 16, nullptr, 10);
		} else if (::strncmp(szReadBuffer, "\r\n", 2) == 0) {
			break;
		} else {
			LogTrace("Unsuported LSP message header: %s", szReadBuffer);
		}
	}
	return len;
}


int
LSPPipeClient::Read(int length, std::string &out)
{
	int readSize = 0;
	ssize_t hasRead;
	out.resize(length);
	while ((hasRead = fPipeImage.Read(&out[readSize], length)) != -1) {
		if (hasRead == 0) // pipe eof
			return 0;

		readSize += hasRead;
		if (readSize >= length) {
			break;
		}
	}

	return readSize;
}


bool
LSPPipeClient::readMessage(std::string &json)
{
	json.clear();
	int length = ReadMessageHeader();
	if (length == 0)
		return false;
	if (Read(length, json) == 0)
		return false;
	LogTrace("Client - rcv %d:\n%s\n", length, json.c_str());
	return true;
}


bool
LSPPipeClient::readStep()
{
	std::string data;
	bool result = readMessage(data);
	if (result) {
		BMessage req(kReadResult);
		req.AddString("data", data.c_str());
		return BLooper::PostMessage(&req) == B_OK;
	}
	return result;
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
