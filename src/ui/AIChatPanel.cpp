/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "AIChatPanel.h"

#include <Application.h>
#include <Alert.h>
#include <Button.h>
#include <Catalog.h>
#include <Entry.h>
#include <Layout.h>
#include <LayoutBuilder.h>
#include <ListView.h>
#include <Looper.h>
#include <Message.h>
#include <Messenger.h>
#include <Path.h>
#include <Roster.h>
#include <ScrollView.h>
#include <StringItem.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>
#include <Window.h>

#include "ConfigManager.h"
#include "GenioWindowMessages.h"

#include <cerrno>
#include <cstring>
#include <cstdlib>
#include <string>
#include <thread>

#ifdef HAIKODE_AI_NETWORK
#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <unistd.h>
#endif

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "AIChatPanel"

extern ConfigManager gCFG;

namespace {

const uint32 kMsgSaveProvider = 'hisp';
const uint32 kMsgOpenSetup = 'hios';
const uint32 kMsgSetupSave = 'hiss';
const uint32 kMsgSetupCancel = 'hisc';
const uint32 kMsgSetupSaved = 'hisd';
const uint32 kMsgSetupPresetOpenAI = 'hiso';
const uint32 kMsgSetupPresetOllama = 'hisl';
const uint32 kMsgSetupPresetLMStudio = 'hism';
const uint32 kMsgSetupPresetOpenRouter = 'hisr';
const uint32 kMsgSetupPresetLlamaCpp = 'hslc';
const uint32 kMsgSetupPresetOAuth = 'hioa';
const uint32 kMsgPresetOpenAI = 'hpai';
const uint32 kMsgPresetOllama = 'hpol';
const uint32 kMsgPresetLMStudio = 'hplm';
const uint32 kMsgPresetOpenRouter = 'hpor';
const uint32 kMsgPresetLlamaCpp = 'hplc';
const uint32 kMsgTestProvider = 'hitp';
const uint32 kMsgTestProviderResponse = 'hitr';
const uint32 kMsgStartOAuth = 'hiox';
const uint32 kMsgExchangeOAuth = 'hixo';
const uint32 kMsgOAuthResponse = 'hior';
const uint32 kMsgOAuthCallback = 'hioc';
const uint32 kMsgCancelRequest = 'hicx';
const uint32 kMsgAsk = 'hiak';
const uint32 kMsgExplainSelection = 'hiex';
const uint32 kMsgSummarizeProject = 'hisu';
const uint32 kMsgProposePatch = 'hipa';
const uint32 kMsgCodexStatus = 'hicd';
const uint32 kMsgCodexLogin = 'hicl';
const uint32 kMsgCodexAsk = 'hica';
const uint32 kMsgCodexCaptureResponse = 'hicr';
const uint32 kMsgCommandCaptureResponse = 'hccr';
const uint32 kMsgAIResponse = 'hirs';
const uint32 kMsgPreviousPatchFile = 'hipv';
const uint32 kMsgNextPatchFile = 'hinx';
const uint32 kMsgPreviousPatchHunk = 'hihp';
const uint32 kMsgNextPatchHunk = 'hihn';
const uint32 kMsgApplyFirstFile = 'hiaf';
const uint32 kMsgRejectFirstFile = 'hirf';
const uint32 kMsgApplyHunk = 'hiah';
const uint32 kMsgRejectHunk = 'hirh';
const uint32 kMsgReviewPatch = 'hivr';
const uint32 kMsgApplyPatch = 'hiap';
const uint32 kMsgRejectPatch = 'hirp';
const uint32 kMsgRunCommand = 'hirc';
const uint32 kMsgRejectCommand = 'hirx';
const uint32 kMsgListProjectFiles = 'hifl';
const uint32 kMsgOpenProjectFile = 'hifo';
const uint32 kMsgProjectFilePicked = 'hifp';
const uint32 kMsgListRecords = 'hilr';
const uint32 kMsgShowRecord = 'hird';
const uint32 kMsgRecordPicked = 'hipr';
const uint32 kMsgRecordOpen = 'hiro';
const uint32 kMsgRecordCancel = 'hrcx';

Haikode::AI::AuthMode
AuthModeFromString(const BString& value)
{
	if (value.ICompare("local") == 0)
		return Haikode::AI::AuthMode::Local;
	if (value.ICompare("oauth") == 0)
		return Haikode::AI::AuthMode::OAuth;
	if (value.ICompare("none") == 0)
		return Haikode::AI::AuthMode::None;
	return Haikode::AI::AuthMode::ApiKey;
}

const char*
InitialAIStatusText()
{
#ifdef HAIKODE_AI_NETWORK
	return B_TRANSLATE("AI network transport is enabled. Use AI Setup to paste an API key inside Haikode, choose a local provider, or configure OAuth.");
#else
	return B_TRANSLATE("AI network transport is disabled in this build. Rebuild with HAIKODE_AI_NETWORK=1 and install curl_devel to use cloud AI. API keys are entered inside AI Setup; no Terminal export is required.");
#endif
}


const char*
SetupNetworkStatusText()
{
#ifdef HAIKODE_AI_NETWORK
	return B_TRANSLATE("Network AI is enabled in this build. Paste the API key here and press Save.");
#else
	return B_TRANSLATE("Network AI is disabled in this build. Settings can be saved here, but cloud AI needs a rebuild with HAIKODE_AI_NETWORK=1.");
#endif
}

#ifdef HAIKODE_AI_NETWORK
bool
ParseLoopbackRedirect(const std::string& redirectUri, uint16_t& port,
	std::string& error)
{
	port = 0;
	error.clear();

	const size_t schemeEnd = redirectUri.find("://");
	if (schemeEnd == std::string::npos
		|| redirectUri.substr(0, schemeEnd) != "http") {
		error = "OAuth callback listener requires an http:// loopback redirect URI.";
		return false;
	}

	const size_t authorityStart = schemeEnd + 3;
	const size_t pathStart = redirectUri.find('/', authorityStart);
	const std::string authority = redirectUri.substr(authorityStart,
		pathStart == std::string::npos ? std::string::npos
			: pathStart - authorityStart);
	const size_t colon = authority.rfind(':');
	const std::string host = colon == std::string::npos ? authority
		: authority.substr(0, colon);
	if (host != "127.0.0.1" && host != "localhost") {
		error = "OAuth callback listener only accepts 127.0.0.1 or localhost redirect URIs.";
		return false;
	}

	if (colon == std::string::npos) {
		port = 80;
		return true;
	}

	char* end = nullptr;
	const long parsedPort = std::strtol(authority.c_str() + colon + 1, &end, 10);
	if (end == authority.c_str() + colon + 1 || *end != '\0'
		|| parsedPort <= 0 || parsedPort > 65535) {
		error = "OAuth redirect URI has an invalid port.";
		return false;
	}
	port = static_cast<uint16_t>(parsedPort);
	return true;
}


bool
WaitForOAuthCallback(const std::string& redirectUri, std::string& callback,
	std::string& error)
{
	callback.clear();
	error.clear();

	uint16_t port = 0;
	if (!ParseLoopbackRedirect(redirectUri, port, error))
		return false;

	const int server = socket(AF_INET, SOCK_STREAM, 0);
	if (server < 0) {
		error = std::string("Could not create OAuth callback socket: ")
			+ strerror(errno);
		return false;
	}

	const int reuse = 1;
	setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));

	sockaddr_in address;
	memset(&address, 0, sizeof(address));
	address.sin_family = AF_INET;
	address.sin_addr.s_addr = htonl(INADDR_LOOPBACK);
	address.sin_port = htons(port);

	if (bind(server, reinterpret_cast<sockaddr*>(&address), sizeof(address))
		!= 0) {
		error = std::string("Could not bind OAuth callback listener: ")
			+ strerror(errno);
		close(server);
		return false;
	}
	if (listen(server, 1) != 0) {
		error = std::string("Could not listen for OAuth callback: ")
			+ strerror(errno);
		close(server);
		return false;
	}

	fd_set readSet;
	FD_ZERO(&readSet);
	FD_SET(server, &readSet);
	timeval timeout;
	timeout.tv_sec = 120;
	timeout.tv_usec = 0;
	const int ready = select(server + 1, &readSet, nullptr, nullptr, &timeout);
	if (ready <= 0) {
		error = ready == 0 ? "OAuth callback listener timed out."
			: std::string("OAuth callback listener failed: ") + strerror(errno);
		close(server);
		return false;
	}

	const int client = accept(server, nullptr, nullptr);
	if (client < 0) {
		error = std::string("Could not accept OAuth callback: ")
			+ strerror(errno);
		close(server);
		return false;
	}

	char buffer[4096];
	const ssize_t received = recv(client, buffer, sizeof(buffer) - 1, 0);
	if (received <= 0) {
		error = "OAuth callback was empty.";
		close(client);
		close(server);
		return false;
	}
	buffer[received] = '\0';

	const std::string request(buffer);
	const size_t methodEnd = request.find(' ');
	const size_t targetEnd = methodEnd == std::string::npos
		? std::string::npos : request.find(' ', methodEnd + 1);
	if (methodEnd == std::string::npos || targetEnd == std::string::npos) {
		error = "OAuth callback HTTP request was malformed.";
		close(client);
		close(server);
		return false;
	}
	callback = request.substr(methodEnd + 1, targetEnd - methodEnd - 1);

	const char response[] =
		"HTTP/1.1 200 OK\r\n"
		"Content-Type: text/plain; charset=utf-8\r\n"
		"Connection: close\r\n"
		"\r\n"
		"Haikode received the OAuth callback. You can return to Haikode.";
	send(client, response, sizeof(response) - 1, 0);
	close(client);
	close(server);
	return true;
}
#endif

bool
IsSafeRelativeProjectPath(const BString& path)
{
	if (path.IsEmpty() || path.StartsWith("/"))
		return false;
	if (path == "." || path == "..")
		return false;
	if (path.FindFirst("../") >= 0 || path.FindFirst("/../") >= 0
		|| path.FindFirst("/..") == path.Length() - 3) {
		return false;
	}
	return true;
}


class ProjectFileListItem : public BStringItem {
public:
	ProjectFileListItem(const Haikode::AI::ProjectFileSummary& file)
		:
		BStringItem(_Label(file).String()),
		fPath(file.path.c_str())
	{
	}

	const char* Path() const
	{
		return fPath.String();
	}

private:
	static BString _Label(const Haikode::AI::ProjectFileSummary& file)
	{
		BString label;
		label << file.path.c_str() << "  [" << file.language.c_str() << ", "
			<< file.role.c_str() << ", " << file.risk.c_str() << "]";
		if (file.hasTodo)
			label << " TODO";
		return label;
	}

	BString fPath;
};


class ProjectFileBrowserWindow : public BWindow {
public:
	ProjectFileBrowserWindow(BMessenger target,
		const std::vector<Haikode::AI::ProjectFileSummary>& files)
		:
		BWindow(BRect(120, 120, 760, 520), B_TRANSLATE("Haikode project files"),
			B_TITLED_WINDOW_LOOK, B_MODAL_APP_WINDOW_FEEL,
			B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS
				| B_CLOSE_ON_ESCAPE),
		fTarget(target)
	{
		fListView = new BListView("haikode_project_file_picker",
			B_SINGLE_SELECTION_LIST);
		fListView->SetInvocationMessage(new BMessage(kMsgProjectFilePicked));
		fListView->SetTarget(this);
		for (const Haikode::AI::ProjectFileSummary& file : files)
			fListView->AddItem(new ProjectFileListItem(file));
		if (fListView->CountItems() > 0)
			fListView->Select(0);

		BScrollView* scroll = new BScrollView(
			"haikode_project_file_picker_scroll", fListView, B_FOLLOW_ALL, 0,
			false, true);
		scroll->SetExplicitMinSize(BSize(560, 260));

		BButton* cancelButton = new BButton("haikode_project_file_cancel",
			B_TRANSLATE("Cancel"), new BMessage(kMsgRecordCancel));
		BButton* openButton = new BButton("haikode_project_file_open",
			B_TRANSLATE("Open"), new BMessage(kMsgOpenProjectFile));
		cancelButton->SetTarget(this);
		openButton->SetTarget(this);

		BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_DEFAULT_SPACING)
			.SetInsets(B_USE_WINDOW_SPACING)
			.Add(scroll)
			.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
				.AddGlue()
				.Add(cancelButton)
				.Add(openButton)
			.End();

		openButton->MakeDefault(true);
		if (GetLayout() != nullptr) {
			const BSize size = GetLayout()->MinSize();
			ResizeTo(size.width, size.height);
		}
		CenterOnScreen();
	}

	void MessageReceived(BMessage* message) override
	{
		switch (message->what) {
			case kMsgProjectFilePicked:
			case kMsgOpenProjectFile:
				_SendSelectionAndQuit();
				break;
			case kMsgRecordCancel:
				Quit();
				break;
			default:
				BWindow::MessageReceived(message);
				break;
		}
	}

private:
	void _SendSelectionAndQuit()
	{
		const int32 selection = fListView->CurrentSelection();
		ProjectFileListItem* item = dynamic_cast<ProjectFileListItem*>(
			fListView->ItemAt(selection));
		if (item == nullptr)
			return;

		BMessage picked(kMsgProjectFilePicked);
		picked.AddString("path", item->Path());
		fTarget.SendMessage(&picked);
		Quit();
	}

	BMessenger fTarget;
	BListView* fListView;
};


class ProjectRecordListItem : public BStringItem {
public:
	ProjectRecordListItem(const Haikode::AI::ProjectRecordEntry& record)
		:
		BStringItem(_Label(record).String()),
		fPath(record.path.c_str())
	{
	}

	const char* Path() const
	{
		return fPath.String();
	}

private:
	static BString _Label(const Haikode::AI::ProjectRecordEntry& record)
	{
		BString label;
		label << record.type.c_str() << "  " << record.path.c_str()
			<< "  (" << std::to_string(record.sizeBytes).c_str() << " bytes)";
		return label;
	}

	BString fPath;
};


