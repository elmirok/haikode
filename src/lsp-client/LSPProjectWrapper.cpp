/*
 * Copyright 2023, Andrea Anzani
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include "LSPProjectWrapper.h"

#include "Log.h"
#include "LSPJsonBridge.h"
#include "LSPPipeClient.h"
#include "LSPReaderThread.h"
#include "LSPServersManager.h"
#include "LSPTextDocument.h"
#include "protocol.h"

#include <lsp/messages.h>


namespace {
// Helper: build an lsp::DocumentUri from an LSPTextDocument's file URI.
inline lsp::DocumentUri MakeDocUri(LSPTextDocument* td) {
	return lsp::Uri::parse(td->GetFilenameURI().String());
}

// Build the lsp::ClientCapabilities struct with all features Genio advertises.
// Clangd-specific extension fields are NOT set here — they are patched into the
// serialized JSON in Initialize() since they have no lsp-framework equivalent.
lsp::ClientCapabilities BuildClientCapabilities()
{
	lsp::ClientCapabilities caps;

	// ---- textDocument ----
	lsp::TextDocumentClientCapabilities textDoc;

	// publishDiagnostics (standard fields only; clangd extensions patched later)
	lsp::PublishDiagnosticsClientCapabilities pubDiag;
	pubDiag.relatedInformation = true;
	textDoc.publishDiagnostics = std::move(pubDiag);

	// completion
	lsp::CompletionClientCapabilities comp;
	lsp::CompletionClientCapabilitiesCompletionItem compItem;
	compItem.snippetSupport = true;
	compItem.deprecatedSupport = true;
	comp.completionItem = std::move(compItem);

	lsp::CompletionClientCapabilitiesCompletionItemKind compItemKind;
	lsp::Array<lsp::CompletionItemKindEnum> cikValues;
	for (int i = 0; i < static_cast<int>(lsp::CompletionItemKind::MAX_VALUE); ++i)
		cikValues.push_back(lsp::CompletionItemKindEnum(static_cast<lsp::CompletionItemKind>(i)));
	compItemKind.valueSet = std::move(cikValues);
	comp.completionItemKind = std::move(compItemKind);
	textDoc.completion = std::move(comp);

	// codeAction
	lsp::CodeActionClientCapabilities codeAction;
	lsp::CodeActionClientCapabilitiesCodeActionLiteralSupport litSupport;
	lsp::CodeActionClientCapabilitiesCodeActionLiteralSupportCodeActionKind caKind;
	caKind.valueSet.push_back(lsp::CodeActionKindEnum(lsp::CodeActionKind::QuickFix));
	litSupport.codeActionKind = std::move(caKind);
	codeAction.codeActionLiteralSupport = std::move(litSupport);

	lsp::CodeActionClientCapabilitiesResolveSupport resolveSupport;
	resolveSupport.properties.push_back("edit");
	codeAction.resolveSupport = std::move(resolveSupport);
	textDoc.codeAction = std::move(codeAction);

	// documentSymbol
	lsp::DocumentSymbolClientCapabilities docSym;
	docSym.hierarchicalDocumentSymbolSupport = true;
	textDoc.documentSymbol = std::move(docSym);

	// hover
	lsp::HoverClientCapabilities hover;
	lsp::Array<lsp::MarkupKindEnum> hoverFormats;
	hoverFormats.push_back(lsp::MarkupKindEnum(lsp::MarkupKind::PlainText));
	hover.contentFormat = std::move(hoverFormats);
	textDoc.hover = std::move(hover);

	// signatureHelp
	lsp::SignatureHelpClientCapabilities sigHelp;
	lsp::SignatureHelpClientCapabilitiesSignatureInformation sigInfo;
	lsp::SignatureHelpClientCapabilitiesSignatureInformationParameterInformation paramInfo;
	paramInfo.labelOffsetSupport = true;
	sigInfo.parameterInformation = std::move(paramInfo);
	sigHelp.signatureInformation = std::move(sigInfo);
	textDoc.signatureHelp = std::move(sigHelp);

	caps.textDocument = std::move(textDoc);

	// ---- workspace ----
	lsp::WorkspaceClientCapabilities workspace;

	lsp::WorkspaceSymbolClientCapabilities wsSym;
	lsp::WorkspaceSymbolClientCapabilitiesSymbolKind wsSymKind;
	lsp::Array<lsp::SymbolKindEnum> skValues;
	for (int i = 0; i < static_cast<int>(lsp::SymbolKind::MAX_VALUE); ++i)
		skValues.push_back(lsp::SymbolKindEnum(static_cast<lsp::SymbolKind>(i)));
	wsSymKind.valueSet = std::move(skValues);
	wsSym.symbolKind = std::move(wsSymKind);
	workspace.symbol = std::move(wsSym);

	workspace.applyEdit = false;
	lsp::WorkspaceEditClientCapabilities wsEdit;
	wsEdit.documentChanges = false;
	workspace.workspaceEdit = std::move(wsEdit);

	caps.workspace = std::move(workspace);

	// ---- window ----
	lsp::WindowClientCapabilities window;
	window.workDoneProgress = true;
	caps.window = std::move(window);

	// ---- general ----
	lsp::GeneralClientCapabilities general;
	lsp::Array<lsp::PositionEncodingKindEnum> encodings;
	encodings.push_back(lsp::PositionEncodingKindEnum(lsp::PositionEncodingKind::UTF8));
	general.positionEncodings = std::move(encodings);
	caps.general = std::move(general);

	return caps;
}
} // anonymous namespace


const int32 kLSPMessage = 'LSP!';

LSPProjectWrapper::LSPProjectWrapper(BPath rootPath, const BMessenger& msgr,
		const LSPServerConfigInterface& serverConfig)
	:
	BHandler(rootPath.Path()),
	fLSPPipeClient(nullptr),
	fUrl(rootPath),
	fMessenger(msgr),
	fServerConfig(serverConfig),
	fServerCapabilities(0U)
{
	fUrl.SetAuthority("");
	fInitialized.store(false);
}


void
LSPProjectWrapper::MessageReceived(BMessage* msg)
{
	if (msg->what == kLSPMessage) {
		const char* json;
		if (msg->FindString("data", &json) == B_OK && fLSPPipeClient) {
			try {
				auto value = nlohmann::json::parse(json);

				if (value.count("id")) {
					if (value.contains("method")) {
						onRequest(value["method"].get<std::string>(), value["params"], value["id"]);
					} else if (value.contains("result")) {
						onResponse(value["id"].get<std::string>(), value["result"]);
					} else if (value.contains("error")) {
						onError(value["id"].get<std::string>(), value["error"]);
					}
				} else if (value.contains("method")) {
					if (value.contains("params")) {
						onNotify(value["method"].get<std::string>(), value["params"]);
					}
				}
			}
			catch (std::exception& e) {
				LogTrace("LSPProjectWrapper exception: %s", e.what());
				return;
			}
		}
	}
}


#define X(A) std::to_string((size_t) A)
bool
LSPProjectWrapper::RegisterTextDocument(LSPTextDocument* textDocument)
{
	if (!fServerConfig.IsFileTypeSupported(textDocument->FileType()))
		return false;

	if (!fLSPPipeClient)
		_Create();

	fTextDocs[X(textDocument)] = textDocument;

	if (fInitialized) {
		value emptyParam;
		textDocument->onResponse("initialize", emptyParam);
	}

	return true;
}


void
LSPProjectWrapper::UnregisterTextDocument(LSPTextDocument* textDocument)
{
	if (fTextDocs.find(X(textDocument)) != fTextDocs.end())
		fTextDocs.erase(X(textDocument));
}


bool
LSPProjectWrapper::_Create()
{
	BLooper* looper = nullptr;
	fMessenger.Target(&looper);
	if (!looper)
		return false;

	looper->AddHandler(this);
	BMessenger thisProject = BMessenger(this, looper);

	chdir(Name());

	fLSPPipeClient = new LSPPipeClient(kLSPMessage, thisProject);

	status_t started = fLSPPipeClient->Start(
											const_cast<const char**>(fServerConfig.Argv()),
											fServerConfig.Argc());
	if (started != B_OK) {
		// TODO: show an alert to the user. (but only once per session!)
		// TODO: We usually never get here if the LSP server isn't installed
		// since in ProjectFolder::GetLSPServer() we check if an installed LSP server
		// supports a given file type
		LogInfo("Can't execute lsp sever to provide advanced features! Please install '%s'",
				fServerConfig.Argv()[0]);
		return false;
	}

	Initialize(string_ref(fUrl.UrlString().String()));

	return true;
}


LSPProjectWrapper::~LSPProjectWrapper()
{
	if (Looper()) {
		Looper()->RemoveHandler(this);
	}

	if (!fInitialized) {
		if (fLSPPipeClient) {
			fLSPPipeClient->ForceQuit();
		}
		return;
	}

	for (auto& m : fTextDocs)
		LogError("LSPProjectWrapper::Dispose() still textDocument registered! [%s]",
			m.second->GetFilenameURI().String());

	Shutdown();
	Exit();

	int i = 0;
	while (fLSPPipeClient && !fLSPPipeClient->HasQuitBeenRequested() && i++ < 3) {
		snooze(50000);
	}

	// let's force the thread to quit.
	if (fLSPPipeClient) {
		fLSPPipeClient->KillThread();
	}
}


LSPTextDocument*
LSPProjectWrapper::_DocumentByURI(const char* uri)
{
	LSPTextDocument* doc = nullptr;
	for (auto& x : fTextDocs) {
		if (x.second->GetFilenameURI().Compare(uri) == 0) {
			doc = x.second;
			break;
		}
	}
	return doc;
}


void
LSPProjectWrapper::onNotify(std::string method, value& params)
{
	if (method.compare("textDocument/publishDiagnostics") == 0
		|| method.compare("textDocument/clangd.fileStatus") == 0) {
		auto uri = params["uri"].get<std::string>();

		LSPTextDocument* doc = _DocumentByURI(uri.c_str());
		if (doc) {
			doc->onNotify(method, params);
		} else {
			LogError(
				"Can't deliver a notify from LSP to %s\n%s\n", uri.c_str(), params.dump().c_str());
		}
		return;
	} else if (method.compare("$/progress") == 0) {
/*
 {"token":"backgroundIndexProgress","value":{"kind":"begin","percentage":0,"title":"indexing"}}
 {"token":"backgroundIndexProgress","value":{"kind":"report","message":"0/1","percentage":0}}
 {"token":"backgroundIndexProgress","value":{"kind":"report","message":"0/1","percentage":0}}
 {"token":"backgroundIndexProgress","value":{"kind":"report","message":"0/1","percentage":0}}
 {"token":"backgroundIndexProgress","value":{"kind":"end"}}
*/
		auto value = params["value"];
		auto kind  = value["kind"].get<std::string>();
		if (kind.compare("begin") == 0) {
			fWorkDone.MakeEmpty();
			fWorkDone.what = kLSPWorkProgress;
			fWorkDone.AddString("project", fUrl.Path().String());
			fWorkDone.AddString("kind", kind.c_str());
			fWorkDone.AddString("title", value["title"].get<std::string>().c_str());
			if (value["percentage"].is_null() == false)
				fWorkDone.AddInt32("percentage", value["percentage"].get<int>());
			if (value["message"].is_null() == false)
				fWorkDone.AddString("message", value["message"].get<std::string>().c_str());
			else
				fWorkDone.AddString("message", "");

		} else if (kind.compare("report") == 0 && fWorkDone.IsEmpty() == false) {
			fWorkDone.ReplaceString("kind", kind.c_str());
			if (value["percentage"].is_null() == false) {
			if(fWorkDone.HasInt32("percentage"))
				fWorkDone.ReplaceInt32("percentage", value["percentage"].get<int>());
			else
				fWorkDone.AddInt32("percentage", value["percentage"].get<int>());
			}
			if (value["message"].is_null() == false)
				fWorkDone.ReplaceString("message", value["message"].get<std::string>().c_str());

		} else if (kind.compare("end") == 0 && fWorkDone.IsEmpty() == false) {
			fWorkDone.ReplaceString("kind", kind.c_str());
			if (value["message"].is_null() == false)
				fWorkDone.ReplaceString("message", value["message"].get<std::string>().c_str());
		}

		if(fWorkDone.IsEmpty() == false) {
			fMessenger.SendMessage(&fWorkDone);
		}

		return;

	} else if (method.compare("window/logMessage") == 0) {
		MessageType type = params["type"];
		std::string message = params["message"].get<std::string>();
		switch(type) {
			case MessageType::Error:
				LogError("(Error) %s", message.c_str());
				break;
			case MessageType::Warning:
				LogError("(Warning) %s",message.c_str());
				break;
			case MessageType::Info:
				LogInfo("(Info) %s", message.c_str());
				break;
			case MessageType::Log:
				LogInfo("(Log) %s", message.c_str());
				break;
			case MessageType::Debug:
				LogDebug("(Debug) %s", message.c_str());
				break;
		};
		return;
	}
	LogError("LSPProjectWrapper::onNotify not implemented! [%s]", method.c_str());
}


