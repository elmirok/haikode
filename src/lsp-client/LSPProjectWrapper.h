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
#include <map>
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

class LSPTextDocument;
class LSPServerConfigInterface;

using json = lsp::json::Value;

const int32 kLSPWorkProgress = 'lswp';

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
    void Shutdown();
    void Sync();
    void Exit();
    void Initialized(json& result);

    void DidOpen(LSPTextDocument* textDocument, std::string_view text, std::string_view languageId);
    void DidClose(LSPTextDocument* textDocument);
    void DidChange(LSPTextDocument* textDocument, std::vector<lsp::TextDocumentContentChangeEvent> &changes,
                   std::optional<bool> wantDiagnostics = {});
    void DidSave(LSPTextDocument* textDocument);
    void RangeFomatting(LSPTextDocument* textDocument, Range range);
    void FoldingRange(LSPTextDocument* textDocument);
    void SelectionRange(LSPTextDocument* textDocument, std::vector<Position> &positions);
    void OnTypeFormatting(LSPTextDocument* textDocument, Position position, std::string_view ch);
    void Formatting(LSPTextDocument* textDocument);
    void CodeAction(LSPTextDocument* textDocument, Range range, lsp::CodeActionContext& context);
	void CodeActionResolve(LSPTextDocument* textDocument, lsp::CodeAction& data);
	void CodeActionResolve(LSPTextDocument* textDocument, value& data);
    void Completion(LSPTextDocument* textDocument, Position position, lsp::CompletionContext& context);
    void SignatureHelp(LSPTextDocument* textDocument, Position position);
    void GoToDefinition(LSPTextDocument* textDocument, Position position);
    void GoToImplementation(LSPTextDocument* textDocument, Position position);
    void GoToDeclaration(LSPTextDocument* textDocument, Position position);
    void References(LSPTextDocument* textDocument, Position position);
    void SwitchSourceHeader(LSPTextDocument* textDocument);
    void Rename(LSPTextDocument* textDocument, Position position, std::string_view newName);
    void Hover(LSPTextDocument* textDocument, Position position);
    void DocumentSymbol(LSPTextDocument* textDocument);
    void DocumentColor(LSPTextDocument* textDocument);
    void DocumentHighlight(LSPTextDocument* textDocument, Position position);
    void SymbolInfo(LSPTextDocument* textDocument, Position position);
    void DocumentLink(LSPTextDocument* textDocument);

    std::string&	allCommitCharacters() { return fAllCommitCharacters; } //not yet used.
    std::string&	triggerCharacters() { return fTriggerCharacters; } //for completion

private:
	bool	_Create();
	void	_RegisterHandlers();
	void	_SendRequest(LSPTextDocument* textDocument, std::string_view method, value params);
	void	_SendNotify(std::string_view method, value params);

	template<typename M, typename F>
	void	_SendTypedRequest(typename M::Params&& params, F&& then);

	LSPPipeClient*			fLSPPipeClient;
	LSPTextDocument*	_DocumentByURI(const char* uri);
	bool _CheckAndSetCapability(json& capas, const char* str, const LSPCapability flag);

	// Notification/response dispatch helpers (called on UI thread)
	void _OnNotify(std::string method, value& params);
	void _OnResponse(const std::string& documentKey, std::string method, value& result);
	void _OnError(const std::string& documentKey, std::string method, value& error);
	void _OnRequest(std::string method, value& params, value& id);

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
};


template<typename M, typename F>
void
LSPProjectWrapper::_SendTypedRequest(typename M::Params&& params, F&& then)
{
	fLSPPipeClient->Handler().sendRequest<M>(
		std::move(params),
		[this, cb = std::forward<F>(then)](typename M::Result&& result) {
			BAutolock lock(this->Looper());
			cb(std::move(result));
		},
		[](const lsp::ResponseError& error) {
			LogError("LSP request error: %s", error.message());
		});
}


#endif // _H_LSPProjectWrapper
