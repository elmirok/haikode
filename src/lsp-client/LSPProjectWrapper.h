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
#include "LSPCompat.h"
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

public:
    void Initialized(json& result);

    void DidOpen(LSPTextDocument* textDocument, std::string_view text, std::string_view languageId);
    void DidClose(LSPTextDocument* textDocument);
    void DidChange(LSPTextDocument* textDocument, std::vector<lsp::TextDocumentContentChangeEvent> &changes,
                   std::optional<bool> wantDiagnostics = {});
    void DidSave(LSPTextDocument* textDocument);
    void RangeFomatting(LSPTextDocument* textDocument, Range range);

    /* not used */ void FoldingRange(LSPTextDocument* textDocument);
    /* not used */ void SelectionRange(LSPTextDocument* textDocument, std::vector<Position> &positions);
    /* not used */ void OnTypeFormatting(LSPTextDocument* textDocument, Position position, std::string_view ch);
    void Formatting(LSPTextDocument* textDocument);
    void CodeAction(LSPTextDocument* textDocument, Range range, lsp::CodeActionContext& context);
	void CodeActionResolve(LSPTextDocument* textDocument, lsp::CodeAction& data);
    void Completion(LSPTextDocument* textDocument, Position position, lsp::CompletionContext& context);
    void SignatureHelp(LSPTextDocument* textDocument, Position position);
    void GoToDefinition(LSPTextDocument* textDocument, Position position);
    void GoToImplementation(LSPTextDocument* textDocument, Position position);
    void GoToDeclaration(LSPTextDocument* textDocument, Position position);
     /* not used */ void References(LSPTextDocument* textDocument, Position position);
    void Rename(LSPTextDocument* textDocument, Position position, std::string_view newName);
    void Hover(LSPTextDocument* textDocument, Position position);
    void DocumentSymbol(LSPTextDocument* textDocument);
    void DocumentLink(LSPTextDocument* textDocument);

    std::string&	allCommitCharacters() { return fAllCommitCharacters; } //not yet used.
    std::string&	triggerCharacters() { return fTriggerCharacters; } //for completion

private:
	bool	_Create();
	void	_RegisterHandlers();
	void	_SendRequest(LSPTextDocument* textDocument, std::string_view method, value params);
	void	_SendNotify(std::string_view method, value params);

	LSPPipeClient*			fLSPPipeClient;
	LSPTextDocument*	_DocumentByURI(const char* uri);
	bool _CheckAndSetCapability(json& capas, const char* str, const LSPCapability flag);

	// Notification/response dispatch helpers (called on UI thread)
	void _OnNotify(std::string method, value& params);
	void _OnResponse(const std::string& documentKey, std::string method, value& result);
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
	void	_SendJsonRequest(std::string_view method, value params, F&& then);
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
LSPProjectWrapper::_SendJsonRequest(std::string_view method, value params, F&& then)
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