void
LSPProjectWrapper::onResponse(RequestID id, value& result)
{
	std::size_t found = id.find('_');
	std::string key;
	if (found != std::string::npos) {
		key = id.substr(0, found);
		id = id.substr(found + 1);
	}

	if (id.compare("initialize") == 0) {
		fInitialized.store(true);
		Initialized(result);
		for(std::pair<std::string, LSPTextDocument*> doc : fTextDocs) {
			doc.second->onResponse(id, result);
		}
		return;
	}
	if (id.compare("shutdown") == 0) {
		fprintf(stderr, "Shutdown received\n");
		fInitialized.store(false);
		return;
	}

	auto search = fTextDocs.find(key);
	if (search != fTextDocs.end()) {
		search->second->onResponse(id, result);
	} else {
		LogError("LSPProjectWrapper::onResponse not handled! [%s][%s] for [%s]", key.c_str(),
			id.c_str(), key.c_str());
	}
}


void
LSPProjectWrapper::onError(RequestID id, value& error)
{
	std::size_t found = id.find('_');
	std::string key;
	if (found != std::string::npos) {
		key = id.substr(0, found);
		id = id.substr(found + 1);
	}

	auto search = fTextDocs.find(key);
	if (search != fTextDocs.end())
		search->second->onError(id, error);
	else
		LogError("LSPProjectWrapper::onError not handled! [%s][%s] for [%s]", key.c_str(),
			id.c_str(), key.c_str());
}


