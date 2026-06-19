/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "AIChatPanel.h"

#include <Application.h>
#include <Alert.h>
#include <Button.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
#include <Message.h>
#include <Messenger.h>
#include <Roster.h>
#include <ScrollView.h>
#include <TextControl.h>
#include <TextView.h>

#include "ConfigManager.h"
#include "ConfigWindow.h"
#include "GenioWindowMessages.h"

#include <cstring>
#include <thread>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "AIChatPanel"

extern ConfigManager gCFG;

namespace {

const uint32 kMsgSaveProvider = 'hisp';
const uint32 kMsgOpenSetup = 'hios';
const uint32 kMsgPresetOpenAI = 'hpai';
const uint32 kMsgPresetOllama = 'hpol';
const uint32 kMsgPresetLMStudio = 'hplm';
const uint32 kMsgStartOAuth = 'hiso';
const uint32 kMsgExchangeOAuth = 'hixo';
const uint32 kMsgOAuthResponse = 'hior';
const uint32 kMsgAsk = 'hiak';
const uint32 kMsgProposePatch = 'hipa';
const uint32 kMsgAIResponse = 'hirs';
const uint32 kMsgApplyPatch = 'hiap';
const uint32 kMsgRejectPatch = 'hirp';
const uint32 kMsgRunCommand = 'hirc';

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
	fOutput(nullptr),
	fSaveProvider(nullptr),
	fSetupButton(nullptr),
	fOpenAIPresetButton(nullptr),
	fOllamaPresetButton(nullptr),
	fLMStudioPresetButton(nullptr),
	fStartOAuthButton(nullptr),
	fExchangeOAuthButton(nullptr),
	fAskButton(nullptr),
	fPatchButton(nullptr),
	fApplyPatchButton(nullptr),
	fRejectPatchButton(nullptr),
	fRunCommandButton(nullptr),
	fRequestRunning(false)
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
	fStartOAuthButton->SetTarget(this);
	fExchangeOAuthButton->SetTarget(this);
	fAskButton->SetTarget(this);
	fPatchButton->SetTarget(this);
	fApplyPatchButton->SetTarget(this);
	fRejectPatchButton->SetTarget(this);
	fRunCommandButton->SetTarget(this);
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
		case kMsgPresetOpenAI:
			_ApplyProviderPreset(Haikode::AI::ProviderPreset::OpenAI);
			break;
		case kMsgPresetOllama:
			_ApplyProviderPreset(Haikode::AI::ProviderPreset::Ollama);
			break;
		case kMsgPresetLMStudio:
			_ApplyProviderPreset(Haikode::AI::ProviderPreset::LMStudio);
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
		case kMsgProposePatch:
			_SendPrompt(Haikode::AI::PromptMode::ProposePatch);
			break;
		case kMsgAIResponse:
		{
			_FinishResponse(message->GetString("text", ""),
				message->GetString("error", ""), message->GetInt64("status", 0));
			break;
		}
		case kMsgOAuthResponse:
		{
			_FinishOAuthExchange(message->GetString("token", ""),
				message->GetString("error", ""), message->GetInt64("status", 0));
			break;
		}
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
	const BString& selection, const BString& fileText)
{
	fProjectRoot = projectRoot;
	fFilePath = filePath;
	fSelection = selection;
	fFileText = fileText;
}


void
AIChatPanel::SetTabLabel(BString label)
{
	if (fPanelTabManager != nullptr)
		fPanelTabManager->SetLabelForTab(fTabId, label);
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
	fOAuthToken = new BTextControl("haikode_ai_oauth_token",
		B_TRANSLATE("OAuth token"), "", nullptr);
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
	fStartOAuthButton = new BButton("haikode_ai_start_oauth",
		B_TRANSLATE("Start OAuth"), new BMessage(kMsgStartOAuth));
	fExchangeOAuthButton = new BButton("haikode_ai_exchange_oauth",
		B_TRANSLATE("Exchange code"), new BMessage(kMsgExchangeOAuth));
	fAskButton = new BButton("haikode_ai_ask", B_TRANSLATE("Ask"),
		new BMessage(kMsgAsk));
	fPatchButton = new BButton("haikode_ai_patch", B_TRANSLATE("Propose patch"),
		new BMessage(kMsgProposePatch));
	fApplyPatchButton = new BButton("haikode_ai_apply_patch",
		B_TRANSLATE("Apply patch"), new BMessage(kMsgApplyPatch));
	fApplyPatchButton->SetEnabled(false);
	fRejectPatchButton = new BButton("haikode_ai_reject_patch",
		B_TRANSLATE("Reject patch"), new BMessage(kMsgRejectPatch));
	fRejectPatchButton->SetEnabled(false);
	fRunCommandButton = new BButton("haikode_ai_run_command",
		B_TRANSLATE("Run command"), new BMessage(kMsgRunCommand));
	fRunCommandButton->SetEnabled(false);

	fOutput = new BTextView("haikode_ai_output");
	fOutput->MakeEditable(false);
	fOutput->SetText(B_TRANSLATE("Configure a cloud or local OpenAI-compatible provider, then ask about the active project, file, or selection."));
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
			.AddGlue()
		.End()
		.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
			.Add(fPrompt)
			.Add(fSetupButton)
			.Add(fSaveProvider)
			.Add(fStartOAuthButton)
			.Add(fExchangeOAuthButton)
			.Add(fAskButton)
			.Add(fPatchButton)
			.Add(fApplyPatchButton)
			.Add(fRejectPatchButton)
			.Add(fRunCommandButton)
		.End()
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

	_AppendOutput(B_TRANSLATE("Provider settings saved."));
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
	ConfigWindow* window = new ConfigWindow(gCFG);
	window->Show();
	_AppendOutput(B_TRANSLATE("Opened Haikode settings. Select the Haikode AI section, paste your API key, and close the settings window to save it."));
}