class ProjectRecordBrowserWindow : public BWindow {
public:
	ProjectRecordBrowserWindow(BMessenger target,
		const std::vector<Haikode::AI::ProjectRecordEntry>& records)
		:
		BWindow(BRect(120, 120, 760, 520), B_TRANSLATE("Haikode records"),
			B_TITLED_WINDOW_LOOK, B_MODAL_APP_WINDOW_FEEL,
			B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS
				| B_CLOSE_ON_ESCAPE),
		fTarget(target)
	{
		fListView = new BListView("haikode_record_picker",
			B_SINGLE_SELECTION_LIST);
		fListView->SetInvocationMessage(new BMessage(kMsgRecordPicked));
		fListView->SetTarget(this);
		for (const Haikode::AI::ProjectRecordEntry& record : records)
			fListView->AddItem(new ProjectRecordListItem(record));
		if (fListView->CountItems() > 0)
			fListView->Select(0);

		BScrollView* scroll = new BScrollView("haikode_record_picker_scroll",
			fListView, B_FOLLOW_ALL, 0, false, true);
		scroll->SetExplicitMinSize(BSize(560, 260));

		BButton* cancelButton = new BButton("haikode_record_cancel",
			B_TRANSLATE("Cancel"), new BMessage(kMsgRecordCancel));
		BButton* openButton = new BButton("haikode_record_open",
			B_TRANSLATE("Open"), new BMessage(kMsgRecordOpen));
		cancelButton->SetTarget(this);
		openButton->SetTarget(this);

		BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_DEFAULT_SPACING)
			.SetInsets(B_USE_WINDOW_SPACING)
			.Add(scroll)
			.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
				.AddGlue()
				.Add(cancelButton)
				.Add(openButton)
			.End();

		openButton->MakeDefault(true);
		if (GetLayout() != nullptr) {
			const BSize size = GetLayout()->MinSize();
			ResizeTo(size.width, size.height);
		}
		CenterOnScreen();
	}

	void MessageReceived(BMessage* message) override
	{
		switch (message->what) {
			case kMsgRecordPicked:
			case kMsgRecordOpen:
				_SendSelectionAndQuit();
				break;
			case kMsgRecordCancel:
				Quit();
				break;
			default:
				BWindow::MessageReceived(message);
				break;
		}
	}

private:
	void _SendSelectionAndQuit()
	{
		const int32 selection = fListView->CurrentSelection();
		ProjectRecordListItem* item = dynamic_cast<ProjectRecordListItem*>(
			fListView->ItemAt(selection));
		if (item == nullptr)
			return;

		BMessage picked(kMsgRecordPicked);
		picked.AddString("path", item->Path());
		fTarget.SendMessage(&picked);
		Quit();
	}

	BMessenger fTarget;
	BListView* fListView;
};


class AIProviderSetupWindow : public BWindow {
public:
	AIProviderSetupWindow(BMessenger target, const char* baseUrl,
		const char* model, const char* authMode, const char* apiKey,
		const char* oauthToken, const char* oauthAuthUrl,
		const char* oauthTokenUrl, const char* oauthClientId,
		const char* oauthScope, const char* oauthRedirectUri)
		:
		BWindow(BRect(100, 100, 780, 620), B_TRANSLATE("Haikode AI setup"),
			B_TITLED_WINDOW_LOOK, B_MODAL_APP_WINDOW_FEEL,
			B_ASYNCHRONOUS_CONTROLS | B_AUTO_UPDATE_SIZE_LIMITS
				| B_CLOSE_ON_ESCAPE),
		fTarget(target)
	{
		fBaseUrl = new BTextControl("setup_base_url", B_TRANSLATE("Base URL"),
			baseUrl != nullptr && baseUrl[0] != '\0' ? baseUrl
				: "https://api.openai.com",
			nullptr);
		fModel = new BTextControl("setup_model", B_TRANSLATE("Model"),
			model != nullptr && model[0] != '\0' ? model : "gpt-4.1-mini",
			nullptr);
		fAuthMode = new BTextControl("setup_auth_mode",
			B_TRANSLATE("Auth mode"),
			authMode != nullptr && authMode[0] != '\0' ? authMode : "api-key",
			nullptr);
		fApiKey = new BTextControl("setup_api_key", B_TRANSLATE("API key"),
			apiKey != nullptr ? apiKey : "", nullptr);
		fApiKey->TextView()->HideTyping(true);
		fOAuthToken = new BTextControl("setup_oauth_token",
			B_TRANSLATE("OAuth token"), oauthToken != nullptr ? oauthToken : "",
			nullptr);
		fOAuthToken->TextView()->HideTyping(true);
		fOAuthAuthUrl = new BTextControl("setup_oauth_auth_url",
			B_TRANSLATE("OAuth auth URL"),
			oauthAuthUrl != nullptr ? oauthAuthUrl : "", nullptr);
		fOAuthTokenUrl = new BTextControl("setup_oauth_token_url",
			B_TRANSLATE("OAuth token URL"),
			oauthTokenUrl != nullptr ? oauthTokenUrl : "", nullptr);
		fOAuthClientId = new BTextControl("setup_oauth_client_id",
			B_TRANSLATE("OAuth client ID"),
			oauthClientId != nullptr ? oauthClientId : "", nullptr);
		fOAuthScope = new BTextControl("setup_oauth_scope",
			B_TRANSLATE("OAuth scope"), oauthScope != nullptr ? oauthScope : "",
			nullptr);
		fOAuthRedirectUri = new BTextControl("setup_oauth_redirect_uri",
			B_TRANSLATE("OAuth redirect URI"),
			oauthRedirectUri != nullptr && oauthRedirectUri[0] != '\0'
				? oauthRedirectUri : "http://127.0.0.1:8765/callback",
			nullptr);

		BButton* openAIButton = new BButton("setup_openai",
			B_TRANSLATE("OpenAI"), new BMessage(kMsgSetupPresetOpenAI));
		BButton* ollamaButton = new BButton("setup_ollama",
			B_TRANSLATE("Ollama"), new BMessage(kMsgSetupPresetOllama));
		BButton* lmStudioButton = new BButton("setup_lmstudio",
			B_TRANSLATE("LM Studio"), new BMessage(kMsgSetupPresetLMStudio));
		BButton* openRouterButton = new BButton("setup_openrouter",
			B_TRANSLATE("OpenRouter"), new BMessage(kMsgSetupPresetOpenRouter));
		BButton* llamaCppButton = new BButton("setup_llamacpp",
			B_TRANSLATE("llama.cpp"), new BMessage(kMsgSetupPresetLlamaCpp));
		BButton* oauthButton = new BButton("setup_oauth",
			B_TRANSLATE("OAuth"), new BMessage(kMsgSetupPresetOAuth));
		BButton* cancelButton = new BButton("setup_cancel",
			B_TRANSLATE("Cancel"), new BMessage(kMsgSetupCancel));
		BButton* saveButton = new BButton("setup_save",
			B_TRANSLATE("Save"), new BMessage(kMsgSetupSave));
		BStringView* networkStatus = new BStringView("setup_network_status",
			SetupNetworkStatusText());
		openAIButton->SetTarget(this);
		ollamaButton->SetTarget(this);
		lmStudioButton->SetTarget(this);
		openRouterButton->SetTarget(this);
		llamaCppButton->SetTarget(this);
		oauthButton->SetTarget(this);
		cancelButton->SetTarget(this);
		saveButton->SetTarget(this);

		BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_DEFAULT_SPACING)
			.SetInsets(B_USE_WINDOW_SPACING)
			.Add(networkStatus)
			.AddGrid(B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING)
				.Add(fBaseUrl->CreateLabelLayoutItem(), 0, 0)
				.Add(fBaseUrl->CreateTextViewLayoutItem(), 1, 0)
				.Add(fModel->CreateLabelLayoutItem(), 0, 1)
				.Add(fModel->CreateTextViewLayoutItem(), 1, 1)
				.Add(fAuthMode->CreateLabelLayoutItem(), 0, 2)
				.Add(fAuthMode->CreateTextViewLayoutItem(), 1, 2)
				.Add(fApiKey->CreateLabelLayoutItem(), 0, 3)
				.Add(fApiKey->CreateTextViewLayoutItem(), 1, 3)
				.Add(fOAuthToken->CreateLabelLayoutItem(), 0, 4)
				.Add(fOAuthToken->CreateTextViewLayoutItem(), 1, 4)
				.Add(fOAuthAuthUrl->CreateLabelLayoutItem(), 0, 5)
				.Add(fOAuthAuthUrl->CreateTextViewLayoutItem(), 1, 5)
				.Add(fOAuthTokenUrl->CreateLabelLayoutItem(), 0, 6)
				.Add(fOAuthTokenUrl->CreateTextViewLayoutItem(), 1, 6)
				.Add(fOAuthClientId->CreateLabelLayoutItem(), 0, 7)
				.Add(fOAuthClientId->CreateTextViewLayoutItem(), 1, 7)
				.Add(fOAuthScope->CreateLabelLayoutItem(), 0, 8)
				.Add(fOAuthScope->CreateTextViewLayoutItem(), 1, 8)
				.Add(fOAuthRedirectUri->CreateLabelLayoutItem(), 0, 9)
				.Add(fOAuthRedirectUri->CreateTextViewLayoutItem(), 1, 9)
			.End()
			.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
				.Add(openAIButton)
				.Add(ollamaButton)
				.Add(lmStudioButton)
				.Add(openRouterButton)
				.Add(llamaCppButton)
				.Add(oauthButton)
				.AddGlue()
			.End()
			.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
				.AddGlue()
				.Add(cancelButton)
				.Add(saveButton)
			.End();

		saveButton->MakeDefault(true);
		if (GetLayout() != nullptr) {
			const BSize size = GetLayout()->MinSize();
			ResizeTo(size.width, size.height);
		}
		CenterOnScreen();
	}

	void MessageReceived(BMessage* message) override
	{
		switch (message->what) {
			case kMsgSetupPresetOpenAI:
				fBaseUrl->SetText("https://api.openai.com");
				fModel->SetText("gpt-4.1-mini");
				fAuthMode->SetText("api-key");
				break;
			case kMsgSetupPresetOllama:
				fBaseUrl->SetText("http://127.0.0.1:11434");
				fModel->SetText("llama3.1");
				fAuthMode->SetText("local");
				break;
			case kMsgSetupPresetLMStudio:
				fBaseUrl->SetText("http://127.0.0.1:1234");
				fModel->SetText("local-model");
				fAuthMode->SetText("local");
				break;
			case kMsgSetupPresetOpenRouter:
				fBaseUrl->SetText("https://openrouter.ai/api");
				fModel->SetText("openai/gpt-4.1-mini");
				fAuthMode->SetText("api-key");
				break;
			case kMsgSetupPresetLlamaCpp:
				fBaseUrl->SetText("http://127.0.0.1:8080");
				fModel->SetText("local-model");
				fAuthMode->SetText("local");
				break;
			case kMsgSetupPresetOAuth:
				fAuthMode->SetText("oauth");
				if (strlen(fOAuthRedirectUri->Text()) == 0)
					fOAuthRedirectUri->SetText("http://127.0.0.1:8765/callback");
				break;
			case kMsgSetupSave:
			{
				BMessage saved(kMsgSetupSaved);
				saved.AddString("base_url", fBaseUrl->Text());
				saved.AddString("model", fModel->Text());
				saved.AddString("auth_mode", fAuthMode->Text());
				saved.AddString("api_key", fApiKey->Text());
				saved.AddString("oauth_token", fOAuthToken->Text());
				saved.AddString("oauth_auth_url", fOAuthAuthUrl->Text());
				saved.AddString("oauth_token_url", fOAuthTokenUrl->Text());
				saved.AddString("oauth_client_id", fOAuthClientId->Text());
				saved.AddString("oauth_scope", fOAuthScope->Text());
				saved.AddString("oauth_redirect_uri", fOAuthRedirectUri->Text());
				fTarget.SendMessage(&saved);
				Quit();
				break;
			}
			case kMsgSetupCancel:
				Quit();
				break;
			default:
				BWindow::MessageReceived(message);
				break;
		}
	}

private:
	BMessenger fTarget;
	BTextControl* fBaseUrl;
	BTextControl* fModel;
	BTextControl* fAuthMode;
	BTextControl* fApiKey;
	BTextControl* fOAuthToken;
	BTextControl* fOAuthAuthUrl;
	BTextControl* fOAuthTokenUrl;
	BTextControl* fOAuthClientId;
	BTextControl* fOAuthScope;
	BTextControl* fOAuthRedirectUri;
};

} // namespace

AIChatPanel::AIChatPanel(PanelTabManager* panelTabManager, tab_id id)
	:
	BGroupView("Haikode AI", B_VERTICAL),
	fPanelTabManager(panelTabManager),
	fTabId(id),
	fBaseUrl(nullptr),
	fModel(nullptr),
	fAuthMode(nullptr),
	fApiKey(nullptr),
	fOAuthToken(nullptr),
	fOAuthAuthUrl(nullptr),
	fOAuthTokenUrl(nullptr),
	fOAuthClientId(nullptr),
	fOAuthScope(nullptr),
	fOAuthRedirectUri(nullptr),
	fOAuthCode(nullptr),
	fPrompt(nullptr),
	fProjectFilePath(nullptr),
	fPatchPath(nullptr),
	fPatchHunk(nullptr),
	fRecordPath(nullptr),
	fPendingActions(nullptr),
	fOutput(nullptr),
	fSaveProvider(nullptr),
	fSetupButton(nullptr),
	fOpenAIPresetButton(nullptr),
	fOllamaPresetButton(nullptr),
	fLMStudioPresetButton(nullptr),
	fOpenRouterPresetButton(nullptr),
	fLlamaCppPresetButton(nullptr),
	fTestProviderButton(nullptr),
	fStartOAuthButton(nullptr),
	fExchangeOAuthButton(nullptr),
	fCancelButton(nullptr),
	fAskButton(nullptr),
	fExplainButton(nullptr),
	fSummarizeButton(nullptr),
	fPatchButton(nullptr),
	fCodexStatusButton(nullptr),
	fCodexLoginButton(nullptr),
	fCodexAskButton(nullptr),
	fPreviousPatchFileButton(nullptr),
	fNextPatchFileButton(nullptr),
	fPreviousPatchHunkButton(nullptr),
	fNextPatchHunkButton(nullptr),
	fApplyFirstFileButton(nullptr),
	fRejectFirstFileButton(nullptr),
	fApplyHunkButton(nullptr),
	fRejectHunkButton(nullptr),
	fReviewPatchButton(nullptr),
	fApplyPatchButton(nullptr),
	fRejectPatchButton(nullptr),
	fRunCommandButton(nullptr),
	fRejectCommandButton(nullptr),
	fProjectFilesButton(nullptr),
	fOpenProjectFileButton(nullptr),
	fRecentRecordsButton(nullptr),
	fShowRecordButton(nullptr),
	fRequestRunning(false),
	fActiveRequestId(0),
	fActiveCancellation(nullptr)
{
	_BuildInterface();
}