void
LSPProjectWrapper::onRequest(std::string method, value& params, value& ID)
{
	LogError("LSPProjectWrapper::onRequest not implemented! [%s] [%s]", method.c_str(),
		ID.dump().c_str());
}


RequestID
LSPProjectWrapper::Initialize(std::optional<DocumentUri> rootUri)
{
	lsp::InitializeParams params;
	params.processId = static_cast<int>(fLSPPipeClient->GetChildPid());

	if (rootUri.has_value())
		params.rootUri = lsp::Uri::parse(rootUri->c_str());
	else
		params.rootUri = nullptr;

	params.capabilities = BuildClientCapabilities();

	// Serialize clangd-specific InitializationOptions via old struct, then
	// inject as opaque LSPAny (the lsp-framework field is Opt<LSPAny>).
	InitializationOptions initOpts;
	initOpts.clangdFileStatus = true;
	nlohmann::json jInitOpts = initOpts;
	params.initializationOptions = LSPBridge::fromNlohmann<lsp::LSPAny>(jInitOpts);

	// Serialize to nlohmann::json via bridge
	nlohmann::json jParams = LSPBridge::toNlohmann(params);

	// Patch clangd extension fields into capabilities (no lsp-framework model)
	jParams["capabilities"]["textDocument"]["publishDiagnostics"]["categorySupport"] = true;
	jParams["capabilities"]["textDocument"]["publishDiagnostics"]["codeActionsInline"] = true;
	jParams["capabilities"]["textDocument"]["completion"]["editsNearCursor"] = true;
	jParams["capabilities"]["window"]["implicitWorkDoneProgressCreate"] = true;

	return SendRequest("client", "initialize", jParams);
}


