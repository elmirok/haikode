/*
 * Copyright 2023, Andrea Anzani
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#include "LSPProjectWrapper.h"

#include "Log.h"
#include "LSPEditorWrapper.h"
#include "LSPJsonBridge.h"
#include "LSPPipeClient.h"
#include "LSPServersManager.h"
#include "LSPTextDocument.h"

#include <lsp/json/json.h>
#include <lsp/messages.h>
#include <Looper.h>
#include <chrono>


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


enum {
	kLSPNotify   = 'LSn!',
	kLSPResponse = 'LSr!',
	kLSPError    = 'LSe!',
};


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
	switch (msg->what) {
		case kLSPNotify:
		{
			const char* method;
			const char* data;
			if (msg->FindString("method", &method) == B_OK
				&& msg->FindString("data", &data) == B_OK) {
				try {
					auto params = lsp::json::parse(data);
					_OnNotify(method, params);
				} catch (std::exception& e) {
					LogTrace("LSPProjectWrapper notification exception: %s", e.what());
				}
			}
			break;
		}
		case kLSPResponse:
		{
			const char* documentKey;
			const char* method;
			const char* data;
			if (msg->FindString("documentKey", &documentKey) == B_OK
				&& msg->FindString("method", &method) == B_OK
				&& msg->FindString("data", &data) == B_OK) {
				try {
					auto result = lsp::json::parse(data);
					_OnResponse(documentKey, method, result);
				} catch (std::exception& e) {
					LogTrace("LSPProjectWrapper response exception: %s", e.what());
				}
			}
			break;
		}
		case kLSPError:
		{
			const char* documentKey;
			const char* method;
			const char* data;
			if (msg->FindString("documentKey", &documentKey) == B_OK
				&& msg->FindString("method", &method) == B_OK
				&& msg->FindString("data", &data) == B_OK) {
				try {
					auto error = lsp::json::parse(data);
					_OnError(documentKey, method, error);
				} catch (std::exception& e) {
					LogTrace("LSPProjectWrapper error exception: %s", e.what());
				}
			}
			break;
		}
		case kLSPTypedResponse:
		{
			_DrainResponseQueue();
			break;
		}
		default:
			BHandler::MessageReceived(msg);
			break;
	}
}


void
LSPProjectWrapper::_DrainResponseQueue()
{
	std::queue<std::function<void()>> pending;
	{
		std::lock_guard<std::mutex> guard(fResponseQueueLock);
		std::swap(pending, fResponseQueue);
	}
	while (!pending.empty()) {
		try {
			pending.front()();
		} catch (std::exception& e) {
			LogTrace("LSPProjectWrapper typed-response exception: %s", e.what());
		}
		pending.pop();
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
	fHandlerMessenger = BMessenger(this, looper);

	chdir(Name());

	fLSPPipeClient = new LSPPipeClient();

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

	_RegisterHandlers();
	Initialize(std::string(fUrl.UrlString().String()));

	return true;
}


LSPProjectWrapper::~LSPProjectWrapper()
{
	if (Looper()) {
		Looper()->RemoveHandler(this);
	}

	if (!fInitialized || !fLSPPipeClient) {
		delete fLSPPipeClient;
		return;
	}

	for (auto& m : fTextDocs)
		LogError("LSPProjectWrapper::Dispose() still textDocument registered! [%s]",
			m.second->GetFilenameURI().String());

	// Synchronous shutdown: send shutdown request and wait for response
	if (fLSPPipeClient->IsRunning()) {
		try {
			auto resp = fLSPPipeClient->Handler().sendRequest("shutdown");
			resp.result.wait_for(std::chrono::milliseconds(500));
		} catch (...) {}

		try {
			fLSPPipeClient->Handler().sendNotification("exit");
		} catch (...) {}

		// Give server a moment to process exit
		int i = 0;
		while (fLSPPipeClient->IsRunning() && i++ < 3)
			snooze(50000);
	}

	delete fLSPPipeClient;
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
LSPProjectWrapper::_OnNotify(std::string method, value& params)
{
	if (method.compare("textDocument/publishDiagnostics") == 0
		|| method.compare("textDocument/clangd.fileStatus") == 0) {
		auto uri = params.object().get("uri").string();

		LSPTextDocument* doc = _DocumentByURI(uri.c_str());
		if (doc) {
			doc->onNotify(method, params);
		} else {
			LogError(
				"Can't deliver a notify from LSP to %s\n%s\n", uri.c_str(),
				lsp::json::stringify(params).c_str());
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
		auto& val = params.object().get("value");
		auto kind = val.object().get("kind").string();
		if (kind.compare("begin") == 0) {
			fWorkDone.MakeEmpty();
			fWorkDone.what = kLSPWorkProgress;
			fWorkDone.AddString("project", fUrl.Path().String());
			fWorkDone.AddString("kind", kind.c_str());
			fWorkDone.AddString("title", val.object().get("title").string().c_str());
			auto* percentage = val.object().find("percentage");
			if (percentage && !percentage->isNull())
				fWorkDone.AddInt32("percentage", percentage->integer());
			auto* message = val.object().find("message");
			if (message && !message->isNull())
				fWorkDone.AddString("message", message->string().c_str());
			else
				fWorkDone.AddString("message", "");

		} else if (kind.compare("report") == 0 && fWorkDone.IsEmpty() == false) {
			fWorkDone.ReplaceString("kind", kind.c_str());
			auto* percentage = val.object().find("percentage");
			if (percentage && !percentage->isNull()) {
			if(fWorkDone.HasInt32("percentage"))
				fWorkDone.ReplaceInt32("percentage", percentage->integer());
			else
				fWorkDone.AddInt32("percentage", percentage->integer());
			}
			auto* message = val.object().find("message");
			if (message && !message->isNull())
				fWorkDone.ReplaceString("message", message->string().c_str());

		} else if (kind.compare("end") == 0 && fWorkDone.IsEmpty() == false) {
			fWorkDone.ReplaceString("kind", kind.c_str());
			auto* message = val.object().find("message");
			if (message && !message->isNull())
				fWorkDone.ReplaceString("message", message->string().c_str());
		}

		if(fWorkDone.IsEmpty() == false) {
			fMessenger.SendMessage(&fWorkDone);
		}

		return;

	} else if (method.compare("window/logMessage") == 0) {
		auto logParams = LSPBridge::fromJson<lsp::LogMessageParams>(params);
		lsp::MessageType type = static_cast<lsp::MessageType>(logParams.type);
		switch(type) {
			case lsp::MessageType::Error:
				LogError("(Error) %s", logParams.message.c_str());
				break;
			case lsp::MessageType::Warning:
				LogError("(Warning) %s", logParams.message.c_str());
				break;
			case lsp::MessageType::Info:
				LogInfo("(Info) %s", logParams.message.c_str());
				break;
			case lsp::MessageType::Log:
				LogInfo("(Log) %s", logParams.message.c_str());
				break;
			case lsp::MessageType::Debug:
				LogDebug("(Debug) %s", logParams.message.c_str());
				break;
			default:
				break;
		};
		return;
	}
	LogError("LSPProjectWrapper::onNotify not implemented! [%s]", method.c_str());
}


void
LSPProjectWrapper::_OnResponse(const std::string& documentKey, std::string method, value& result)
{
	if (method.compare("shutdown") == 0) {
		fprintf(stderr, "Shutdown received\n");
		fInitialized.store(false);
		return;
	}

	auto search = fTextDocs.find(documentKey);
	if (search != fTextDocs.end()) {
		search->second->onResponse(method, result);
	} else {
		LogError("LSPProjectWrapper::_OnResponse not handled! [%s] for [%s]", method.c_str(),
			documentKey.c_str());
	}
}


void
LSPProjectWrapper::_OnError(const std::string& documentKey, std::string method, value& error)
{
	auto search = fTextDocs.find(documentKey);
	if (search != fTextDocs.end())
		search->second->onError(method, error);
	else
		LogError("LSPProjectWrapper::_OnError not handled! [%s] for [%s]", method.c_str(),
			documentKey.c_str());
}


void
LSPProjectWrapper::Initialize(std::optional<std::string> rootUri)
{
	lsp::InitializeParams params;
	params.processId = static_cast<int>(fLSPPipeClient->GetChildPid());

	if (rootUri.has_value())
		params.rootUri = lsp::Uri::parse(rootUri->c_str());
	else
		params.rootUri = nullptr;

	params.capabilities = BuildClientCapabilities();

	// Build clangd-specific InitializationOptions directly as JSON.
	lsp::json::Object initOptsObj;
	initOptsObj["clangdFileStatus"] = lsp::json::Value(true);
	params.initializationOptions = lsp::json::Value(std::move(initOptsObj));

	// Serialize to lsp::json::Value
	auto jParams = lsp::toJson(std::move(params));

	// Patch clangd extension fields into capabilities (no lsp-framework model)
	auto& caps = jParams.object()["capabilities"].object();
	auto& textDoc = caps["textDocument"].object();
	auto& pubDiagObj = textDoc["publishDiagnostics"].object();
	pubDiagObj["categorySupport"] = lsp::json::Value(true);
	pubDiagObj["codeActionsInline"] = lsp::json::Value(true);
	textDoc["completion"].object()["editsNearCursor"] = lsp::json::Value(true);
	caps["window"].object()["implicitWorkDoneProgressCreate"] = lsp::json::Value(true);

	_SendJsonRequest("initialize", std::move(jParams),
		[this](value& result) {
			fInitialized.store(true);
			Initialized(result);
			for (auto& doc : fTextDocs)
				static_cast<LSPEditorWrapper*>(doc.second)->_DoInitialize(result);
		});
}


void
LSPProjectWrapper::Shutdown()
{
	_SendRequest(nullptr, "shutdown", json());
}


void
LSPProjectWrapper::Sync()
{
	_SendRequest(nullptr, "sync", json());
}


void
LSPProjectWrapper::Exit()
{
	_SendNotify("exit", json());
}


bool
LSPProjectWrapper::_CheckAndSetCapability(json& capas, const char* str, const LSPCapability flag)
{
	auto* cap = capas.object().find(str);
	if (cap && !cap->isNull()) {
		if ((cap->isBoolean() && cap->boolean()) || cap->isObject()) {
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
	auto* capas = result.object().find("capabilities");
	if (capas && !capas->isNull()) {

		if (_CheckAndSetCapability(*capas, "completionProvider", kLCapCompletion)) {
			auto& completionProvider = capas->object().get("completionProvider");
			auto* triggerCharacters = completionProvider.object().find("triggerCharacters");
			if (triggerCharacters && !triggerCharacters->isNull()) {
				fTriggerCharacters.clear();
				for (auto& c : triggerCharacters->array()) {
					fTriggerCharacters.append(c.string().c_str());
				}
				LogDebug("triggerCharacters [%s]", this->triggerCharacters().c_str());
			}
		}
		_CheckAndSetCapability(*capas, "documentFormattingProvider", kLCapDocFormatting);
		_CheckAndSetCapability(*capas, "documentRangeFormattingProvider", kLCapDocRangeFormatting);
		_CheckAndSetCapability(*capas, "definitionProvider", kLCapDefinition);
		_CheckAndSetCapability(*capas, "declarationProvider", kLCapDeclaration);
		_CheckAndSetCapability(*capas, "implementationProvider", kLCapImplementation);
		_CheckAndSetCapability(*capas, "documentLinkProvider", kLCapDocLink);
		_CheckAndSetCapability(*capas, "hoverProvider", kLCapHover);
		_CheckAndSetCapability(*capas, "signatureHelpProvider", kLCapSignatureHelp);
		_CheckAndSetCapability(*capas, "renameProvider", kLCapRename);
		_CheckAndSetCapability(*capas, "documentSymbolProvider", kLCapDocumentSymbols);
	}

	_SendNotify("initialized", json());

	fMessenger.SendMessage(kMsgCapabilitiesUpdated);
}


void
LSPProjectWrapper::DidOpen(LSPTextDocument* textDocument, std::string_view text, std::string_view languageId)
{
	lsp::DidOpenTextDocumentParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	params.textDocument.text = std::string(text);
	params.textDocument.languageId = std::string(languageId);
	textDocument->ResetVersion();
	params.textDocument.version = textDocument->NextVersion();

	fLSPPipeClient->Handler().sendNotification<lsp::notifications::TextDocument_DidOpen>(std::move(params));
}


void
LSPProjectWrapper::DidClose(LSPTextDocument* textDocument)
{
	lsp::DidCloseTextDocumentParams params;
	params.textDocument.uri = MakeDocUri(textDocument);

	fLSPPipeClient->Handler().sendNotification<lsp::notifications::TextDocument_DidClose>(std::move(params));
}


void
LSPProjectWrapper::DidChange(LSPTextDocument* textDocument,
	std::vector<lsp::TextDocumentContentChangeEvent>& changes, std::optional<bool> wantDiagnostics)
{
	lsp::DidChangeTextDocumentParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	params.textDocument.version = textDocument->NextVersion();
	params.contentChanges = std::move(changes);
	fLSPPipeClient->Handler().sendNotification<lsp::notifications::TextDocument_DidChange>(std::move(params));
}



void
LSPProjectWrapper::DidSave(LSPTextDocument* textDocument)
{
	lsp::DidSaveTextDocumentParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	fLSPPipeClient->Handler().sendNotification<lsp::notifications::TextDocument_DidSave>(std::move(params));
}


void
LSPProjectWrapper::RangeFomatting(LSPTextDocument* textDocument, Range range)
{
	if (!HasCapability(kLCapDocRangeFormatting))
		return;

	lsp::DocumentRangeFormattingParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	params.range = range;
	params.options.tabSize = 4;
	params.options.insertSpaces = false;

	_SendTypedRequest<lsp::requests::TextDocument_RangeFormatting>(
		std::move(params),
		[textDocument](lsp::TextDocument_RangeFormattingResult&& result) {
			if (!result.isNull())
				static_cast<LSPEditorWrapper*>(textDocument)->_DoFormat(std::move(result.value()));
		});
}


void
LSPProjectWrapper::FoldingRange(LSPTextDocument* textDocument)
{
	lsp::FoldingRangeParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	_SendRequest(textDocument, "textDocument/foldingRange", LSPBridge::toJson(params));
}


void
LSPProjectWrapper::SelectionRange(LSPTextDocument* textDocument, std::vector<Position>& positions)
{
	lsp::SelectionRangeParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	params.positions = std::move(positions);
	_SendRequest(textDocument, "textDocument/selectionRange", LSPBridge::toJson(params));
}


void
LSPProjectWrapper::OnTypeFormatting(LSPTextDocument* textDocument, Position position, std::string_view ch)
{
	lsp::DocumentOnTypeFormattingParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	params.position = position;
	params.ch = std::string(ch);
	params.options.tabSize = 4;
	params.options.insertSpaces = false;
	_SendRequest(textDocument, "textDocument/onTypeFormatting", LSPBridge::toJson(std::move(params)));
}


void
LSPProjectWrapper::Formatting(LSPTextDocument* textDocument)
{
	if (!HasCapability(kLCapDocFormatting))
		return;

	lsp::DocumentFormattingParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	params.options.tabSize = 4;
	params.options.insertSpaces = false;

	_SendTypedRequest<lsp::requests::TextDocument_Formatting>(
		std::move(params),
		[textDocument](lsp::TextDocument_FormattingResult&& result) {
			if (!result.isNull())
				static_cast<LSPEditorWrapper*>(textDocument)->_DoFormat(std::move(result.value()));
		});
}


void
LSPProjectWrapper::CodeAction(LSPTextDocument* textDocument, Range range, lsp::CodeActionContext& context)
{
	lsp::CodeActionParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	params.range = range;
	params.context = context;

	_SendTypedRequest<lsp::requests::TextDocument_CodeAction>(
		std::move(params),
		[textDocument](lsp::TextDocument_CodeActionResult&& result) {
			if (!result.isNull())
				static_cast<LSPEditorWrapper*>(textDocument)->_DoCodeActions(std::move(result.value()));
		});
}


void
LSPProjectWrapper::CodeActionResolve(LSPTextDocument* textDocument, lsp::CodeAction& params)
{
	_SendTypedRequest<lsp::requests::CodeAction_Resolve>(
		std::move(params),
		[textDocument](lsp::CodeAction&& result) {
				static_cast<LSPEditorWrapper*>(textDocument)->_DoCodeActionResolve(std::move(result));
		});
}


void
LSPProjectWrapper::Completion(
	LSPTextDocument* textDocument, Position position, lsp::CompletionContext& context)
{
	if (!HasCapability(kLCapCompletion))
		return;

	lsp::CompletionParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	params.position = position;
	params.context = context;

	_SendTypedRequest<lsp::requests::TextDocument_Completion>(
		std::move(params),
		[textDocument](lsp::TextDocument_CompletionResult&& result) {
			static_cast<LSPEditorWrapper*>(textDocument)->_DoCompletion(std::move(result.value()));
		});
}


void
LSPProjectWrapper::SignatureHelp(LSPTextDocument* textDocument, Position position)
{
	if (!HasCapability(kLCapSignatureHelp))
		return;

	lsp::SignatureHelpParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	params.position = position;

	_SendTypedRequest<lsp::requests::TextDocument_SignatureHelp>(
		std::move(params),
		[textDocument](lsp::TextDocument_SignatureHelpResult&& result) {
			if (!result.isNull())
				static_cast<LSPEditorWrapper*>(textDocument)->_DoSignatureHelp(std::move(result.value()));
		});

}


void
LSPProjectWrapper::GoToDefinition(LSPTextDocument* textDocument, Position position)
{
	if (!HasCapability(kLCapDefinition))
		return;

	lsp::DefinitionParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	params.position = position;
	_SendTypedRequest<lsp::requests::TextDocument_Definition>(
		std::move(params),
		[textDocument](lsp::TextDocument_DefinitionResult&& result) {
			if (!result.isNull()) {
				static_cast<LSPEditorWrapper*>(textDocument)->_DoGoTo(std::move(result.value()));
			}
		});
}


void
LSPProjectWrapper::GoToImplementation(LSPTextDocument* textDocument, Position position)
{
	if (!HasCapability(kLCapImplementation))
		return;

	lsp::ImplementationParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	params.position = position;
	_SendTypedRequest<lsp::requests::TextDocument_Implementation>(
		std::move(params),
		[textDocument](lsp::TextDocument_ImplementationResult&& result) {
			lsp::TextDocument_DefinitionResult&& impl = (lsp::TextDocument_DefinitionResult&&)result;
			if (!result.isNull()) {
				static_cast<LSPEditorWrapper*>(textDocument)->_DoGoTo(std::move(impl.value()));
			}
		});
}


void
LSPProjectWrapper::GoToDeclaration(LSPTextDocument* textDocument, Position position)
{
	if (!HasCapability(kLCapDeclaration))
		return;

	lsp::DeclarationParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	params.position = position;

	//using TextDocument_DeclarationResult = NullOrOneOf<Declaration, Array<DeclarationLink>>;


	_SendTypedRequest<lsp::requests::TextDocument_Declaration>(
		std::move(params),
		[textDocument](lsp::TextDocument_DeclarationResult&& result) {
			lsp::TextDocument_DefinitionResult&& impl = (lsp::TextDocument_DefinitionResult&&)result;
			if (!result.isNull()) {
				static_cast<LSPEditorWrapper*>(textDocument)->_DoGoTo(std::move(impl.value()));
			}
		});
}


void
LSPProjectWrapper::References(LSPTextDocument* textDocument, Position position)
{
	lsp::ReferenceParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	params.position = position;
	params.context.includeDeclaration = true;
	_SendRequest(textDocument, "textDocument/references", LSPBridge::toJson(std::move(params)));
}




void
LSPProjectWrapper::Rename(LSPTextDocument* textDocument, Position position, std::string_view newName)
{
	lsp::RenameParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	params.position = position;
	params.newName = std::string(newName);

	_SendTypedRequest<lsp::requests::TextDocument_Rename>(
		std::move(params),
		[textDocument](lsp::TextDocument_RenameResult&& result) {
			if (!result.isNull())
				static_cast<LSPEditorWrapper*>(textDocument)->_DoRename(std::move(result.value()));
		});
}


void
LSPProjectWrapper::Hover(LSPTextDocument* textDocument, Position position)
{
	if (!HasCapability(kLCapHover))
		return;

	lsp::HoverParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	params.position = position;

	_SendTypedRequest<lsp::requests::TextDocument_Hover>(
		std::move(params),
		[textDocument](lsp::TextDocument_HoverResult&& result) {
			static_cast<LSPEditorWrapper*>(textDocument)->_DoHover(std::move(result));
		});
}


void
LSPProjectWrapper::DocumentSymbol(LSPTextDocument* textDocument)
{
	if (!HasCapability(kLCapDocumentSymbols))
		return;

	lsp::DocumentSymbolParams params;
	params.textDocument.uri = MakeDocUri(textDocument);

	_SendTypedRequest<lsp::requests::TextDocument_DocumentSymbol>(
		std::move(params),
		[textDocument](lsp::TextDocument_DocumentSymbolResult&& result) {
			static_cast<LSPEditorWrapper*>(textDocument)->_DoDocumentSymbol(std::move(result));
		});
}

/*
void
LSPProjectWrapper::DocumentColor(LSPTextDocument* textDocument)
{
	lsp::DocumentColorParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	_SendRequest(textDocument, "textDocument/documentColor", LSPBridge::toJson(std::move(params)));
}


void
LSPProjectWrapper::DocumentHighlight(LSPTextDocument* textDocument, Position position)
{
	lsp::DocumentHighlightParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	params.position = position;
	_SendRequest(textDocument, "textDocument/documentHighlight", LSPBridge::toJson(std::move(params)));
}
*/
/*
void
LSPProjectWrapper::SymbolInfo(LSPTextDocument* textDocument, Position position)
{
	lsp::TextDocumentPositionParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	params.position = position;
	_SendRequest(textDocument, "textDocument/symbolInfo", LSPBridge::toJson(std::move(params)));
}
*/

