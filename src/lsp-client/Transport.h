//
// Code and inspiration taken from https://github.com/microsoft/language-server-protocol
// Copyright 2023, Andrea Anzani 
//

#ifndef LSP_TRANSPORT_H
#define LSP_TRANSPORT_H

#include "MessageHandler.h"

#include <Looper.h>
#include <Messenger.h>

#include <string_view>

class Transport {
public:
    virtual void notify(std::string_view method,  value &params) = 0;
    virtual void request(std::string_view method, value &params, RequestID &id) = 0;

    virtual bool  readStep() = 0;

    virtual bool readMessage(std::string &) = 0;
    virtual bool writeMessage(std::string &) = 0;
};

class AsyncJsonTransport: public Transport, public BLooper {

public:
		 AsyncJsonTransport(uint32 handler, BMessenger& msgr);

    void notify(std::string_view method, value &params) override;
    void request(std::string_view method, value &params, RequestID &id) override;

	bool readStep() override;

	void MessageReceived(BMessage* msg) override;

private:

	bool writeJson(value& value);

	uint32			fWhat;
	BMessenger		fMessenger;
};

#endif //LSP_TRANSPORT_H