RequestID
LSPProjectWrapper::Shutdown()
{
	return SendRequest("client", "shutdown", json());
}


RequestID
LSPProjectWrapper::Sync()
{
	return SendRequest("client", "sync", json());
}


void
LSPProjectWrapper::Exit()
{
	SendNotify("exit", json());
}


bool
LSPProjectWrapper::_CheckAndSetCapability(json& capas, const char* str, const LSPCapability flag)
{
	auto& cap = capas[str];
	if (!cap.is_null()) {
		if ( (cap.is_boolean() && cap.get<bool>() == true) || cap.is_object()) {
				fServerCapabilities |= flag;
				return true;
		}
	}
	return false;
}


bool
LSPProjectWrapper::HasCapability(const LSPCapability flag)
{
	return fServerCapabilities & flag;
}


void
LSPProjectWrapper::Initialized(json& result)
{
	auto& capas = result["capabilities"];
	if (!capas.is_null()) {

		if (_CheckAndSetCapability(capas, "completionProvider", kLCapCompletion)) {
			auto& completionProvider = capas["completionProvider"];
			// auto& allCommitCharacters = completionProvider["allCommitCharacters"];
			// if (allCommitCharacters != value::value_t::null) {
				// fAllCommitCharacters.clear();
				// for (auto& c : allCommitCharacters) {
					// printf("--> %s\n", c.get<std::string>().c_str());
					// fAllCommitCharacters.append(c.get<std::string>().c_str());
				// }
				// LogDebug("allCommitCharacters [%s]", this->allCommitCharacters().c_str());
			// }
			auto& triggerCharacters = completionProvider["triggerCharacters"];
			if (!triggerCharacters.is_null()) {
				fTriggerCharacters.clear();
				for (auto& c : triggerCharacters) {
					fTriggerCharacters.append(c.get<std::string>().c_str());
				}
				LogDebug("triggerCharacters [%s]", this->triggerCharacters().c_str());
			}
		}
		_CheckAndSetCapability(capas, "documentFormattingProvider", kLCapDocFormatting);
		_CheckAndSetCapability(capas, "documentRangeFormattingProvider", kLCapDocRangeFormatting);
		_CheckAndSetCapability(capas, "definitionProvider", kLCapDefinition);
		_CheckAndSetCapability(capas, "declarationProvider", kLCapDeclaration);
		_CheckAndSetCapability(capas, "implementationProvider", kLCapImplementation);
		_CheckAndSetCapability(capas, "documentLinkProvider", kLCapDocLink);
		_CheckAndSetCapability(capas, "hoverProvider", kLCapHover);
		_CheckAndSetCapability(capas, "signatureHelpProvider", kLCapSignatureHelp);
		_CheckAndSetCapability(capas, "renameProvider", kLCapRename);
		_CheckAndSetCapability(capas, "documentSymbolProvider", kLCapDocumentSymbols);
	}

	SendNotify("initialized", json());

	fMessenger.SendMessage(kMsgCapabilitiesUpdated);
}