void
AIChatPanel::AttachedToWindow()
{
	BGroupView::AttachedToWindow();
	fPrompt->SetTarget(this);
	fSaveProvider->SetTarget(this);
	fSetupButton->SetTarget(this);
	fOpenAIPresetButton->SetTarget(this);
	fOllamaPresetButton->SetTarget(this);
	fLMStudioPresetButton->SetTarget(this);
	fOpenRouterPresetButton->SetTarget(this);
	fLlamaCppPresetButton->SetTarget(this);
	fTestProviderButton->SetTarget(this);
	fStartOAuthButton->SetTarget(this);
	fExchangeOAuthButton->SetTarget(this);
	fCancelButton->SetTarget(this);
	fAskButton->SetTarget(this);
	fExplainButton->SetTarget(this);
	fSummarizeButton->SetTarget(this);
	fPatchButton->SetTarget(this);
	fCodexStatusButton->SetTarget(this);
	fCodexLoginButton->SetTarget(this);
	fCodexAskButton->SetTarget(this);
	fPreviousPatchFileButton->SetTarget(this);
	fNextPatchFileButton->SetTarget(this);
	fPreviousPatchHunkButton->SetTarget(this);
	fNextPatchHunkButton->SetTarget(this);
	fApplyFirstFileButton->SetTarget(this);
	fRejectFirstFileButton->SetTarget(this);
	fApplyHunkButton->SetTarget(this);
	fRejectHunkButton->SetTarget(this);
	fReviewPatchButton->SetTarget(this);
	fApplyPatchButton->SetTarget(this);
	fRejectPatchButton->SetTarget(this);
	fRunCommandButton->SetTarget(this);
	fRejectCommandButton->SetTarget(this);
	fProjectFilesButton->SetTarget(this);
	fOpenProjectFileButton->SetTarget(this);
	fRecentRecordsButton->SetTarget(this);
	fShowRecordButton->SetTarget(this);
	_LoadProviderFromConfig();
	be_app->StartWatching(this, gCFG.UpdateMessageWhat());
	SetTabLabel(B_TRANSLATE("Haikode AI"));
}


void
AIChatPanel::DetachedFromWindow()
{
	be_app->StopWatching(this, gCFG.UpdateMessageWhat());
	BGroupView::DetachedFromWindow();
}


void
AIChatPanel::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgSaveProvider:
			_SaveProviderToConfig();
			break;
		case kMsgOpenSetup:
			_OpenProviderSettings();
			break;
		case kMsgSetupSaved:
			fBaseUrl->SetText(message->GetString("base_url", ""));
			fModel->SetText(message->GetString("model", ""));
			fAuthMode->SetText(message->GetString("auth_mode", ""));
			fApiKey->SetText(message->GetString("api_key", ""));
			fOAuthToken->SetText(message->GetString("oauth_token", ""));
			fOAuthAuthUrl->SetText(message->GetString("oauth_auth_url", ""));
			fOAuthTokenUrl->SetText(message->GetString("oauth_token_url", ""));
			fOAuthClientId->SetText(message->GetString("oauth_client_id", ""));
			fOAuthScope->SetText(message->GetString("oauth_scope", ""));
			fOAuthRedirectUri->SetText(
				message->GetString("oauth_redirect_uri", ""));
			_SaveProviderToConfig();
			_AppendOutput(B_TRANSLATE("AI provider settings saved inside Haikode."));
			break;
		case kMsgPresetOpenAI:
			_ApplyProviderPreset(Haikode::AI::ProviderPreset::OpenAI);
			break;
		case kMsgPresetOllama:
			_ApplyProviderPreset(Haikode::AI::ProviderPreset::Ollama);
			break;
		case kMsgPresetLMStudio:
			_ApplyProviderPreset(Haikode::AI::ProviderPreset::LMStudio);
			break;
		case kMsgPresetOpenRouter:
			_ApplyProviderPreset(Haikode::AI::ProviderPreset::OpenRouter);
			break;
		case kMsgPresetLlamaCpp:
			_ApplyProviderPreset(Haikode::AI::ProviderPreset::LlamaCpp);
			break;
		case kMsgTestProvider:
			_TestProvider();
			break;
		case kMsgTestProviderResponse:
			if (_IsCurrentRequest(message, "provider test"))
				_FinishProviderTest(message->GetString("text", ""),
					message->GetString("error", ""),
					message->GetInt64("status", 0));
			break;
		case kMsgStartOAuth:
			_StartOAuth();
			break;
		case kMsgExchangeOAuth:
			_ExchangeOAuthCode();
			break;
		case kMsgAsk:
			_SendPrompt(Haikode::AI::PromptMode::Ask);
			break;
		case kMsgExplainSelection:
			_SendPrompt(Haikode::AI::PromptMode::ExplainSelection);
			break;
		case kMsgSummarizeProject:
			_SendPrompt(Haikode::AI::PromptMode::SummarizeProject);
			break;
		case kMsgProposePatch:
			_SendPrompt(Haikode::AI::PromptMode::ProposePatch);
			break;
		case kMsgCodexStatus:
			_QueueCodexLoginStatus();
			break;
		case kMsgCodexLogin:
			_QueueCodexDeviceLogin();
			break;
		case kMsgCodexAsk:
			_QueueCodexReadOnlyAsk();
			break;
		case kMsgCodexCaptureResponse:
			if (_IsCurrentRequest(message, "Codex response"))
				_FinishCodexCapture(message->GetString("output", ""),
					message->GetString("error", ""),
					message->GetInt32("exit_code", -1),
					message->GetBool("timed_out", false),
					message->GetBool("cancelled", false),
					message->GetString("log_path", ""),
					message->GetString("log_error", ""));
			break;
		case kMsgCommandCaptureResponse:
			if (_IsCurrentRequest(message, "command response"))
				_FinishCommandCapture(message->GetString("output", ""),
					message->GetString("error", ""),
					message->GetInt32("exit_code", -1),
					message->GetBool("timed_out", false),
					message->GetBool("cancelled", false),
					message->GetString("log_path", ""),
					message->GetString("log_error", ""));
			break;
		case kMsgAIResponse:
		{
			if (_IsCurrentRequest(message, "AI response"))
				_FinishResponse(message->GetString("text", ""),
					message->GetString("error", ""),
					message->GetInt64("status", 0));
			break;
		}
		case kMsgPreviousPatchFile:
			_SelectPatchFile(-1);
			break;
		case kMsgNextPatchFile:
			_SelectPatchFile(1);
			break;
		case kMsgPreviousPatchHunk:
			_SelectPatchHunk(-1);
			break;
		case kMsgNextPatchHunk:
			_SelectPatchHunk(1);
			break;
		case kMsgOAuthResponse:
		{
			if (_IsCurrentRequest(message, "OAuth response"))
				_FinishOAuthExchange(message->GetString("token", ""),
					message->GetString("error", ""),
					message->GetInt64("status", 0));
			break;
		}
		case kMsgCancelRequest:
			_CancelRequest();
			break;
		case kMsgOAuthCallback:
		{
			const BString callback(message->GetString("callback", ""));
			const BString error(message->GetString("error", ""));
			if (!callback.IsEmpty()) {
				fOAuthCode->SetText(callback.String());
				_AppendOutput(B_TRANSLATE("OAuth callback received. Exchanging authorization code."));
				_ExchangeOAuthCode();
			} else if (!error.IsEmpty()) {
				BString line(B_TRANSLATE("OAuth callback listener: "));
				line << error;
				_AppendOutput(line.String());
			}
			break;
		}
		case kMsgApplyFirstFile:
			_ApplyFirstPendingFile();
			break;
		case kMsgRejectFirstFile:
			_RejectFirstPendingFile();
			break;
		case kMsgApplyHunk:
			_ApplySelectedPendingHunk();
			break;
		case kMsgRejectHunk:
			_RejectSelectedPendingHunk();
			break;
		case kMsgReviewPatch:
			if (fPendingDiff.IsEmpty() || fPendingRawDiff.IsEmpty())
				_AppendOutput(B_TRANSLATE("No pending patch to review."));
			else if (fPatchPath != nullptr
				&& !BString(fPatchPath->Text()).IsEmpty()
				&& fPendingDiff.ReviewTextForFile(fPatchPath->Text()).empty()) {
				BString line(B_TRANSLATE("Pending patch does not contain selected file: "));
				line << fPatchPath->Text();
				_AppendOutput(line.String());
			} else
				_SendPrompt(Haikode::AI::PromptMode::ReviewDiff);
			break;
		case kMsgApplyPatch:
			_ApplyPendingDiff();
			break;
		case kMsgRejectPatch:
			_RejectPendingDiff();
			_AppendOutput(B_TRANSLATE("Pending patch rejected."));
			break;
		case kMsgRunCommand:
			_RunPendingCommand();
			break;
		case kMsgRejectCommand:
			_RejectPendingCommand();
			break;
		case kMsgListProjectFiles:
			_ListProjectFiles();
			break;
		case kMsgOpenProjectFile:
			_OpenSelectedProjectFile();
			break;
		case kMsgProjectFilePicked:
			fProjectFilePath->SetText(message->GetString("path", ""));
			_OpenSelectedProjectFile();
			break;
		case kMsgListRecords:
			_ListRecentProjectRecords();
			break;
		case kMsgShowRecord:
			_ShowSelectedProjectRecord();
			break;
		case kMsgRecordPicked:
			fRecordPath->SetText(message->GetString("path", ""));
			_ShowSelectedProjectRecord();
			break;
		case B_OBSERVER_NOTICE_CHANGE:
		{
			int32 code;
			if (message->FindInt32(B_OBSERVE_WHAT_CHANGE, &code) == B_OK
				&& code == gCFG.UpdateMessageWhat()) {
				BString key(message->GetString("key", ""));
				if (key.StartsWith("haikode_ai_"))
					_LoadProviderFromConfig();
			}
			break;
		}
		default:
			BGroupView::MessageReceived(message);
			break;
	}
}


void
AIChatPanel::SetActiveContext(const BString& projectRoot, const BString& filePath,
	const BString& selection, const BString& fileText,
	const std::vector<Haikode::AI::ContextFile>& openFiles)
{
	fProjectRoot = projectRoot;
	fFilePath = filePath;
	fSelection = selection;
	fFileText = fileText;
	fOpenFiles = openFiles;
}


void
AIChatPanel::SetTabLabel(BString label)
{
	if (fPanelTabManager != nullptr)
		fPanelTabManager->SetLabelForTab(fTabId, label);
}


void
AIChatPanel::OpenProviderSettings()
{
	_OpenProviderSettings();
}