void
LSPProjectWrapper::DocumentLink(LSPTextDocument* textDocument)
{
	if (!HasCapability(kLCapDocLink))
		return;

	lsp::DocumentLinkParams params;
	params.textDocument.uri = MakeDocUri(textDocument);
	_SendTypedRequest<lsp::requests::TextDocument_DocumentLink>(
		std::move(params),
		[textDocument](lsp::TextDocument_DocumentLinkResult&& result) {
			static_cast<LSPEditorWrapper*>(textDocument)->_DoDocumentLink(std::move(result.value()));
		});
}


// --- Send helpers (via lsp::MessageHandler) ---


void
LSPProjectWrapper::_SendRequest(LSPTextDocument* textDocument, std::string_view method, value params)
{
	std::string docKey = textDocument ? X(textDocument) : "client";
	std::string methodStr(method);

	fLSPPipeClient->Handler().sendRequest(
		methodStr,
		std::move(params),
		[this, docKey, methodStr](lsp::json::Value&& result) {
			BMessage msg(kLSPResponse);
			msg.AddString("documentKey", docKey.c_str());
			msg.AddString("method", methodStr.c_str());
			msg.AddString("data", lsp::json::stringify(result).c_str());
			fHandlerMessenger.SendMessage(&msg);
		},
		[this, docKey, methodStr](const lsp::ResponseError& error) {
			BMessage msg(kLSPError);
			msg.AddString("documentKey", docKey.c_str());
			msg.AddString("method", methodStr.c_str());
			lsp::json::Object errObj;
			errObj["code"] = error.code();
			errObj["message"] = std::string(error.message());
			msg.AddString("data", lsp::json::stringify(lsp::json::Value(std::move(errObj))).c_str());
			fHandlerMessenger.SendMessage(&msg);
		});
}