RequestID
LSPProjectWrapper::RegisterCapability()
{
	//?
	return SendRequest("client/registerCapability", "client/registerCapability", json());
}


void
LSPProjectWrapper::DidOpen(LSPTextDocument* textDocument, string_ref text, string_ref languageId)
{
	lsp::DidOpenTextDocumentParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	params.textDocument.text = std::string(text);
	params.textDocument.languageId = std::string(languageId);
	params.textDocument.version = 0;
	SendNotify("textDocument/didOpen", LSPBridge::toNlohmann(params));
}


void
LSPProjectWrapper::DidClose(LSPTextDocument* textDocument)
{
	lsp::DidCloseTextDocumentParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	SendNotify("textDocument/didClose", LSPBridge::toNlohmann(params));
}


void
LSPProjectWrapper::DidChange(LSPTextDocument* textDocument,
	std::vector<TextDocumentContentChangeEvent>& changes, std::optional<bool> wantDiagnostics)
{
	DidChangeTextDocumentParams params;
	params.textDocument.uri = std::move(textDocument->GetFilenameURI().String());
	params.contentChanges = std::move(changes);
	// params.wantDiagnostics = wantDiagnostics;
	SendNotify("textDocument/didChange", params);
}


// xed
void
LSPProjectWrapper::DidSave(LSPTextDocument* textDocument)
{
	lsp::DidSaveTextDocumentParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	SendNotify("textDocument/didSave", LSPBridge::toNlohmann(params));
}