void
AIChatPanel::_BuildInterface()
{
	fBaseUrl = new BTextControl("haikode_ai_base_url",
		B_TRANSLATE("Base URL"), "", nullptr);
	fModel = new BTextControl("haikode_ai_model", B_TRANSLATE("Model"), "",
		nullptr);
	fAuthMode = new BTextControl("haikode_ai_auth_mode",
		B_TRANSLATE("Auth"), "", nullptr);
	fApiKey = new BTextControl("haikode_ai_api_key", B_TRANSLATE("API key"), "",
		nullptr);
	fApiKey->TextView()->HideTyping(true);
	fOAuthToken = new BTextControl("haikode_ai_oauth_token",
		B_TRANSLATE("OAuth token"), "", nullptr);
	fOAuthToken->TextView()->HideTyping(true);
	fOAuthAuthUrl = new BTextControl("haikode_ai_oauth_auth_url",
		B_TRANSLATE("OAuth auth URL"), "", nullptr);
	fOAuthTokenUrl = new BTextControl("haikode_ai_oauth_token_url",
		B_TRANSLATE("OAuth token URL"), "", nullptr);
	fOAuthClientId = new BTextControl("haikode_ai_oauth_client_id",
		B_TRANSLATE("OAuth client ID"), "", nullptr);
	fOAuthScope = new BTextControl("haikode_ai_oauth_scope",
		B_TRANSLATE("OAuth scope"), "", nullptr);
	fOAuthRedirectUri = new BTextControl("haikode_ai_oauth_redirect_uri",
		B_TRANSLATE("OAuth redirect URI"), "", nullptr);
	fOAuthCode = new BTextControl("haikode_ai_oauth_code",
		B_TRANSLATE("OAuth code"), "", nullptr);
	fPrompt = new BTextControl("haikode_ai_prompt", B_TRANSLATE("Prompt"), "",
		new BMessage(kMsgAsk));
	fProjectFilePath = new BTextControl("haikode_ai_project_file",
		B_TRANSLATE("File"), "", nullptr);
	fPatchPath = new BTextControl("haikode_ai_patch_path",
		B_TRANSLATE("Patch file"), "", nullptr);
	fPatchPath->SetEnabled(false);
	fPatchHunk = new BTextControl("haikode_ai_patch_hunk",
		B_TRANSLATE("Hunk"), "1", nullptr);
	fPatchHunk->SetEnabled(false);
	fRecordPath = new BTextControl("haikode_ai_record_path",
		B_TRANSLATE("Record"), "", nullptr);

	fSaveProvider = new BButton("haikode_ai_save_provider",
		B_TRANSLATE("Save provider"), new BMessage(kMsgSaveProvider));
	fSetupButton = new BButton("haikode_ai_setup",
		B_TRANSLATE("AI Setup"), new BMessage(kMsgOpenSetup));
	fOpenAIPresetButton = new BButton("haikode_ai_preset_openai",
		B_TRANSLATE("OpenAI"), new BMessage(kMsgPresetOpenAI));
	fOllamaPresetButton = new BButton("haikode_ai_preset_ollama",
		B_TRANSLATE("Ollama"), new BMessage(kMsgPresetOllama));
	fLMStudioPresetButton = new BButton("haikode_ai_preset_lmstudio",
		B_TRANSLATE("LM Studio"), new BMessage(kMsgPresetLMStudio));
	fOpenRouterPresetButton = new BButton("haikode_ai_preset_openrouter",
		B_TRANSLATE("OpenRouter"), new BMessage(kMsgPresetOpenRouter));
	fLlamaCppPresetButton = new BButton("haikode_ai_preset_llamacpp",
		B_TRANSLATE("llama.cpp"), new BMessage(kMsgPresetLlamaCpp));
	fTestProviderButton = new BButton("haikode_ai_test_provider",
		B_TRANSLATE("Test provider"), new BMessage(kMsgTestProvider));
	fStartOAuthButton = new BButton("haikode_ai_start_oauth",
		B_TRANSLATE("Start OAuth"), new BMessage(kMsgStartOAuth));
	fExchangeOAuthButton = new BButton("haikode_ai_exchange_oauth",
		B_TRANSLATE("Exchange code"), new BMessage(kMsgExchangeOAuth));
	fCancelButton = new BButton("haikode_ai_cancel_request",
		B_TRANSLATE("Stop"), new BMessage(kMsgCancelRequest));
	fCancelButton->SetEnabled(false);
	fAskButton = new BButton("haikode_ai_ask", B_TRANSLATE("Ask"),
		new BMessage(kMsgAsk));
	fExplainButton = new BButton("haikode_ai_explain",
		B_TRANSLATE("Explain file"), new BMessage(kMsgExplainSelection));
	fSummarizeButton = new BButton("haikode_ai_summarize_project",
		B_TRANSLATE("Summarize project"), new BMessage(kMsgSummarizeProject));
	fPatchButton = new BButton("haikode_ai_patch", B_TRANSLATE("Propose patch"),
		new BMessage(kMsgProposePatch));
	fCodexStatusButton = new BButton("haikode_ai_codex_status",
		B_TRANSLATE("Codex status"), new BMessage(kMsgCodexStatus));
	fCodexLoginButton = new BButton("haikode_ai_codex_login",
		B_TRANSLATE("Codex login"), new BMessage(kMsgCodexLogin));
	fCodexAskButton = new BButton("haikode_ai_codex_ask",
		B_TRANSLATE("Ask Codex"), new BMessage(kMsgCodexAsk));
	fPreviousPatchFileButton = new BButton("haikode_ai_previous_patch_file",
		B_TRANSLATE("Previous file"), new BMessage(kMsgPreviousPatchFile));
	fPreviousPatchFileButton->SetEnabled(false);
	fNextPatchFileButton = new BButton("haikode_ai_next_patch_file",
		B_TRANSLATE("Next file"), new BMessage(kMsgNextPatchFile));
	fNextPatchFileButton->SetEnabled(false);
	fPreviousPatchHunkButton = new BButton("haikode_ai_previous_patch_hunk",
		B_TRANSLATE("Previous hunk"), new BMessage(kMsgPreviousPatchHunk));
	fPreviousPatchHunkButton->SetEnabled(false);
	fNextPatchHunkButton = new BButton("haikode_ai_next_patch_hunk",
		B_TRANSLATE("Next hunk"), new BMessage(kMsgNextPatchHunk));
	fNextPatchHunkButton->SetEnabled(false);
	fApplyFirstFileButton = new BButton("haikode_ai_apply_first_file",
		B_TRANSLATE("Apply selected file"), new BMessage(kMsgApplyFirstFile));
	fApplyFirstFileButton->SetEnabled(false);
	fRejectFirstFileButton = new BButton("haikode_ai_reject_first_file",
		B_TRANSLATE("Reject selected file"), new BMessage(kMsgRejectFirstFile));
	fRejectFirstFileButton->SetEnabled(false);
	fApplyHunkButton = new BButton("haikode_ai_apply_hunk",
		B_TRANSLATE("Apply hunk"), new BMessage(kMsgApplyHunk));
	fApplyHunkButton->SetEnabled(false);
	fRejectHunkButton = new BButton("haikode_ai_reject_hunk",
		B_TRANSLATE("Reject hunk"), new BMessage(kMsgRejectHunk));
	fRejectHunkButton->SetEnabled(false);
	fReviewPatchButton = new BButton("haikode_ai_review_patch",
		B_TRANSLATE("Review patch"), new BMessage(kMsgReviewPatch));
	fReviewPatchButton->SetEnabled(false);
	fApplyPatchButton = new BButton("haikode_ai_apply_patch",
		B_TRANSLATE("Apply patch"), new BMessage(kMsgApplyPatch));
	fApplyPatchButton->SetEnabled(false);
	fRejectPatchButton = new BButton("haikode_ai_reject_patch",
		B_TRANSLATE("Reject patch"), new BMessage(kMsgRejectPatch));
	fRejectPatchButton->SetEnabled(false);
	fRunCommandButton = new BButton("haikode_ai_run_command",
		B_TRANSLATE("Run command"), new BMessage(kMsgRunCommand));
	fRunCommandButton->SetEnabled(false);
	fRejectCommandButton = new BButton("haikode_ai_reject_command",
		B_TRANSLATE("Reject command"), new BMessage(kMsgRejectCommand));
	fRejectCommandButton->SetEnabled(false);
	fProjectFilesButton = new BButton("haikode_ai_project_files",
		B_TRANSLATE("Project files"), new BMessage(kMsgListProjectFiles));
	fOpenProjectFileButton = new BButton("haikode_ai_open_project_file",
		B_TRANSLATE("Open file"), new BMessage(kMsgOpenProjectFile));
	fRecentRecordsButton = new BButton("haikode_ai_recent_records",
		B_TRANSLATE("Recent records"), new BMessage(kMsgListRecords));
	fShowRecordButton = new BButton("haikode_ai_show_record",
		B_TRANSLATE("Show record"), new BMessage(kMsgShowRecord));

	fPendingActions = new BTextView("haikode_ai_pending_actions");
	fPendingActions->MakeEditable(false);
	fPendingActions->SetText(B_TRANSLATE("No pending AI actions."));
	BScrollView* pendingScroll = new BScrollView(
		"haikode_ai_pending_actions_scroll", fPendingActions, B_FOLLOW_LEFT_RIGHT,
		0, true, true);
	pendingScroll->SetExplicitMinSize(BSize(B_SIZE_UNSET, 90));

	fOutput = new BTextView("haikode_ai_output");
	fOutput->MakeEditable(false);
	fOutput->SetText(InitialAIStatusText());
	BScrollView* outputScroll = new BScrollView("haikode_ai_output_scroll",
		fOutput, B_FOLLOW_ALL, 0, true, true);

	BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_DEFAULT_SPACING)
		.SetInsets(B_USE_WINDOW_SPACING)
		.AddGrid(B_USE_DEFAULT_SPACING, B_USE_DEFAULT_SPACING)
			.Add(fBaseUrl->CreateLabelLayoutItem(), 0, 0)
			.Add(fBaseUrl->CreateTextViewLayoutItem(), 1, 0)
			.Add(fModel->CreateLabelLayoutItem(), 2, 0)
			.Add(fModel->CreateTextViewLayoutItem(), 3, 0)
			.Add(fAuthMode->CreateLabelLayoutItem(), 0, 1)
			.Add(fAuthMode->CreateTextViewLayoutItem(), 1, 1)
			.Add(fApiKey->CreateLabelLayoutItem(), 2, 1)
			.Add(fApiKey->CreateTextViewLayoutItem(), 3, 1)
			.Add(fOAuthToken->CreateLabelLayoutItem(), 0, 2)
			.Add(fOAuthToken->CreateTextViewLayoutItem(), 1, 2, 3)
			.Add(fOAuthAuthUrl->CreateLabelLayoutItem(), 0, 3)
			.Add(fOAuthAuthUrl->CreateTextViewLayoutItem(), 1, 3, 3)
			.Add(fOAuthTokenUrl->CreateLabelLayoutItem(), 0, 4)
			.Add(fOAuthTokenUrl->CreateTextViewLayoutItem(), 1, 4, 3)
			.Add(fOAuthClientId->CreateLabelLayoutItem(), 0, 5)
			.Add(fOAuthClientId->CreateTextViewLayoutItem(), 1, 5)
			.Add(fOAuthScope->CreateLabelLayoutItem(), 2, 5)
			.Add(fOAuthScope->CreateTextViewLayoutItem(), 3, 5)
			.Add(fOAuthRedirectUri->CreateLabelLayoutItem(), 0, 6)
			.Add(fOAuthRedirectUri->CreateTextViewLayoutItem(), 1, 6)
			.Add(fOAuthCode->CreateLabelLayoutItem(), 2, 6)
			.Add(fOAuthCode->CreateTextViewLayoutItem(), 3, 6)
		.End()
		.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
			.Add(fOpenAIPresetButton)
			.Add(fOllamaPresetButton)
			.Add(fLMStudioPresetButton)
			.Add(fOpenRouterPresetButton)
			.Add(fLlamaCppPresetButton)
			.AddGlue()
		.End()
		.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
			.Add(fPrompt)
			.Add(fSetupButton)
			.Add(fSaveProvider)
			.Add(fTestProviderButton)
			.Add(fStartOAuthButton)
			.Add(fExchangeOAuthButton)
			.Add(fCancelButton)
			.Add(fAskButton)
			.Add(fExplainButton)
			.Add(fSummarizeButton)
			.Add(fPatchButton)
		.End()
		.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
			.Add(fCodexStatusButton)
			.Add(fCodexLoginButton)
			.Add(fCodexAskButton)
			.AddGlue()
		.End()
		.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
			.Add(fPatchPath)
			.Add(fPatchHunk)
			.Add(fPreviousPatchFileButton)
			.Add(fNextPatchFileButton)
			.Add(fPreviousPatchHunkButton)
			.Add(fNextPatchHunkButton)
			.Add(fApplyFirstFileButton)
			.Add(fRejectFirstFileButton)
			.Add(fApplyHunkButton)
			.Add(fRejectHunkButton)
			.Add(fReviewPatchButton)
			.Add(fApplyPatchButton)
			.Add(fRejectPatchButton)
			.Add(fRunCommandButton)
			.Add(fRejectCommandButton)
		.End()
		.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
			.Add(fProjectFilePath)
			.Add(fProjectFilesButton)
			.Add(fOpenProjectFileButton)
		.End()
		.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
			.Add(fRecordPath)
			.Add(fRecentRecordsButton)
			.Add(fShowRecordButton)
		.End()
		.Add(pendingScroll)
		.Add(outputScroll);
}


void
AIChatPanel::_LoadProviderFromConfig()
{
	fBaseUrl->SetText(BString(gCFG["haikode_ai_base_url"]).String());
	fModel->SetText(BString(gCFG["haikode_ai_model"]).String());
	fAuthMode->SetText(BString(gCFG["haikode_ai_auth_mode"]).String());
	fApiKey->SetText(BString(gCFG["haikode_ai_api_key"]).String());
	fOAuthToken->SetText(BString(gCFG["haikode_ai_oauth_token"]).String());
	fOAuthAuthUrl->SetText(
		BString(gCFG["haikode_ai_oauth_auth_url"]).String());
	fOAuthTokenUrl->SetText(
		BString(gCFG["haikode_ai_oauth_token_url"]).String());
	fOAuthClientId->SetText(
		BString(gCFG["haikode_ai_oauth_client_id"]).String());
	fOAuthScope->SetText(BString(gCFG["haikode_ai_oauth_scope"]).String());
	fOAuthRedirectUri->SetText(
		BString(gCFG["haikode_ai_oauth_redirect_uri"]).String());
}


void
AIChatPanel::_SaveProviderToConfig()
{
	gCFG["haikode_ai_base_url"] = fBaseUrl->Text();
	gCFG["haikode_ai_model"] = fModel->Text();
	gCFG["haikode_ai_auth_mode"] = fAuthMode->Text();
	gCFG["haikode_ai_api_key"] = fApiKey->Text();
	gCFG["haikode_ai_oauth_token"] = fOAuthToken->Text();
	gCFG["haikode_ai_oauth_auth_url"] = fOAuthAuthUrl->Text();
	gCFG["haikode_ai_oauth_token_url"] = fOAuthTokenUrl->Text();
	gCFG["haikode_ai_oauth_client_id"] = fOAuthClientId->Text();
	gCFG["haikode_ai_oauth_scope"] = fOAuthScope->Text();
	gCFG["haikode_ai_oauth_redirect_uri"] = fOAuthRedirectUri->Text();

	_AppendOutput(B_TRANSLATE("Provider settings saved inside Haikode."));
#ifndef HAIKODE_AI_NETWORK
	_AppendOutput(B_TRANSLATE("This binary was built without network AI support; rebuild with HAIKODE_AI_NETWORK=1 to send requests."));
#endif
}


void
AIChatPanel::_ApplyProviderPreset(Haikode::AI::ProviderPreset preset)
{
	const Haikode::AI::ProviderSettings settings
		= Haikode::AI::ProviderPresetSettings(preset);
	fBaseUrl->SetText(settings.baseUrl.c_str());
	fModel->SetText(settings.model.c_str());
	fAuthMode->SetText(Haikode::AI::ToString(settings.authMode));
	_SaveProviderToConfig();

	BString line(B_TRANSLATE("Applied provider preset: "));
	line << settings.name.c_str();
	_AppendOutput(line.String());
}


void
AIChatPanel::_OpenProviderSettings()
{
	BLooper* looper = Window();
	if (looper == nullptr)
		looper = Looper();
	status_t messengerStatus = B_OK;
	BMessenger target(this, looper, &messengerStatus);
	if (messengerStatus != B_OK) {
		BString text(B_TRANSLATE("Haikode could not open AI Setup because the AI panel is not ready yet."));
		text << "\n\n" << B_TRANSLATE("Open the Haikode AI panel, then click AI Setup again.");
		(new BAlert("Haikode AI setup", text.String(), B_TRANSLATE("OK"),
			nullptr, nullptr, B_WIDTH_AS_USUAL, B_WARNING_ALERT))->Go();
		return;
	}

	AIProviderSetupWindow* window = new AIProviderSetupWindow(target,
		fBaseUrl->Text(), fModel->Text(), fAuthMode->Text(), fApiKey->Text(),
		fOAuthToken->Text(), fOAuthAuthUrl->Text(), fOAuthTokenUrl->Text(),
		fOAuthClientId->Text(), fOAuthScope->Text(), fOAuthRedirectUri->Text());
	window->Show();
	window->Activate(true);
	_AppendOutput(B_TRANSLATE("Opened Haikode AI setup. Paste an API key or configure OAuth there; no Terminal export is required."));
}


