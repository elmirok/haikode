/*
 * Copyright 2023, Andrea Anzani
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef _H_LSPProjectWrapper
#define _H_LSPProjectWrapper

#include <Locker.h>
#include <Autolock.h>
#include <Path.h>
#include <Locker.h>
#include <atomic>
#include <functional>
#include <map>
#include <mutex>
#include <queue>
#include <string_view>
#include <MessageFilter.h>
#include <Messenger.h>
#include <Url.h>

#include "LSPServersManager.h"
#include "LSPCapabilities.h"
#include <lsp/types.h>
#include "Log.h"
#include "LSPPipeClient.h"

#include <lsp/types.h>
#include "LSPTextDocument.h"

class LSPServerConfigInterface;

using json = lsp::json::Value;

const int32 kLSPWorkProgress = 'lswp';
const int32 kLSPTypedResponse = 'lstr';

class LSPProjectWrapper : public BHandler {

public:
			LSPProjectWrapper(BPath rootPath,
							  const BMessenger& msgr, const LSPServerConfigInterface& serverConfig);

	virtual ~LSPProjectWrapper();

	const LSPServerConfigInterface&	ServerConfig() { return fServerConfig;}

	virtual	void	MessageReceived(BMessage* message);

	bool	RegisterTextDocument(LSPTextDocument* fw);
	void	UnregisterTextDocument(LSPTextDocument* fw);

	bool HasCapability(const LSPCapability flag);


public:
    void Initialize(std::optional<std::string> rootUri = {});
    void Initialized(json& result);

    void DidOpen(LSPTextDocument* textDocument, std::string_view text, std::string_view languageId);
    void DidClose(LSPTextDocument* textDocument);
    void DidChange(LSPTextDocument* textDocument, std::vector<lsp::TextDocumentContentChangeEvent> &changes);
    void DidSave(LSPTextDocument* textDocument);


	void RangeFomatting(LSPTextDocument* textDocument, lsp::Range range);
	void Formatting(LSPTextDocument* textDocument);
    void CodeAction(LSPTextDocument* textDocument, lsp::Range range, lsp::CodeActionContext& context);
    void Completion(LSPTextDocument* textDocument, lsp::Position position, lsp::CompletionContext& context);
    void SignatureHelp(LSPTextDocument* textDocument, lsp::Position position);
    void GoToDefinition(LSPTextDocument* textDocument, lsp::Position position);
    void GoToImplementation(LSPTextDocument* textDocument, lsp::Position position);
    void GoToDeclaration(LSPTextDocument* textDocument, lsp::Position position);
    void Rename(LSPTextDocument* textDocument, lsp::Position position, std::string_view newName);
	void Hover(LSPTextDocument* textDocument, lsp::Position position);
    void DocumentSymbol(LSPTextDocument* textDocument);
    void DocumentLink(LSPTextDocument* textDocument);

	/* not used */ void FoldingRange(LSPTextDocument* textDocument);
    /* not used */ void SelectionRange(LSPTextDocument* textDocument, std::vector<lsp::Position> &positions);
    /* not used */ void OnTypeFormatting(LSPTextDocument* textDocument, lsp::Position position, std::string_view ch);

	void References(LSPTextDocument* textDocument, lsp::Position position);

    std::string&	allCommitCharacters() { return fAllCommitCharacters; } //not yet used.
    std::string&	triggerCharacters() { return fTriggerCharacters; } //for completion

private:
	bool	_Create();
	void	_RegisterHandlers();
	void	_SendNotify(std::string_view method, lsp::json::Value params);

	LSPPipeClient*			fLSPPipeClient;
	LSPTextDocument*	_DocumentByURI(const char* uri);
	bool _CheckAndSetCapability(json& capas, const char* str, const LSPCapability flag);

	// Notification/response dispatch helpers (called on UI thread)
	void _OnNotify(std::string method, lsp::json::Value& params);
	void _DrainResponseQueue();

	void	_LogMessage(lsp::LogMessageParams&& params);

	typedef std::map<std::string, LSPTextDocument*> MapFile;

	MapFile	fTextDocs;

	std::atomic<bool> fInitialized;

	std::string fAllCommitCharacters;
	std::string fTriggerCharacters;

	BUrl fUrl;
	BMessenger fMessenger;
	BMessenger fHandlerMessenger;  // targets this BHandler (for LSP dispatch)
	const LSPServerConfigInterface& fServerConfig;
	uint32	fServerCapabilities;
	BMessage	fWorkDone;

	std::mutex						fResponseQueueLock;
	std::queue<std::function<void()>>	fResponseQueue;

	// magic starts here!
	template<typename M, typename F>
	void	_SendTypedRequest(LSPTextDocument* textDocument, typename M::Params&& params, F&& then);

	template<typename M, typename F>
	auto	_addFunctionToQueue(F&& then, int32 version = -1);

	template<typename F>
	void	_enqueueOnUIThread(F&& fn);

	template<typename F>
	void	_SendJsonRequest(std::string_view method, lsp::json::Value params, F&& then);
};


template<typename F>
void
LSPProjectWrapper::_enqueueOnUIThread(F&& fn)
{
	{
		std::lock_guard<std::mutex> guard(fResponseQueueLock);
		fResponseQueue.push(std::forward<F>(fn));
	}
	fHandlerMessenger.SendMessage(kLSPTypedResponse);
}


template<typename M, typename F>
auto
LSPProjectWrapper::_addFunctionToQueue(F&& then, int32 requestVersion)
{
	return [this, cb = std::forward<F>(then), requestVersion](typename M::Result&& result) mutable {
		_enqueueOnUIThread([cb = std::move(cb), r = std::move(result), requestVersion]() mutable {
			//if (requestVersion > -1)
				cb(std::move(r), requestVersion);
			/*else
				cb(std::move(r));*/
		});
	};
}


template<typename M, typename F>
void
LSPProjectWrapper::_SendTypedRequest(LSPTextDocument* textDocument, typename M::Params&& params, F&& then)
{
	int32 requestVersion = textDocument ? textDocument->Version() : -1;

	fLSPPipeClient->Handler().sendRequest<M>(
		std::move(params),
		_addFunctionToQueue<M>(std::forward<F>(then), requestVersion),
		[](const lsp::ResponseError& error) {
			LogError("LSP request [%s] - error: %s", M::Method.data(), error.message());
		});
}

// Used only for sending the "initialize" request (customized for clangd)
// need to be review.

template<typename F>
void
LSPProjectWrapper::_SendJsonRequest(std::string_view method, lsp::json::Value params, F&& then)
{
	fLSPPipeClient->Handler().sendRequest(
		method,
		std::optional<lsp::json::Value>(std::move(params)),
		[this, cb = std::forward<F>(then)](lsp::json::Value&& result) mutable {
			_enqueueOnUIThread([cb = std::move(cb), r = std::move(result)]() mutable {
				cb(r);
			});
		},
		[method](const lsp::ResponseError& error) {
			LogError("LSP json request [%s] - error: %s", method.data(), error.message());
		});
}


#endif // _H_LSPProjectWrapper