RequestID
LSPProjectWrapper::RangeFomatting(LSPTextDocument* textDocument, Range range)
{
	if (!HasCapability(kLCapDocRangeFormatting))
		return RequestID();

	lsp::DocumentRangeFormattingParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	params.range = range;
	params.options.tabSize = 4;
	params.options.insertSpaces = false;
	return SendRequest(X(textDocument), "textDocument/rangeFormatting", LSPBridge::toNlohmann(params));
}


RequestID
LSPProjectWrapper::FoldingRange(LSPTextDocument* textDocument)
{
	lsp::FoldingRangeParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	return SendRequest(X(textDocument), "textDocument/foldingRange", LSPBridge::toNlohmann(params));
}


RequestID
LSPProjectWrapper::SelectionRange(LSPTextDocument* textDocument, std::vector<Position>& positions)
{
	lsp::SelectionRangeParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	params.positions = std::move(positions);
	return SendRequest(X(textDocument), "textDocument/selectionRange", LSPBridge::toNlohmann(params));
}


RequestID
LSPProjectWrapper::OnTypeFormatting(LSPTextDocument* textDocument, Position position, string_ref ch)
{
	lsp::DocumentOnTypeFormattingParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	params.position = position;
	params.ch = std::string(ch);
	params.options.tabSize = 4;
	params.options.insertSpaces = false;
	return SendRequest(X(textDocument), "textDocument/onTypeFormatting", LSPBridge::toNlohmann(std::move(params)));
}


RequestID
LSPProjectWrapper::Formatting(LSPTextDocument* textDocument)
{
	if (!HasCapability(kLCapDocFormatting))
		return RequestID();

	lsp::DocumentFormattingParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	params.options.tabSize = 4;
	params.options.insertSpaces = false;
	return SendRequest(X(textDocument), "textDocument/formatting", LSPBridge::toNlohmann(std::move(params)));
}


RequestID
LSPProjectWrapper::CodeAction(LSPTextDocument* textDocument, Range range, lsp::CodeActionContext& context)
{
	lsp::CodeActionParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	params.range = range;
	params.context = context;
	return SendRequest(X(textDocument), "textDocument/codeAction", LSPBridge::toNlohmann(params));
}


RequestID
LSPProjectWrapper::CodeActionResolve(LSPTextDocument* textDocument, lsp::CodeAction& data)
{
	auto j = LSPBridge::toNlohmann(data);
	return SendRequest(X(textDocument), "codeAction/resolve", j);
}


RequestID
LSPProjectWrapper::CodeActionResolve(LSPTextDocument* textDocument, nlohmann::json& data)
{
	return SendRequest(X(textDocument), "codeAction/resolve", data);
}


RequestID
LSPProjectWrapper::Completion(
	LSPTextDocument* textDocument, Position position, lsp::CompletionContext& context)
{
	if (!HasCapability(kLCapCompletion))
		return RequestID();

	lsp::CompletionParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	params.position = position;
	params.context = context;
	return SendRequest(X(textDocument), "textDocument/completion", LSPBridge::toNlohmann(params));
}


RequestID
LSPProjectWrapper::SignatureHelp(LSPTextDocument* textDocument, Position position)
{
	if (!HasCapability(kLCapSignatureHelp))
		return RequestID();

	lsp::TextDocumentPositionParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	params.position = position;
	return SendRequest(X(textDocument), "textDocument/signatureHelp", LSPBridge::toNlohmann(std::move(params)));
}


RequestID
LSPProjectWrapper::GoToDefinition(LSPTextDocument* textDocument, Position position)
{
	if (!HasCapability(kLCapDefinition))
		return RequestID();

	lsp::TextDocumentPositionParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	params.position = position;
	return SendRequest(X(textDocument), "textDocument/definition", LSPBridge::toNlohmann(std::move(params)));
}