void
AIChatPanel::_TestProvider()
{
	if (fRequestRunning) {
		_AppendOutput(B_TRANSLATE("An AI or OAuth request is already running."));
		return;
	}

	_SaveProviderToConfig();
	Haikode::AI::ProviderSettings provider = _ProviderFromFields();
	std::string validationError;
	if (!provider.Validate(validationError)) {
		_AppendOutput(validationError.c_str());
		_AppendOutput(B_TRANSLATE("Click AI Setup to configure the provider inside Haikode. No Terminal export is required."));
		_OpenProviderSettings();
		return;
	}

	const int64 requestId = _BeginRequest();
	std::shared_ptr<Haikode::AI::CancellationToken> cancellation
		= fActiveCancellation;

	BString line(B_TRANSLATE("Testing provider endpoint: "));
	line << provider.ChatCompletionsEndpoint().c_str();
	_AppendOutput(line.String());

	BMessenger messenger(this);
	std::thread([messenger, provider, requestId, cancellation]() mutable {
		Haikode::AI::OpenAICompatibleClient client;
		Haikode::AI::ChatRequest request;
		request.prompt = "Reply with exactly: Haikode provider OK";
		request.maxTokens = 32;
		Haikode::AI::ChatResponse response;
		std::string error;
		const bool ok = client.Send(provider, request, response, error,
			cancellation.get());

		BMessage done(kMsgTestProviderResponse);
		done.AddInt64("request_id", requestId);
		done.AddString("text", ok ? response.text.c_str() : "");
		done.AddString("error", error.c_str());
		done.AddInt64("status", response.httpStatus);
		messenger.SendMessage(&done);
	}).detach();
}


int64
AIChatPanel::_BeginRequest()
{
	fRequestRunning = true;
	fActiveRequestId++;
	fActiveCancellation
		= std::make_shared<Haikode::AI::CancellationToken>();
	_SetRequestControlsEnabled(false);
	return fActiveRequestId;
}


bool
AIChatPanel::_IsCurrentRequest(BMessage* message, const char* label)
{
	const int64 requestId = message->GetInt64("request_id", fActiveRequestId);
	if (requestId == fActiveRequestId)
		return true;

	BString line(B_TRANSLATE("Ignored stale "));
	line << (label == nullptr ? "AI request" : label) << ".";
	_AppendOutput(line.String());
	return false;
}


void
AIChatPanel::_CancelRequest()
{
	if (!fRequestRunning) {
		_AppendOutput(B_TRANSLATE("No AI request is running."));
		return;
	}

	fActiveRequestId++;
	if (fActiveCancellation != nullptr)
		fActiveCancellation->Cancel();
	_FinishRequest();
	_AppendOutput(B_TRANSLATE("AI request stopped. Network work is being cancelled and any late provider response will be ignored."));
}


void
AIChatPanel::_FinishRequest()
{
	fRequestRunning = false;
	fActiveCancellation.reset();
	_SetRequestControlsEnabled(true);
}


void
AIChatPanel::_SetRequestControlsEnabled(bool enabled)
{
	fAskButton->SetEnabled(enabled);
	fExplainButton->SetEnabled(enabled);
	fSummarizeButton->SetEnabled(enabled);
	fPatchButton->SetEnabled(enabled);
	fSaveProvider->SetEnabled(enabled);
	fSetupButton->SetEnabled(enabled);
	fTestProviderButton->SetEnabled(enabled);
	fOpenAIPresetButton->SetEnabled(enabled);
	fOllamaPresetButton->SetEnabled(enabled);
	fLMStudioPresetButton->SetEnabled(enabled);
	fOpenRouterPresetButton->SetEnabled(enabled);
	fLlamaCppPresetButton->SetEnabled(enabled);
	fStartOAuthButton->SetEnabled(enabled);
	fExchangeOAuthButton->SetEnabled(enabled);
	fCodexStatusButton->SetEnabled(enabled);
	fCodexLoginButton->SetEnabled(enabled);
	fCodexAskButton->SetEnabled(enabled);
	fProjectFilesButton->SetEnabled(enabled);
	fOpenProjectFileButton->SetEnabled(enabled);
	fRecentRecordsButton->SetEnabled(enabled);
	fShowRecordButton->SetEnabled(enabled);
	fRunCommandButton->SetEnabled(enabled && !fPendingCommands.empty());
	fRejectCommandButton->SetEnabled(enabled && !fPendingCommands.empty());
	fCancelButton->SetEnabled(!enabled);
}


void
AIChatPanel::_FinishProviderTest(const BString& text, const BString& error,
	long status)
{
	_FinishRequest();

	if (!error.IsEmpty()) {
		BString line(B_TRANSLATE("Provider test failed"));
		if (status != 0)
			line << " (" << status << ")";
		line << ": " << error;
		_AppendOutput(line.String());
		return;
	}

	BString line(B_TRANSLATE("Provider test succeeded: "));
	line << text;
	_AppendOutput(line.String());
}


void
AIChatPanel::_StartOAuth()
{
	_SaveProviderToConfig();
	Haikode::AI::OAuthSettings settings = _OAuthSettingsFromFields();
	if (settings.authUrl.empty() || settings.tokenUrl.empty()
		|| settings.clientId.empty()
		|| settings.redirectUri.empty()) {
		_AppendOutput(B_TRANSLATE("OAuth auth URL, token URL, client ID, and redirect URI are required."));
		return;
	}

	const std::string verifier = Haikode::AI::OAuthClient::GenerateVerifier();
	gCFG["haikode_ai_oauth_verifier"] = verifier.c_str();
	fAuthMode->SetText("oauth");
	gCFG["haikode_ai_auth_mode"] = "oauth";

	const std::string authUrl = Haikode::AI::OAuthClient::BuildAuthUrl(
		settings, verifier, "haikode");

#ifdef HAIKODE_AI_NETWORK
	BMessenger callbackMessenger(this);
	const std::string redirectUri = settings.redirectUri;
	std::thread([callbackMessenger, redirectUri]() mutable {
		std::string callback;
		std::string error;
		const bool ok = WaitForOAuthCallback(redirectUri, callback, error);
		BMessage message(kMsgOAuthCallback);
		message.AddString("callback", ok ? callback.c_str() : "");
		message.AddString("error", ok ? "" : error.c_str());
		callbackMessenger.SendMessage(&message);
	}).detach();
	_AppendOutput(B_TRANSLATE("Waiting for OAuth browser callback on localhost. Paste-code fallback remains available."));
#else
	_AppendOutput(B_TRANSLATE("This build cannot listen for OAuth browser callbacks; paste the returned code manually."));
#endif

	const char* argv[2] = {authUrl.c_str(), nullptr};
	status_t status = be_roster->Launch("text/html", 1, argv);
	if (status != B_OK) {
		BString line(B_TRANSLATE("Could not open OAuth URL in browser: "));
		line << strerror(status);
		_AppendOutput(line.String());
	}

	_AppendOutput(B_TRANSLATE("OAuth login URL generated. Complete login in the browser. If automatic callback capture does not finish, paste the returned authorization code into OAuth code and click Exchange code."));
	_AppendOutput(authUrl.c_str());
}


void
AIChatPanel::_ExchangeOAuthCode()
{
	if (fRequestRunning) {
		_AppendOutput(B_TRANSLATE("An AI or OAuth request is already running."));
		return;
	}

	_SaveProviderToConfig();
	Haikode::AI::OAuthSettings settings = _OAuthSettingsFromFields();
	std::string code;
	std::string codeError;
	if (!Haikode::AI::OAuthClient::ExtractAuthorizationCode(fOAuthCode->Text(),
			code, codeError)) {
		_AppendOutput(codeError.c_str());
		return;
	}
	const std::string verifier = BString(gCFG["haikode_ai_oauth_verifier"]).String();

	const int64 requestId = _BeginRequest();
	std::shared_ptr<Haikode::AI::CancellationToken> cancellation
		= fActiveCancellation;

	BMessenger messenger(this);
	std::thread([messenger, settings, code, verifier, requestId,
		cancellation]() mutable {
		Haikode::AI::OAuthClient client;
		Haikode::AI::OAuthTokenResponse response;
		std::string error;
		const bool ok = client.ExchangeCode(settings, code, verifier, response,
			error, cancellation.get());

		BMessage done(kMsgOAuthResponse);
		done.AddInt64("request_id", requestId);
		done.AddString("token", ok ? response.accessToken.c_str() : "");
		done.AddString("error", error.c_str());
		done.AddInt64("status", response.httpStatus);
		messenger.SendMessage(&done);
	}).detach();
}


void
AIChatPanel::_FinishOAuthExchange(const BString& token, const BString& error,
	long status)
{
	_FinishRequest();

	if (!error.IsEmpty()) {
		BString line(B_TRANSLATE("OAuth token exchange failed"));
		if (status != 0)
			line << " (" << status << ")";
		line << ": " << error;
		_AppendOutput(line.String());
		return;
	}

	fAuthMode->SetText("oauth");
	fOAuthToken->SetText(token.String());
	gCFG["haikode_ai_auth_mode"] = "oauth";
	gCFG["haikode_ai_oauth_token"] = token.String();
	_AppendOutput(B_TRANSLATE("OAuth token saved. Haikode will use oauth bearer auth for AI requests."));
}


void
AIChatPanel::_SendPrompt(Haikode::AI::PromptMode mode)
{
	if (fRequestRunning) {
		_AppendOutput(B_TRANSLATE("An AI request is already running."));
		return;
	}

	Haikode::AI::VibeCodingRequest request = _RequestFromContext(mode);
	Haikode::AI::PromptBuilder builder;
	Haikode::AI::PromptBuildResult result = builder.Build(request, 200 * 1024,
		10);
	_SaveProviderToConfig();
	Haikode::AI::ProviderSettings provider = _ProviderFromFields();

	fOutput->SetText("");
	_AppendOutput(B_TRANSLATE("Sending prompt to configured provider."));
	_AppendOutput("");

	BString providerLine;
	providerLine << B_TRANSLATE("Provider endpoint: ")
		<< provider.ChatCompletionsEndpoint().c_str();
	_AppendOutput(providerLine.String());

	BString credentialLine;
	credentialLine << B_TRANSLATE("Credential mode: ")
		<< Haikode::AI::ToString(provider.authMode)
		<< (provider.HasUsableCredentials() ? " ready" : " missing");
	_AppendOutput(credentialLine.String());

	BString contextLine(B_TRANSLATE("Project-map context: "));
	contextLine << request.projectFiles.size();
	if (request.projectMapCandidateCount > 0)
		contextLine << "/" << request.projectMapCandidateCount;
	contextLine << B_TRANSLATE(" file(s).");
	_AppendOutput(contextLine.String());

	for (const std::string& warning : result.warnings)
		_AppendOutput(warning.c_str());

	_AppendOutput("");

	std::string validationError;
	if (!provider.Validate(validationError)) {
		_AppendOutput(validationError.c_str());
		_AppendOutput(B_TRANSLATE("Click AI Setup to configure the provider inside Haikode. No Terminal export is required."));
		_OpenProviderSettings();
		return;
	}

	fLastUserPrompt = fPrompt->Text();
	fLastProvider = provider;
	const int64 requestId = _BeginRequest();
	std::shared_ptr<Haikode::AI::CancellationToken> cancellation
		= fActiveCancellation;
	_SetPatchControlsEnabled(false);
	const bool reviewingPatch = mode == Haikode::AI::PromptMode::ReviewDiff;
	if (!reviewingPatch) {
		fPendingDiff = Haikode::AI::UnifiedDiff();
		fPendingRawDiff = "";
		fSavedPendingPatchPath = "";
		fPatchPath->SetText("");
	}
	_ClearPendingCommands();
	_UpdatePendingActions();

	BMessenger messenger(this);
	std::string prompt = result.prompt;
	std::thread([messenger, provider, prompt, requestId, cancellation]() mutable {
		Haikode::AI::OpenAICompatibleClient client;
		Haikode::AI::ChatRequest request;
		request.prompt = prompt;
		Haikode::AI::ChatResponse response;
		std::string error;
		const bool ok = client.Send(provider, request, response, error,
			cancellation.get());

		BMessage done(kMsgAIResponse);
		done.AddInt64("request_id", requestId);
		done.AddString("text", ok ? response.text.c_str() : "");
		done.AddString("error", error.c_str());
		done.AddInt64("status", response.httpStatus);
		messenger.SendMessage(&done);
	}).detach();
}


void
AIChatPanel::_FinishResponse(const BString& text, const BString& error,
	long status)
{
	_FinishRequest();

	if (!error.IsEmpty()) {
		BString line(B_TRANSLATE("AI request failed"));
		if (status != 0)
			line << " (" << status << ")";
		line << ": " << error;
		_AppendOutput(line.String());
		return;
	}

	_AppendOutput(B_TRANSLATE("AI response:"));
	_AppendOutput(text.String());

	std::string rawDiff;
	std::string parseError;
	Haikode::AI::UnifiedDiff diff;
	if (Haikode::AI::UnifiedDiff::ExtractFromText(text.String(), diff, rawDiff,
			parseError)) {
		fPendingDiff = diff;
		fPendingRawDiff = rawDiff.c_str();
		fSavedPendingPatchPath = "";
		if (!fProjectRoot.IsEmpty()) {
			std::string savedPatchPath;
			std::string saveError;
			if (Haikode::AI::UnifiedDiff::SavePatchText(fProjectRoot.String(),
					rawDiff, savedPatchPath, saveError)) {
				fSavedPendingPatchPath = savedPatchPath.c_str();
				BString savedLine(B_TRANSLATE("Saved proposed patch: "));
				savedLine << savedPatchPath.c_str();
				_AppendOutput(savedLine.String());
			} else {
				BString saveLine(B_TRANSLATE("Patch proposal save warning: "));
				saveLine << saveError.c_str();
				_AppendOutput(saveLine.String());
			}
		}
		const std::vector<std::string> changedPaths = diff.ChangedPaths();
		if (!changedPaths.empty())
			fPatchPath->SetText(changedPaths.front().c_str());
		if (fPatchHunk != nullptr)
			fPatchHunk->SetText("1");
		_SetPatchControlsEnabled(true);
		BString line(B_TRANSLATE("Unified diff detected: "));
		line << changedPaths.size() << B_TRANSLATE(" file(s), ")
			<< diff.HunkCount() << B_TRANSLATE(" hunk(s).");
		_AppendOutput(line.String());
		for (const std::string& path : changedPaths) {
			BString pathLine("  ");
			pathLine << path.c_str();
			_AppendOutput(pathLine.String());
		}
		_AppendOutput("");
		_AppendOutput(diff.ReviewText().c_str());
		line = B_TRANSLATE("Review the response, then click Apply patch or Reject patch.");
		_AppendOutput(line.String());
	}

	std::vector<Haikode::AI::CommandRequest> commands;
	if (Haikode::AI::ExtractCommandRequests(text.String(), commands, parseError)
		&& !commands.empty()) {
		fPendingCommands = commands;
		fRunCommandButton->SetEnabled(true);
		fRejectCommandButton->SetEnabled(true);
		_AppendOutput(B_TRANSLATE("Command request(s) detected. Haikode did not run them."));
		for (const Haikode::AI::CommandRequest& command : commands) {
			BString line("  ");
			line << (command.summary.empty() ? "Command" : command.summary.c_str())
				<< ":";
			for (const std::string& arg : command.argv)
				line << " " << arg.c_str();
			_AppendOutput(line.String());
			if (command.dangerous) {
				line = B_TRANSLATE("    Warning: ");
				line << command.warning.c_str();
				_AppendOutput(line.String());
			}
		}
		_AppendOutput(B_TRANSLATE("Command execution still requires a separate explicit user action."));
		if (!fProjectRoot.IsEmpty()) {
			std::string savedCommandsPath;
			std::string saveError;
			if (Haikode::AI::SaveCommandRequests(fProjectRoot.String(), commands,
					savedCommandsPath, saveError)) {
				BString line(B_TRANSLATE("Saved command request(s): "));
				line << savedCommandsPath.c_str();
				_AppendOutput(line.String());
			} else {
				BString line(B_TRANSLATE("Command request save warning: "));
				line << saveError.c_str();
				_AppendOutput(line.String());
			}
		}
	} else if (!parseError.empty()) {
		BString line(B_TRANSLATE("Command request parse warning: "));
		line << parseError.c_str();
		_AppendOutput(line.String());
	}
	if (!fPendingDiff.IsEmpty()) {
		_SetPatchControlsEnabled(true);
		if (BString(fPatchPath->Text()).IsEmpty()) {
			const std::vector<std::string> paths = fPendingDiff.ChangedPaths();
			if (!paths.empty())
				fPatchPath->SetText(paths.front().c_str());
		}
	}
	_UpdatePendingActions();
	_SaveSessionRecord(text);
}