void
LSPProjectWrapper::_SendNotify(std::string_view method, value params)
{
	fLSPPipeClient->Handler().sendNotification(method, std::optional<lsp::json::Value>(std::move(params)));
}


// --- Handler registration (callbacks fire on reader thread, marshal to UI) ---


void
LSPProjectWrapper::_RegisterHandlers()
{
	auto& handler = fLSPPipeClient->Handler();

	// Helper: create a generic notification handler that marshals to the UI thread.
	auto marshalNotify = [this](const char* methodName) {
		std::string method(methodName);
		return [this, method](lsp::json::Value&& params) -> lsp::json::Value {
			BMessage msg(kLSPNotify);
			msg.AddString("method", method.c_str());
			msg.AddString("data", lsp::json::stringify(params).c_str());
			fHandlerMessenger.SendMessage(&msg);
			return lsp::json::Value{};
		};
	};

	// Server → Client notifications
	handler.add<lsp::notifications::TextDocument_PublishDiagnostics>(
		[this](lsp::PublishDiagnosticsParams&& params) {
			{
				std::lock_guard<std::mutex> guard(fResponseQueueLock);
				fResponseQueue.push(
					[this, p = std::move(params)]() mutable {
						std::string uri = p.uri.toString();
						LSPTextDocument* doc = _DocumentByURI(uri.c_str());
						if (doc)
							static_cast<LSPEditorWrapper*>(doc)->_DoDiagnostics(std::move(p));
					});
			}
			fHandlerMessenger.SendMessage(kLSPTypedResponse);
		});
	handler.add("textDocument/clangd.fileStatus",
		marshalNotify("textDocument/clangd.fileStatus"));
	handler.add("$/progress",
		marshalNotify("$/progress"));
	handler.add("window/logMessage",
		marshalNotify("window/logMessage"));

	// Server → Client requests
	handler.add("window/workDoneProgress/create",
		[](lsp::json::Value&&) -> lsp::json::Value {
			return lsp::json::Value{};  // accept
		});
}
