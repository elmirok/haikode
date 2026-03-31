/*
 * Copyright 2023, Andrea Anzani
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#ifndef _H_LSPProjectWrapper
#define _H_LSPProjectWrapper

#include <Locker.h>
#include <Path.h>
#include <Locker.h>
#include <atomic>
#include <string_view>
#include <MessageFilter.h>
#include <Messenger.h>
#include <Url.h>

#include "LSPServersManager.h"
#include "MessageHandler.h"
#include "LSPCapabilities.h"
#include "protocol_objects.h"

#include <lsp/types.h>

class  LSPTextDocument;
struct ConfigurationSettings;
class LSPPipeClient;
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

    void onNotify(std::string method, value &params);
    void onResponse(RequestID ID, value &result);
    void onError(RequestID ID, value &error);
    void onRequest(std::string method, value &params, value &ID);


	bool HasCapability(const LSPCapability flag);


public:
    RequestID Initialize(std::optional<std::string> rootUri = {});
    RequestID Shutdown();
    RequestID Sync();
    void Exit();
    void Initialized(json& result);

    RequestID RegisterCapability();
    void DidOpen(LSPTextDocument* textDocument, std::string_view text, std::string_view languageId);
    void DidClose(LSPTextDocument* textDocument);
    void DidChange(LSPTextDocument* textDocument, std::vector<lsp::TextDocumentContentChangeEvent> &changes,
                   std::optional<bool> wantDiagnostics = {});
    void DidSave(LSPTextDocument* textDocument);
    RequestID RangeFomatting(LSPTextDocument* textDocument, Range range);
    RequestID FoldingRange(LSPTextDocument* textDocument);
    RequestID SelectionRange(LSPTextDocument* textDocument, std::vector<Position> &positions);
    RequestID OnTypeFormatting(LSPTextDocument* textDocument, Position position, std::string_view ch);
    RequestID Formatting(LSPTextDocument* textDocument);
    RequestID CodeAction(LSPTextDocument* textDocument, Range range, lsp::CodeActionContext& context);
	RequestID CodeActionResolve(LSPTextDocument* textDocument, lsp::CodeAction& data);
	RequestID CodeActionResolve(LSPTextDocument* textDocument, value& data);
    RequestID Completion(LSPTextDocument* textDocument, Position position, lsp::CompletionContext& context);
    RequestID SignatureHelp(LSPTextDocument* textDocument, Position position);
    RequestID GoToDefinition(LSPTextDocument* textDocument, Position position);
    RequestID GoToImplementation(LSPTextDocument* textDocument, Position position);
    RequestID GoToDeclaration(LSPTextDocument* textDocument, Position position);
    RequestID References(LSPTextDocument* textDocument, Position position);
    RequestID SwitchSourceHeader(LSPTextDocument* textDocument);
    RequestID Rename(LSPTextDocument* textDocument, Position position, std::string_view newName);
    RequestID Hover(LSPTextDocument* textDocument, Position position);
    RequestID DocumentSymbol(LSPTextDocument* textDocument);
    RequestID DocumentColor(LSPTextDocument* textDocument);
    RequestID DocumentHighlight(LSPTextDocument* textDocument, Position position);
    RequestID SymbolInfo(LSPTextDocument* textDocument, Position position);
    RequestID DocumentLink(LSPTextDocument* textDocument);

    RequestID 	SendRequest(RequestID id, std::string_view method, value params);
    void 		SendNotify(std::string_view method, value params);

    std::string&	allCommitCharacters() { return fAllCommitCharacters; } //not yet used.
    std::string&	triggerCharacters() { return fTriggerCharacters; } //for completion

private:
	bool	_Create();
	LSPPipeClient*			fLSPPipeClient;
	LSPTextDocument*	_DocumentByURI(const char* uri);
	bool _CheckAndSetCapability(json& capas, const char* str, const LSPCapability flag);

	typedef std::map<std::string, LSPTextDocument*> MapFile;

	MapFile	fTextDocs;

	std::atomic<bool> fInitialized;

	std::string fAllCommitCharacters;
	std::string fTriggerCharacters;

	BUrl fUrl;
	BMessenger fMessenger;
	const LSPServerConfigInterface& fServerConfig;
	uint32	fServerCapabilities;
	BMessage	fWorkDone;
};

#endif // _H_LSPProjectWrapper