bool
AIChatPanel::_FindCodexExecutable(std::string& executable, std::string& error) const
{
	const char* path = std::getenv("PATH");
	executable = Haikode::AI::CodexBridge::FindExecutable("codex",
		path != nullptr ? path : "", error);
	return !executable.empty();
}


void
AIChatPanel::_QueueCodexCommand(const Haikode::AI::CommandRequest& command,
	const char* queuedText)
{
	fPendingCommands.push_back(command);
	fRunCommandButton->SetEnabled(true);
	fRejectCommandButton->SetEnabled(true);
	_UpdatePendingActions();
	_AppendOutput(queuedText);
	_AppendOutput(Haikode::AI::CommandDisplayString(command).c_str());
	_AppendOutput(B_TRANSLATE("Click Run command to approve it, or Reject command to discard it."));
}


void
AIChatPanel::_QueueCodexLoginStatus()
{
	std::string executable;
	std::string error;
	if (!_FindCodexExecutable(executable, error)) {
		BString line(B_TRANSLATE("Codex CLI not found: "));
		line << error.c_str();
		_AppendOutput(line.String());
		return;
	}

	_QueueCodexCommand(Haikode::AI::CodexBridge::LoginStatusCommand(executable),
		B_TRANSLATE("Prepared Codex login status check."));
}


void
AIChatPanel::_QueueCodexDeviceLogin()
{
	std::string executable;
	std::string error;
	if (!_FindCodexExecutable(executable, error)) {
		BString line(B_TRANSLATE("Codex CLI not found: "));
		line << error.c_str();
		_AppendOutput(line.String());
		return;
	}

	_QueueCodexCommand(Haikode::AI::CodexBridge::DeviceLoginCommand(executable),
		B_TRANSLATE("Prepared Codex device login. Haikode will not read Codex tokens."));
}


void
AIChatPanel::_QueueCodexReadOnlyAsk()
{
	if (fProjectRoot.IsEmpty()) {
		_AppendOutput(B_TRANSLATE("Open or activate a project before asking Codex."));
		return;
	}

	std::string executable;
	std::string error;
	if (!_FindCodexExecutable(executable, error)) {
		BString line(B_TRANSLATE("Codex CLI not found: "));
		line << error.c_str();
		_AppendOutput(line.String());
		return;
	}

	BString prompt(fPrompt->Text());
	if (prompt.IsEmpty())
		prompt = B_TRANSLATE("Explain this Haiku project and suggest the next safe change.");

	BString codexPrompt(B_TRANSLATE("You are helping in Haikode, a native Haiku IDE fork of Genio. Run read-only. Do not write files or run build/test commands. Answer with guidance or a unified diff proposal only after explaining the change.\n\nUser prompt: "));
	codexPrompt << prompt;
	if (!fFilePath.IsEmpty())
		codexPrompt << "\n\nActive file: " << fFilePath;
	if (!fSelection.IsEmpty())
		codexPrompt << "\n\nSelected text:\n" << fSelection;

	Haikode::AI::CodexBridgeSettings settings;
	settings.executable = executable;
	settings.projectRoot = fProjectRoot.String();
	settings.model = fModel->Text();

	Haikode::AI::CommandRequest command;
	if (!Haikode::AI::CodexBridge::BuildReadOnlyAskCommand(settings,
			codexPrompt.String(), command, error)) {
		BString line(B_TRANSLATE("Could not prepare Codex request: "));
		line << error.c_str();
		_AppendOutput(line.String());
		return;
	}

	_QueueCodexCommand(command,
		B_TRANSLATE("Prepared read-only Codex request for the active project."));
}


void
AIChatPanel::_RunCodexCommandCaptured(const Haikode::AI::CommandRequest& command)
{
	if (fRequestRunning) {
		_AppendOutput(B_TRANSLATE("An AI or Codex request is already running."));
		return;
	}
	if (fProjectRoot.IsEmpty()) {
		_AppendOutput(B_TRANSLATE("Open or activate a project before running Codex."));
		return;
	}

	fLastUserPrompt = fPrompt->Text();
	fLastProvider = Haikode::AI::ProviderSettings();
	fLastProvider.name = "Codex CLI";
	fLastProvider.baseUrl = "codex-cli";
	fLastProvider.model = "codex";
	fLastProvider.authMode = Haikode::AI::AuthMode::None;

	const int64 requestId = _BeginRequest();
	std::shared_ptr<Haikode::AI::CancellationToken> cancellation
		= fActiveCancellation;

	_AppendOutput(B_TRANSLATE("Running approved read-only Codex request. Output will return here."));
	_AppendOutput(Haikode::AI::CommandDisplayString(command).c_str());

	BMessenger messenger(this);
	const std::vector<std::string> argv = command.argv;
	const std::string projectRoot = fProjectRoot.String();
	std::thread([messenger, argv, projectRoot, requestId, cancellation]() mutable {
		Haikode::AI::ProcessCaptureOptions options;
		options.argv = argv;
		options.workingDirectory = projectRoot;
		options.timeoutSeconds = 600;
		options.maxOutputBytes = 2 * 1024 * 1024;
		options.cancellation = cancellation.get();

		Haikode::AI::ProcessCaptureResult result;
		std::string error;
		const bool ok = Haikode::AI::ProcessCapture::Run(options, result,
			error);
		std::string savedLogPath;
		std::string logError;
		Haikode::AI::ProcessCapture::SaveLog(projectRoot, "codex-ask",
			options, result, error, savedLogPath, logError);

		BMessage done(kMsgCodexCaptureResponse);
		done.AddInt64("request_id", requestId);
		done.AddString("output", result.output.c_str());
		done.AddString("error", ok ? "" : error.c_str());
		done.AddInt32("exit_code", result.exitCode);
		done.AddBool("timed_out", result.timedOut);
		done.AddBool("cancelled", result.cancelled);
		done.AddString("log_path", savedLogPath.c_str());
		done.AddString("log_error", logError.c_str());
		messenger.SendMessage(&done);
	}).detach();
}


void
AIChatPanel::_RunCommandCaptured(const Haikode::AI::CommandRequest& command)
{
	if (fRequestRunning) {
		_AppendOutput(B_TRANSLATE("An AI command is already running."));
		return;
	}
	if (fProjectRoot.IsEmpty()) {
		_AppendOutput(B_TRANSLATE("Open or activate a project before running an AI command."));
		return;
	}

	fLastUserPrompt = fPrompt->Text();
	fLastProvider = Haikode::AI::ProviderSettings();
	fLastProvider.name = "Approved AI command";
	fLastProvider.baseUrl = "local-process";
	fLastProvider.model = "argv";
	fLastProvider.authMode = Haikode::AI::AuthMode::None;

	const int64 requestId = _BeginRequest();
	std::shared_ptr<Haikode::AI::CancellationToken> cancellation
		= fActiveCancellation;

	_AppendOutput(B_TRANSLATE("Running approved argv command in the active project. Output will return here."));
	_AppendOutput(Haikode::AI::CommandDisplayString(command).c_str());

	BMessenger messenger(this);
	const std::vector<std::string> argv = command.argv;
	const std::string projectRoot = fProjectRoot.String();
	std::thread([messenger, argv, projectRoot, requestId, cancellation]() mutable {
		Haikode::AI::ProcessCaptureOptions options;
		options.argv = argv;
		options.workingDirectory = projectRoot;
		options.timeoutSeconds = 600;
		options.maxOutputBytes = 2 * 1024 * 1024;
		options.cancellation = cancellation.get();

		Haikode::AI::ProcessCaptureResult result;
		std::string error;
		const bool ok = Haikode::AI::ProcessCapture::Run(options, result,
			error);
		std::string savedLogPath;
		std::string logError;
		Haikode::AI::ProcessCapture::SaveLog(projectRoot, "ai-command",
			options, result, error, savedLogPath, logError);

		BMessage done(kMsgCommandCaptureResponse);
		done.AddInt64("request_id", requestId);
		done.AddString("output", result.output.c_str());
		done.AddString("error", ok ? "" : error.c_str());
		done.AddInt32("exit_code", result.exitCode);
		done.AddBool("timed_out", result.timedOut);
		done.AddBool("cancelled", result.cancelled);
		done.AddString("log_path", savedLogPath.c_str());
		done.AddString("log_error", logError.c_str());
		messenger.SendMessage(&done);
	}).detach();
}


void
AIChatPanel::_FinishCodexCapture(const BString& output, const BString& error,
	int32 exitCode, bool timedOut, bool cancelled, const BString& logPath,
	const BString& logError)
{
	if (!logPath.IsEmpty()) {
		BString line(B_TRANSLATE("Saved Codex log: "));
		line << logPath;
		_AppendOutput(line.String());
	} else if (!logError.IsEmpty()) {
		BString line(B_TRANSLATE("Codex log save warning: "));
		line << logError;
		_AppendOutput(line.String());
	}

	if (cancelled) {
		_FinishRequest();
		_AppendOutput(B_TRANSLATE("Codex request cancelled."));
		return;
	}
	if (timedOut) {
		_FinishRequest();
		_AppendOutput(B_TRANSLATE("Codex request timed out."));
		return;
	}
	if (!error.IsEmpty()) {
		_FinishRequest();
		BString line(B_TRANSLATE("Codex request failed"));
		if (exitCode >= 0)
			line << " (" << exitCode << ")";
		line << ": " << error;
		_AppendOutput(line.String());
		if (!output.IsEmpty())
			_AppendOutput(output.String());
		return;
	}

	_FinishResponse(output, "", exitCode);
}


void
AIChatPanel::_FinishCommandCapture(const BString& output, const BString& error,
	int32 exitCode, bool timedOut, bool cancelled, const BString& logPath,
	const BString& logError)
{
	_FinishRequest();
	fRunCommandButton->SetEnabled(!fPendingCommands.empty());
	fRejectCommandButton->SetEnabled(!fPendingCommands.empty());

	if (!logPath.IsEmpty()) {
		BString line(B_TRANSLATE("Saved command log: "));
		line << logPath;
		_AppendOutput(line.String());
	} else if (!logError.IsEmpty()) {
		BString line(B_TRANSLATE("Command log save warning: "));
		line << logError;
		_AppendOutput(line.String());
	}

	if (cancelled) {
		_AppendOutput(B_TRANSLATE("Command cancelled."));
		return;
	}
	if (timedOut) {
		_AppendOutput(B_TRANSLATE("Command timed out."));
		if (!output.IsEmpty())
			_AppendOutput(output.String());
		return;
	}
	if (!error.IsEmpty()) {
		BString line(B_TRANSLATE("Command failed"));
		if (exitCode >= 0)
			line << " (" << exitCode << ")";
		line << ": " << error;
		_AppendOutput(line.String());
		if (!output.IsEmpty())
			_AppendOutput(output.String());
		return;
	}

	BString line(B_TRANSLATE("Command completed"));
	if (exitCode >= 0)
		line << " (" << exitCode << ")";
	line << ".";
	_AppendOutput(line.String());
	if (!output.IsEmpty())
		_AppendOutput(output.String());
}


void
AIChatPanel::_RunPendingCommand()
{
	if (fPendingCommands.empty()) {
		_AppendOutput(B_TRANSLATE("No pending command request to run."));
		return;
	}

	const Haikode::AI::CommandRequest command = fPendingCommands.front();
	const std::string display = Haikode::AI::CommandDisplayString(command);
	if (fRequestRunning) {
		_AppendOutput(B_TRANSLATE("An AI or command request is already running."));
		return;
	}
	if (fProjectRoot.IsEmpty()) {
		_AppendOutput(B_TRANSLATE("Open or activate a project before running an AI command."));
		return;
	}

	BString prompt(B_TRANSLATE("Run this AI-requested command in the active project?"));
	prompt << "\n\n" << display.c_str();
	if (command.dangerous)
		prompt << "\n\n" << B_TRANSLATE("Warning: ") << command.warning.c_str();
	if (!command.runnable) {
		prompt << "\n\n"
			<< B_TRANSLATE("Haikode will not run this command because it needs shell-style interpretation. Review and run it manually if you trust it.");
	}

	BAlert* alert = new BAlert("HaikodeRunCommand", prompt.String(),
		B_TRANSLATE("Cancel"), command.runnable ? B_TRANSLATE("Run")
			: B_TRANSLATE("Acknowledge"), nullptr,
		B_WIDTH_AS_USUAL, B_OFFSET_SPACING,
		command.dangerous ? B_WARNING_ALERT : B_IDEA_ALERT);
	const int32 choice = alert->Go();
	if (choice != 1) {
		_AppendOutput(B_TRANSLATE("Command run cancelled."));
		return;
	}

	if (!command.runnable) {
		_AppendOutput(B_TRANSLATE("Command was not run because it requires manual shell review."));
		fPendingCommands.erase(fPendingCommands.begin());
		fRunCommandButton->SetEnabled(!fPendingCommands.empty());
		fRejectCommandButton->SetEnabled(!fPendingCommands.empty());
		_UpdatePendingActions();
		return;
	}

	if (Haikode::AI::CodexBridge::IsReadOnlyAskCommand(command)) {
		if (fRequestRunning) {
			_AppendOutput(B_TRANSLATE("An AI or Codex request is already running."));
			return;
		}
		fPendingCommands.erase(fPendingCommands.begin());
		fRunCommandButton->SetEnabled(!fPendingCommands.empty());
		fRejectCommandButton->SetEnabled(!fPendingCommands.empty());
		_UpdatePendingActions();
		_RunCodexCommandCaptured(command);
		return;
	}

	fPendingCommands.erase(fPendingCommands.begin());
	fRunCommandButton->SetEnabled(!fPendingCommands.empty());
	fRejectCommandButton->SetEnabled(!fPendingCommands.empty());
	_UpdatePendingActions();
	_RunCommandCaptured(command);
}