void
AIChatPanel::_StartOAuth()
{
	_SaveProviderToConfig();
	Haikode::AI::OAuthSettings settings = _OAuthSettingsFromFields();
	if (settings.authUrl.empty() || settings.clientId.empty()
		|| settings.redirectUri.empty()) {
		_AppendOutput(B_TRANSLATE("OAuth auth URL, client ID, and redirect URI are required."));
		return;
	}

	const std::string verifier = Haikode::AI::OAuthClient::GenerateVerifier();
	gCFG["haikode_ai_oauth_verifier"] = verifier.c_str();
	fAuthMode->SetText("oauth");
	gCFG["haikode_ai_auth_mode"] = "oauth";

	const std::string authUrl = Haikode::AI::OAuthClient::BuildAuthUrl(
		settings, verifier, "haikode");
	const char* argv[2] = {authUrl.c_str(), nullptr};
	status_t status = be_roster->Launch("text/html", 1, argv);
	if (status != B_OK) {
		BString line(B_TRANSLATE("Could not open OAuth URL in browser: "));
		line << strerror(status);
		_AppendOutput(line.String());
	}

	_AppendOutput(B_TRANSLATE("OAuth login URL generated. Complete login in the browser, then paste the returned authorization code into OAuth code and click Exchange code."));
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
	const std::string code = fOAuthCode->Text();
	const std::string verifier = BString(gCFG["haikode_ai_oauth_verifier"]).String();

	fRequestRunning = true;
	fAskButton->SetEnabled(false);
	fPatchButton->SetEnabled(false);
	fSaveProvider->SetEnabled(false);
	fSetupButton->SetEnabled(false);
	fOpenAIPresetButton->SetEnabled(false);
	fOllamaPresetButton->SetEnabled(false);
	fLMStudioPresetButton->SetEnabled(false);
	fStartOAuthButton->SetEnabled(false);
	fExchangeOAuthButton->SetEnabled(false);

	BMessenger messenger(this);
	std::thread([messenger, settings, code, verifier]() mutable {
		Haikode::AI::OAuthClient client;
		Haikode::AI::OAuthTokenResponse response;
		std::string error;
		const bool ok = client.ExchangeCode(settings, code, verifier, response,
			error);

		BMessage done(kMsgOAuthResponse);
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
	fRequestRunning = false;
	fAskButton->SetEnabled(true);
	fPatchButton->SetEnabled(true);
	fSaveProvider->SetEnabled(true);
	fSetupButton->SetEnabled(true);
	fOpenAIPresetButton->SetEnabled(true);
	fOllamaPresetButton->SetEnabled(true);
	fLMStudioPresetButton->SetEnabled(true);
	fStartOAuthButton->SetEnabled(true);
	fExchangeOAuthButton->SetEnabled(true);

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

	Haikode::AI::PromptBuilder builder;
	Haikode::AI::PromptBuildResult result = builder.Build(
		_RequestFromContext(mode), 200 * 1024, 10);
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

	for (const std::string& warning : result.warnings)
		_AppendOutput(warning.c_str());

	_AppendOutput("");

	std::string validationError;
	if (!provider.Validate(validationError)) {
		_AppendOutput(validationError.c_str());
		_AppendOutput(B_TRANSLATE("Click AI Setup to configure the provider inside Haikode. No Terminal export is required."));
		return;
	}

	fRequestRunning = true;
	fAskButton->SetEnabled(false);
	fPatchButton->SetEnabled(false);
	fSaveProvider->SetEnabled(false);
	fSetupButton->SetEnabled(false);
	fOpenAIPresetButton->SetEnabled(false);
	fOllamaPresetButton->SetEnabled(false);
	fLMStudioPresetButton->SetEnabled(false);
	fStartOAuthButton->SetEnabled(false);
	fExchangeOAuthButton->SetEnabled(false);
	fApplyPatchButton->SetEnabled(false);
	fRejectPatchButton->SetEnabled(false);
	fPendingDiff = Haikode::AI::UnifiedDiff();
	fPendingRawDiff = "";
	_ClearPendingCommands();

	BMessenger messenger(this);
	std::string prompt = result.prompt;
	std::thread([messenger, provider, prompt]() mutable {
		Haikode::AI::OpenAICompatibleClient client;
		Haikode::AI::ChatRequest request;
		request.prompt = prompt;
		Haikode::AI::ChatResponse response;
		std::string error;
		const bool ok = client.Send(provider, request, response, error);

		BMessage done(kMsgAIResponse);
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
	fRequestRunning = false;
	fAskButton->SetEnabled(true);
	fPatchButton->SetEnabled(true);
	fSaveProvider->SetEnabled(true);
	fSetupButton->SetEnabled(true);
	fOpenAIPresetButton->SetEnabled(true);
	fOllamaPresetButton->SetEnabled(true);
	fLMStudioPresetButton->SetEnabled(true);
	fStartOAuthButton->SetEnabled(true);
	fExchangeOAuthButton->SetEnabled(true);

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
		fApplyPatchButton->SetEnabled(true);
		fRejectPatchButton->SetEnabled(true);
		BString line(B_TRANSLATE("Unified diff detected: "));
		line << diff.ChangedPaths().size() << B_TRANSLATE(" file(s), ")
			<< diff.HunkCount() << B_TRANSLATE(" hunk(s).");
		_AppendOutput(line.String());
		for (const std::string& path : diff.ChangedPaths()) {
			BString pathLine("  ");
			pathLine << path.c_str();
			_AppendOutput(pathLine.String());
		}
		line = B_TRANSLATE("Review the response, then click Apply patch or Reject patch.");
		_AppendOutput(line.String());
	}

	std::vector<Haikode::AI::CommandRequest> commands;
	if (Haikode::AI::ExtractCommandRequests(text.String(), commands, parseError)
		&& !commands.empty()) {
		fPendingCommands = commands;
		fRunCommandButton->SetEnabled(true);
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
	BString prompt(B_TRANSLATE("Run this AI-requested command in the active project?"));
	prompt << "\n\n" << display.c_str();
	if (command.dangerous)
		prompt << "\n\n" << B_TRANSLATE("Warning: ") << command.warning.c_str();

	BAlert* alert = new BAlert("HaikodeRunCommand", prompt.String(),
		B_TRANSLATE("Cancel"), B_TRANSLATE("Run"), nullptr,
		B_WIDTH_AS_USUAL, B_OFFSET_SPACING,
		command.dangerous ? B_WARNING_ALERT : B_IDEA_ALERT);
	const int32 choice = alert->Go();
	if (choice != 1) {
		_AppendOutput(B_TRANSLATE("Command run cancelled."));
		return;
	}

	if (Window() == nullptr) {
		_AppendOutput(B_TRANSLATE("No window is available to run the command."));
		return;
	}

	BMessage run(MSG_RUN_CONSOLE_PROGRAM);
	run.AddString("command", display.c_str());
	Window()->PostMessage(&run);
	BString line(B_TRANSLATE("Approved command sent to Genio console: "));
	line << display.c_str();
	_AppendOutput(line.String());

	fPendingCommands.erase(fPendingCommands.begin());
	fRunCommandButton->SetEnabled(!fPendingCommands.empty());
}


void
AIChatPanel::_ClearPendingCommands()
{
	fPendingCommands.clear();
	if (fRunCommandButton != nullptr)
		fRunCommandButton->SetEnabled(false);
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

	std::string savedPatchPath;
	if (!fPendingRawDiff.IsEmpty()) {
		std::string saveError;
		if (!Haikode::AI::UnifiedDiff::SavePatchText(fProjectRoot.String(),
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
	fApplyPatchButton->SetEnabled(false);
	fRejectPatchButton->SetEnabled(false);
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

	const std::string contextText = Haikode::AI::SelectContextText(
		fSelection.String(), fFileText.String());
	if (!fFilePath.IsEmpty() && !contextText.empty()) {
		Haikode::AI::ContextFile file;
		file.path = fFilePath.String();
		file.text = contextText;
		request.files.push_back(file);
	}

	return request;
}