RequestID
LSPProjectWrapper::GoToImplementation(LSPTextDocument* textDocument, Position position)
{
	if (!HasCapability(kLCapImplementation))
		return RequestID();

	lsp::TextDocumentPositionParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	params.position = position;
	return SendRequest(X(textDocument), "textDocument/implementation", LSPBridge::toNlohmann(std::move(params)));
}


RequestID
LSPProjectWrapper::GoToDeclaration(LSPTextDocument* textDocument, Position position)
{
	if (!HasCapability(kLCapDeclaration))
		return RequestID();

	lsp::TextDocumentPositionParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	params.position = position;
	return SendRequest(X(textDocument), "textDocument/declaration", LSPBridge::toNlohmann(std::move(params)));
}


RequestID
LSPProjectWrapper::References(LSPTextDocument* textDocument, Position position)
{
	lsp::ReferenceParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	params.position = position;
	params.context.includeDeclaration = true;
	return SendRequest(X(textDocument), "textDocument/references", LSPBridge::toNlohmann(std::move(params)));
}


RequestID
LSPProjectWrapper::SwitchSourceHeader(LSPTextDocument* textDocument)
{
	lsp::TextDocumentIdentifier params;
	params.uri = MakeDocUri(textDocument);
	return SendRequest(X(textDocument), "textDocument/switchSourceHeader", LSPBridge::toNlohmann(std::move(params)));
}


RequestID
LSPProjectWrapper::Rename(LSPTextDocument* textDocument, Position position, string_ref newName)
{
	lsp::RenameParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	params.position = position;
	params.newName = std::string(newName);
	return SendRequest(X(textDocument), "textDocument/rename", LSPBridge::toNlohmann(std::move(params)));
}


RequestID
LSPProjectWrapper::Hover(LSPTextDocument* textDocument, Position position)
{
	if (!HasCapability(kLCapHover))
		return RequestID();

	lsp::TextDocumentPositionParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	params.position = position;
	return SendRequest(X(textDocument), "textDocument/hover", LSPBridge::toNlohmann(std::move(params)));
}


RequestID
LSPProjectWrapper::DocumentSymbol(LSPTextDocument* textDocument)
{
	if (!HasCapability(kLCapDocumentSymbols))
		return RequestID();

	lsp::DocumentSymbolParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	return SendRequest(X(textDocument), "textDocument/documentSymbol", LSPBridge::toNlohmann(std::move(params)));
}


RequestID
LSPProjectWrapper::DocumentColor(LSPTextDocument* textDocument)
{
	lsp::DocumentColorParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	return SendRequest(X(textDocument), "textDocument/documentColor", LSPBridge::toNlohmann(std::move(params)));
}


RequestID
LSPProjectWrapper::DocumentHighlight(LSPTextDocument* textDocument, Position position)
{
	lsp::DocumentHighlightParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	params.position = position;
	return SendRequest(X(textDocument), "textDocument/documentHighlight", LSPBridge::toNlohmann(std::move(params)));
}


RequestID
LSPProjectWrapper::SymbolInfo(LSPTextDocument* textDocument, Position position)
{
	lsp::TextDocumentPositionParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	params.position = position;
	return SendRequest(X(textDocument), "textDocument/symbolInfo", LSPBridge::toNlohmann(std::move(params)));
}


RequestID
LSPProjectWrapper::DocumentLink(LSPTextDocument* textDocument)
{
	if (!HasCapability(kLCapDocLink))
		return RequestID();

	lsp::DocumentLinkParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	return SendRequest(X(textDocument), "textDocument/documentLink", LSPBridge::toNlohmann(std::move(params)));
}


RequestID
LSPProjectWrapper::SendRequest(RequestID id, string_ref method, value params)
{
	id.append("_").append(method);
	fLSPPipeClient->request(method, params, id);
	return id;
}


void
LSPProjectWrapper::SendNotify(string_ref method, value params = json())
{
	fLSPPipeClient->notify(method, params);
}