void
AIChatPanel::_RejectPendingCommand()
{
	if (fPendingCommands.empty()) {
		_AppendOutput(B_TRANSLATE("No pending command request to reject."));
		return;
	}

	const Haikode::AI::CommandRequest command = fPendingCommands.front();
	BString line(B_TRANSLATE("Rejected AI command request: "));
	line << Haikode::AI::CommandDisplayString(command).c_str();
	_AppendOutput(line.String());

	fPendingCommands.erase(fPendingCommands.begin());
	fRunCommandButton->SetEnabled(!fPendingCommands.empty());
	fRejectCommandButton->SetEnabled(!fPendingCommands.empty());
	_UpdatePendingActions();
}


void
AIChatPanel::_ClearPendingCommands()
{
	fPendingCommands.clear();
	if (fRunCommandButton != nullptr)
		fRunCommandButton->SetEnabled(false);
	if (fRejectCommandButton != nullptr)
		fRejectCommandButton->SetEnabled(false);
	_UpdatePendingActions();
}


void
AIChatPanel::_ListProjectFiles()
{
	if (fProjectRoot.IsEmpty()) {
		_AppendOutput(B_TRANSLATE("Open or activate a project before listing project files."));
		return;
	}

	size_t candidateCount = 0;
	const std::vector<Haikode::AI::ProjectFileSummary> files
		= Haikode::AI::BuildProjectMap(fProjectRoot.String(), 500,
			&candidateCount);
	if (files.empty()) {
		BString line(B_TRANSLATE("No text/source files found in active project: "));
		line << fProjectRoot;
		_AppendOutput(line.String());
		_AppendOutput(B_TRANSLATE("Binary files, build folders, .git, .haikode, vendor, and oversized files are skipped."));
		return;
	}

	BString line(B_TRANSLATE("Project files available to Haikode: "));
	line << std::to_string(files.size()).c_str();
	if (candidateCount > files.size())
		line << "/" << std::to_string(candidateCount).c_str();
	line << B_TRANSLATE(" text/source file(s) from ");
	line << fProjectRoot;
	line << ".";
	_AppendOutput(line.String());
	if (fProjectFilePath != nullptr)
		fProjectFilePath->SetText(files.front().path.c_str());

	ProjectFileBrowserWindow* window = new ProjectFileBrowserWindow(
		BMessenger(this), files);
	window->Show();
}


void
AIChatPanel::_OpenSelectedProjectFile()
{
	if (fProjectRoot.IsEmpty()) {
		_AppendOutput(B_TRANSLATE("Open or activate a project before opening a project file."));
		return;
	}
	if (fProjectFilePath == nullptr
		|| BString(fProjectFilePath->Text()).IsEmpty()) {
		_AppendOutput(B_TRANSLATE("Choose or paste a project file path first."));
		return;
	}

	const BString relativePath(fProjectFilePath->Text());
	if (!IsSafeRelativeProjectPath(relativePath)) {
		BString line(B_TRANSLATE("Refusing unsafe project file path: "));
		line << relativePath;
		_AppendOutput(line.String());
		return;
	}

	BPath path(fProjectRoot.String());
	status_t status = path.Append(relativePath.String());
	if (status != B_OK) {
		BString line(B_TRANSLATE("Could not resolve project file path: "));
		line << strerror(status);
		_AppendOutput(line.String());
		return;
	}

	entry_ref ref;
	status = get_ref_for_path(path.Path(), &ref);
	if (status != B_OK) {
		BString line(B_TRANSLATE("Could not find project file: "));
		line << path.Path();
		_AppendOutput(line.String());
		return;
	}

	BMessage open(B_REFS_RECEIVED);
	open.AddRef("refs", &ref);
	if (Window() == nullptr || Window()->PostMessage(&open) != B_OK) {
		BString line(B_TRANSLATE("Could not ask Genio to open project file: "));
		line << path.Path();
		_AppendOutput(line.String());
		return;
	}

	BString line(B_TRANSLATE("Opening project file: "));
	line << relativePath;
	_AppendOutput(line.String());
}


void
AIChatPanel::_ListRecentProjectRecords()
{
	if (fProjectRoot.IsEmpty()) {
		_AppendOutput(B_TRANSLATE("Open or activate a project before listing Haikode records."));
		return;
	}

	const std::vector<Haikode::AI::ProjectRecordEntry> records
		= Haikode::AI::ListProjectRecords(fProjectRoot.String(), 20);
	if (records.empty()) {
		_AppendOutput(B_TRANSLATE("No .haikode records found for this project yet."));
		return;
	}

	_AppendOutput(B_TRANSLATE("Recent Haikode records:"));
	for (const Haikode::AI::ProjectRecordEntry& record : records) {
		BString line("  ");
		line << record.type.c_str() << "  " << record.path.c_str()
			<< "  (" << std::to_string(record.sizeBytes).c_str() << " bytes)";
		_AppendOutput(line.String());
	}
	if (fRecordPath != nullptr)
		fRecordPath->SetText(records.front().path.c_str());

	ProjectRecordBrowserWindow* window = new ProjectRecordBrowserWindow(
		BMessenger(this), records);
	window->Show();
}


void
AIChatPanel::_ShowSelectedProjectRecord()
{
	if (fProjectRoot.IsEmpty()) {
		_AppendOutput(B_TRANSLATE("Open or activate a project before showing a Haikode record."));
		return;
	}
	if (fRecordPath == nullptr || BString(fRecordPath->Text()).IsEmpty()) {
		_AppendOutput(B_TRANSLATE("Choose or paste a .haikode record path first."));
		return;
	}

	std::string text;
	std::string error;
	if (!Haikode::AI::ReadProjectRecord(fProjectRoot.String(),
			fRecordPath->Text(), 256 * 1024, text, error)) {
		BString line(B_TRANSLATE("Could not read Haikode record: "));
		line << error.c_str();
		_AppendOutput(line.String());
		return;
	}

	fOutput->SetText("");
	BString line(B_TRANSLATE("Haikode record: "));
	line << fRecordPath->Text();
	_AppendOutput(line.String());
	_AppendOutput("");
	_AppendOutput(text.c_str());
}


void
AIChatPanel::_UpdatePendingActions()
{
	if (fPendingActions == nullptr)
		return;

	Haikode::AI::PendingActionSummary summary;
	if (!fPendingDiff.IsEmpty()) {
		for (const Haikode::AI::PatchFileStats& stats
				: fPendingDiff.FileStats()) {
			summary.patchFiles.push_back({
				stats.path,
				stats.additions,
				stats.deletions,
				stats.hunkCount,
				stats.newFile
			});
		}
		summary.hunkCount = fPendingDiff.HunkCount();
	}
	summary.commands = fPendingCommands;
	fPendingActions->SetText(Haikode::AI::FormatPendingActions(summary).c_str());
}


void
AIChatPanel::_SaveSessionRecord(const BString& responseText)
{
	if (fProjectRoot.IsEmpty())
		return;

	Haikode::AI::AiSessionRecord session;
	session.userPrompt = fLastUserPrompt.String();
	session.providerBaseUrl = fLastProvider.baseUrl;
	session.providerModel = fLastProvider.model;
	session.authMode = Haikode::AI::ToString(fLastProvider.authMode);
	session.activeFile = fFilePath.String();
	session.responseText = responseText.String();
	if (fPendingActions != nullptr)
		session.pendingActions = fPendingActions->Text();
	session.savedPatchPath = fSavedPendingPatchPath.String();

	std::string savedPath;
	std::string error;
	if (Haikode::AI::SaveAiSession(fProjectRoot.String(), session, savedPath,
			error)) {
		BString line(B_TRANSLATE("Saved AI session: "));
		line << savedPath.c_str();
		_AppendOutput(line.String());
	} else {
		BString line(B_TRANSLATE("AI session save warning: "));
		line << error.c_str();
		_AppendOutput(line.String());
	}
}


void
AIChatPanel::_SelectPatchFile(int32 delta)
{
	if (fPendingDiff.IsEmpty()) {
		_AppendOutput(B_TRANSLATE("No pending patch file to select."));
		return;
	}

	const std::vector<std::string> paths = fPendingDiff.ChangedPaths();
	if (paths.empty()) {
		_AppendOutput(B_TRANSLATE("Pending diff has no changed files."));
		return;
	}

	const BString current = fPatchPath != nullptr
		? BString(fPatchPath->Text()) : BString();
	int32 selected = 0;
	for (int32 index = 0; index < static_cast<int32>(paths.size()); index++) {
		if (current == paths[index].c_str()) {
			selected = index;
			break;
		}
	}

	selected += delta;
	while (selected < 0)
		selected += static_cast<int32>(paths.size());
	selected %= static_cast<int32>(paths.size());

	fPatchPath->SetText(paths[selected].c_str());
	if (fPatchHunk != nullptr)
		fPatchHunk->SetText("1");
	BString line(B_TRANSLATE("Selected patch file: "));
	line << paths[selected].c_str();
	_AppendOutput(line.String());
}


size_t
AIChatPanel::_SelectedPatchHunkIndex(const std::string& path) const
{
	const size_t hunkCount = fPendingDiff.HunkCountForFile(path);
	if (hunkCount == 0)
		return 0;

	long requested = 1;
	if (fPatchHunk != nullptr) {
		const char* text = fPatchHunk->Text();
		char* end = nullptr;
		const long parsed = std::strtol(text, &end, 10);
		if (end != text && parsed > 0)
			requested = parsed;
	}
	if (requested > static_cast<long>(hunkCount))
		requested = static_cast<long>(hunkCount);
	return static_cast<size_t>(requested - 1);
}


void
AIChatPanel::_SelectPatchHunk(int32 delta)
{
	if (fPendingDiff.IsEmpty()) {
		_AppendOutput(B_TRANSLATE("No pending patch hunk to select."));
		return;
	}

	std::vector<std::string> paths = fPendingDiff.ChangedPaths();
	if (paths.empty()) {
		_AppendOutput(B_TRANSLATE("Pending diff has no changed files."));
		return;
	}

	std::string path = fPatchPath != nullptr ? fPatchPath->Text() : "";
	if (path.empty()) {
		path = paths.front();
		if (fPatchPath != nullptr)
			fPatchPath->SetText(path.c_str());
	}

	const size_t hunkCount = fPendingDiff.HunkCountForFile(path);
	if (hunkCount == 0) {
		BString line(B_TRANSLATE("Pending patch has no hunks for selected file: "));
		line << path.c_str();
		_AppendOutput(line.String());
		return;
	}

	int32 selected = static_cast<int32>(_SelectedPatchHunkIndex(path));
	selected += delta;
	while (selected < 0)
		selected += static_cast<int32>(hunkCount);
	selected %= static_cast<int32>(hunkCount);

	BString hunkText;
	hunkText << (selected + 1);
	if (fPatchHunk != nullptr)
		fPatchHunk->SetText(hunkText.String());

	BString line(B_TRANSLATE("Selected patch hunk: "));
	line << path.c_str() << " #" << (selected + 1) << "/"
		<< static_cast<int32>(hunkCount);
	_AppendOutput(line.String());
	const std::string preview = fPendingDiff.ReviewTextForHunk(path,
		static_cast<size_t>(selected));
	if (!preview.empty())
		_AppendOutput(preview.c_str());
}


void
AIChatPanel::_SetPatchControlsEnabled(bool enabled)
{
	const bool hasMultipleFiles = enabled
		&& fPendingDiff.ChangedPaths().size() > 1;
	size_t selectedHunkCount = 0;
	if (enabled && fPatchPath != nullptr && !BString(fPatchPath->Text()).IsEmpty())
		selectedHunkCount = fPendingDiff.HunkCountForFile(fPatchPath->Text());
	if (fPatchPath != nullptr)
		fPatchPath->SetEnabled(enabled);
	if (fPatchHunk != nullptr)
		fPatchHunk->SetEnabled(enabled);
	if (fPreviousPatchFileButton != nullptr)
		fPreviousPatchFileButton->SetEnabled(hasMultipleFiles);
	if (fNextPatchFileButton != nullptr)
		fNextPatchFileButton->SetEnabled(hasMultipleFiles);
	if (fPreviousPatchHunkButton != nullptr)
		fPreviousPatchHunkButton->SetEnabled(enabled && selectedHunkCount > 1);
	if (fNextPatchHunkButton != nullptr)
		fNextPatchHunkButton->SetEnabled(enabled && selectedHunkCount > 1);
	if (fApplyFirstFileButton != nullptr)
		fApplyFirstFileButton->SetEnabled(enabled);
	if (fRejectFirstFileButton != nullptr)
		fRejectFirstFileButton->SetEnabled(enabled);
	if (fApplyHunkButton != nullptr)
		fApplyHunkButton->SetEnabled(enabled);
	if (fRejectHunkButton != nullptr)
		fRejectHunkButton->SetEnabled(enabled);
	if (fReviewPatchButton != nullptr)
		fReviewPatchButton->SetEnabled(enabled);
	if (fApplyPatchButton != nullptr)
		fApplyPatchButton->SetEnabled(enabled);
	if (fRejectPatchButton != nullptr)
		fRejectPatchButton->SetEnabled(enabled);
}


void
AIChatPanel::_ApplyFirstPendingFile()
{
	if (fPendingDiff.IsEmpty()) {
		_AppendOutput(B_TRANSLATE("No pending diff to apply."));
		return;
	}
	if (fProjectRoot.IsEmpty()) {
		_AppendOutput(B_TRANSLATE("Open or activate a project before applying a patch."));
		return;
	}

	std::vector<std::string> paths = fPendingDiff.ChangedPaths();
	if (paths.empty()) {
		_AppendOutput(B_TRANSLATE("Pending diff has no changed files."));
		return;
	}

	std::string path = fPatchPath != nullptr ? fPatchPath->Text() : "";
	if (path.empty())
		path = paths.front();
	Haikode::AI::PatchApplyResult result;
	std::string error;
	if (!fPendingDiff.ApplyFile(fProjectRoot.String(), path, result, error)) {
		BString line(B_TRANSLATE("Selected-file patch apply failed: "));
		line << error.c_str();
		_AppendOutput(line.String());
		return;
	}

	fPendingDiff.RemoveFile(path);
	BString line(B_TRANSLATE("Applied selected patch file: "));
	line << path.c_str();
	_AppendOutput(line.String());
	if (!result.backupDirectory.empty()) {
		line = B_TRANSLATE("Backup: ");
		line << result.backupDirectory.c_str();
		_AppendOutput(line.String());
	}

	if (Window() != nullptr) {
		BMessage notify(MSG_HAIKODE_AI_PATCH_APPLIED);
		notify.AddString("project_root", fProjectRoot);
		notify.AddString("changed_file", path.c_str());
		Window()->PostMessage(&notify);
	}

	if (fPendingDiff.IsEmpty()) {
		fPendingRawDiff = "";
		fSavedPendingPatchPath = "";
		fPatchPath->SetText("");
		if (fPatchHunk != nullptr)
			fPatchHunk->SetText("1");
		_SetPatchControlsEnabled(false);
	} else {
		const std::vector<std::string> remainingPaths
			= fPendingDiff.ChangedPaths();
		if (!remainingPaths.empty())
			fPatchPath->SetText(remainingPaths.front().c_str());
		if (fPatchHunk != nullptr)
			fPatchHunk->SetText("1");
		_SetPatchControlsEnabled(true);
	}
	_UpdatePendingActions();
}


void
AIChatPanel::_ApplySelectedPendingHunk()
{
	if (fPendingDiff.IsEmpty()) {
		_AppendOutput(B_TRANSLATE("No pending diff to apply."));
		return;
	}
	if (fProjectRoot.IsEmpty()) {
		_AppendOutput(B_TRANSLATE("Open or activate a project before applying a patch."));
		return;
	}

	std::vector<std::string> paths = fPendingDiff.ChangedPaths();
	if (paths.empty()) {
		_AppendOutput(B_TRANSLATE("Pending diff has no changed files."));
		return;
	}

	std::string path = fPatchPath != nullptr ? fPatchPath->Text() : "";
	if (path.empty())
		path = paths.front();
	const size_t hunkIndex = _SelectedPatchHunkIndex(path);

	Haikode::AI::PatchApplyResult result;
	std::string error;
	if (!fPendingDiff.ApplyHunk(fProjectRoot.String(), path, hunkIndex, result,
			error)) {
		BString line(B_TRANSLATE("Selected-hunk patch apply failed: "));
		line << error.c_str();
		_AppendOutput(line.String());
		return;
	}

	fPendingDiff.RemoveHunk(path, hunkIndex);
	BString line(B_TRANSLATE("Applied selected patch hunk: "));
	line << path.c_str() << " #" << static_cast<int32>(hunkIndex + 1);
	_AppendOutput(line.String());
	if (!result.backupDirectory.empty()) {
		line = B_TRANSLATE("Backup: ");
		line << result.backupDirectory.c_str();
		_AppendOutput(line.String());
	}

	if (Window() != nullptr) {
		BMessage notify(MSG_HAIKODE_AI_PATCH_APPLIED);
		notify.AddString("project_root", fProjectRoot);
		notify.AddString("changed_file", path.c_str());
		Window()->PostMessage(&notify);
	}

	if (fPendingDiff.IsEmpty()) {
		fPendingRawDiff = "";
		fSavedPendingPatchPath = "";
		fPatchPath->SetText("");
		fPatchHunk->SetText("1");
		_SetPatchControlsEnabled(false);
	} else {
		if (fPendingDiff.HunkCountForFile(path) == 0) {
			const std::vector<std::string> remainingPaths
				= fPendingDiff.ChangedPaths();
			if (!remainingPaths.empty())
				fPatchPath->SetText(remainingPaths.front().c_str());
		}
		fPatchHunk->SetText("1");
		_SetPatchControlsEnabled(true);
	}
	_UpdatePendingActions();
}


void
AIChatPanel::_RejectFirstPendingFile()
{
	if (fPendingDiff.IsEmpty()) {
		_AppendOutput(B_TRANSLATE("No pending diff to reject."));
		return;
	}

	std::vector<std::string> paths = fPendingDiff.ChangedPaths();
	if (paths.empty()) {
		_AppendOutput(B_TRANSLATE("Pending diff has no changed files."));
		return;
	}

	std::string path = fPatchPath != nullptr ? fPatchPath->Text() : "";
	if (path.empty())
		path = paths.front();
	if (!fPendingDiff.RemoveFile(path)) {
		BString line(B_TRANSLATE("Pending diff does not contain selected file: "));
		line << path.c_str();
		_AppendOutput(line.String());
		return;
	}

	BString line(B_TRANSLATE("Rejected selected patch file: "));
	line << path.c_str();
	_AppendOutput(line.String());

	if (fPendingDiff.IsEmpty()) {
		fPendingRawDiff = "";
		fSavedPendingPatchPath = "";
		fPatchPath->SetText("");
		if (fPatchHunk != nullptr)
			fPatchHunk->SetText("1");
		_SetPatchControlsEnabled(false);
	} else {
		const std::vector<std::string> remainingPaths
			= fPendingDiff.ChangedPaths();
		if (!remainingPaths.empty())
			fPatchPath->SetText(remainingPaths.front().c_str());
		if (fPatchHunk != nullptr)
			fPatchHunk->SetText("1");
		_SetPatchControlsEnabled(true);
	}
	_UpdatePendingActions();
}


void
AIChatPanel::_RejectSelectedPendingHunk()
{
	if (fPendingDiff.IsEmpty()) {
		_AppendOutput(B_TRANSLATE("No pending diff to reject."));
		return;
	}

	std::vector<std::string> paths = fPendingDiff.ChangedPaths();
	if (paths.empty()) {
		_AppendOutput(B_TRANSLATE("Pending diff has no changed files."));
		return;
	}

	std::string path = fPatchPath != nullptr ? fPatchPath->Text() : "";
	if (path.empty())
		path = paths.front();
	const size_t hunkIndex = _SelectedPatchHunkIndex(path);
	if (!fPendingDiff.RemoveHunk(path, hunkIndex)) {
		BString line(B_TRANSLATE("Pending patch does not contain selected hunk: "));
		line << path.c_str() << " #" << static_cast<int32>(hunkIndex + 1);
		_AppendOutput(line.String());
		return;
	}

	BString line(B_TRANSLATE("Rejected selected patch hunk: "));
	line << path.c_str() << " #" << static_cast<int32>(hunkIndex + 1);
	_AppendOutput(line.String());

	if (fPendingDiff.IsEmpty()) {
		fPendingRawDiff = "";
		fSavedPendingPatchPath = "";
		fPatchPath->SetText("");
		fPatchHunk->SetText("1");
		_SetPatchControlsEnabled(false);
	} else {
		if (fPendingDiff.HunkCountForFile(path) == 0) {
			const std::vector<std::string> remainingPaths
				= fPendingDiff.ChangedPaths();
			if (!remainingPaths.empty())
				fPatchPath->SetText(remainingPaths.front().c_str());
		}
		fPatchHunk->SetText("1");
		_SetPatchControlsEnabled(true);
	}
	_UpdatePendingActions();
}


void
AIChatPanel::_ApplyPendingDiff()
{
	if (fPendingDiff.IsEmpty()) {
		_AppendOutput(B_TRANSLATE("No pending diff to apply."));
		return;
	}
	if (fProjectRoot.IsEmpty()) {
		_AppendOutput(B_TRANSLATE("Open or activate a project before applying a patch."));
		return;
	}

	std::string savedPatchPath = fSavedPendingPatchPath.String();
	if (!fPendingRawDiff.IsEmpty()) {
		std::string saveError;
		if (savedPatchPath.empty()
			&& !Haikode::AI::UnifiedDiff::SavePatchText(fProjectRoot.String(),
				fPendingRawDiff.String(), savedPatchPath, saveError)) {
			BString line(B_TRANSLATE("Patch apply failed: could not save patch: "));
			line << saveError.c_str();
			_AppendOutput(line.String());
			return;
		}
	}

	Haikode::AI::PatchApplyResult result;
	std::string error;
	if (!fPendingDiff.Apply(fProjectRoot.String(), result, error)) {
		BString line(B_TRANSLATE("Patch apply failed: "));
		line << error.c_str();
		_AppendOutput(line.String());
		return;
	}

	BString line(B_TRANSLATE("Patch applied. Backup: "));
	line << result.backupDirectory.c_str();
	_AppendOutput(line.String());
	if (!savedPatchPath.empty()) {
		line = B_TRANSLATE("Saved patch: ");
		line << savedPatchPath.c_str();
		_AppendOutput(line.String());
	}
	for (const std::string& file : result.changedFiles) {
		BString fileLine("  ");
		fileLine << file.c_str();
		_AppendOutput(fileLine.String());
	}
	if (Window() != nullptr) {
		BMessage notify(MSG_HAIKODE_AI_PATCH_APPLIED);
		notify.AddString("project_root", fProjectRoot);
		for (const std::string& file : result.changedFiles)
			notify.AddString("changed_file", file.c_str());
		Window()->PostMessage(&notify);
	}
	_RejectPendingDiff();
}


void
AIChatPanel::_RejectPendingDiff()
{
	fPendingDiff = Haikode::AI::UnifiedDiff();
	fPendingRawDiff = "";
	fSavedPendingPatchPath = "";
	fPatchPath->SetText("");
	if (fPatchHunk != nullptr)
		fPatchHunk->SetText("1");
	_SetPatchControlsEnabled(false);
	_UpdatePendingActions();
}


void
AIChatPanel::_AppendOutput(const char* text)
{
	BString current(fOutput->Text());
	if (!current.IsEmpty())
		current << "\n";
	current << (text == nullptr ? "" : text);
	fOutput->SetText(current.String());
}


Haikode::AI::ProviderSettings
AIChatPanel::_ProviderFromFields() const
{
	Haikode::AI::ProviderSettings provider;
	provider.baseUrl = fBaseUrl->Text();
	provider.model = fModel->Text();
	provider.authMode = AuthModeFromString(fAuthMode->Text());
	provider.apiKey = fApiKey->Text();
	provider.oauthToken = fOAuthToken->Text();
	return provider;
}


Haikode::AI::OAuthSettings
AIChatPanel::_OAuthSettingsFromFields() const
{
	Haikode::AI::OAuthSettings settings;
	settings.authUrl = fOAuthAuthUrl->Text();
	settings.tokenUrl = fOAuthTokenUrl->Text();
	settings.clientId = fOAuthClientId->Text();
	settings.scope = fOAuthScope->Text();
	settings.redirectUri = fOAuthRedirectUri->Text();
	return settings;
}


Haikode::AI::VibeCodingRequest
AIChatPanel::_RequestFromContext(Haikode::AI::PromptMode mode) const
{
	Haikode::AI::VibeCodingRequest request;
	request.mode = mode;
	request.projectRoot = fProjectRoot.String();
	request.userPrompt = fPrompt->Text();
	size_t projectMapCandidateCount = 0;
	request.projectFiles = Haikode::AI::BuildProjectMap(fProjectRoot.String(),
		80, &projectMapCandidateCount);
	request.projectMapCandidateCount = projectMapCandidateCount;
	if (mode == Haikode::AI::PromptMode::ReviewDiff) {
		const BString selectedPath = fPatchPath != nullptr
			? BString(fPatchPath->Text()) : BString();
		if (!selectedPath.IsEmpty()) {
			const std::string selectedDiff
				= fPendingDiff.ReviewTextForFile(selectedPath.String());
			if (!selectedDiff.empty()) {
				request.pendingDiffPath = selectedPath.String();
				request.pendingDiff = selectedDiff;
			}
		}
		if (request.pendingDiff.empty())
			request.pendingDiff = fPendingRawDiff.String();
	}

	if (!fOpenFiles.empty()) {
		for (const Haikode::AI::ContextFile& file : fOpenFiles) {
			if (!file.path.empty() && !file.text.empty()) {
				std::string normalizedPath;
				std::string normalizeError;
				if (Haikode::AI::NormalizeProjectContextPath(
						fProjectRoot.String(), file.path, normalizedPath,
						normalizeError)) {
					Haikode::AI::ContextFile normalizedFile = file;
					normalizedFile.path = normalizedPath;
					request.files.push_back(normalizedFile);
				} else {
					request.contextWarnings.push_back(
						"Open editor file was not included: " + normalizeError);
				}
			}
		}
	} else {
		const std::string contextText = Haikode::AI::SelectContextText(
			fSelection.String(), fFileText.String());
		if (!fFilePath.IsEmpty() && !contextText.empty()) {
			std::string normalizedPath;
			std::string normalizeError;
			Haikode::AI::ContextFile file;
			if (Haikode::AI::NormalizeProjectContextPath(fProjectRoot.String(),
					fFilePath.String(), normalizedPath, normalizeError)) {
				file.path = normalizedPath;
				file.text = contextText;
				request.files.push_back(file);
			} else {
				request.contextWarnings.push_back(
					"Active editor file was not included: " + normalizeError);
			}
		}
	}

	if (fProjectFilePath != nullptr
		&& !BString(fProjectFilePath->Text()).IsEmpty()) {
		Haikode::AI::ContextFile selectedFile;
		std::string loadError;
		if (Haikode::AI::LoadProjectContextFile(fProjectRoot.String(),
				fProjectFilePath->Text(), 200 * 1024, selectedFile,
				loadError)) {
			bool alreadyIncluded = false;
			for (const Haikode::AI::ContextFile& file : request.files) {
				if (file.path == selectedFile.path) {
					alreadyIncluded = true;
					break;
				}
			}
			if (!alreadyIncluded)
				request.files.push_back(selectedFile);
		} else {
			request.contextWarnings.push_back(
				"Selected project file was not included: " + loadError);
		}
	}

	return request;
}
