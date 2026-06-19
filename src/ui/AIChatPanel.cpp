/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "AIChatPanel.h"

#include <Application.h>
#include <Button.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
#include <Message.h>
#include <Messenger.h>
#include <ScrollView.h>
#include <TextControl.h>
#include <TextView.h>

#include "ConfigManager.h"
#include "ConfigWindow.h"
#include "GenioWindowMessages.h"

#include <thread>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "AIChatPanel"

extern ConfigManager gCFG;

namespace {

const uint32 kMsgSaveProvider = 'hisp';
const uint32 kMsgOpenSetup = 'hios';
const uint32 kMsgAsk = 'hiak';
const uint32 kMsgProposePatch = 'hipa';
const uint32 kMsgAIResponse = 'hirs';
const uint32 kMsgApplyPatch = 'hiap';
const uint32 kMsgRejectPatch = 'hirp';

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
	fPrompt(nullptr),
	fOutput(nullptr),
	fSaveProvider(nullptr),
	fSetupButton(nullptr),
	fAskButton(nullptr),
	fPatchButton(nullptr),
	fApplyPatchButton(nullptr),
	fRejectPatchButton(nullptr),
	fRequestRunning(false)
{
	_BuildInterface();
}


void
AIChatPanel::AttachedToWindow()
{
	BGroupView::AttachedToWindow();
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
		case kMsgApplyPatch:
			_ApplyPendingDiff();
			break;
		case kMsgRejectPatch:
			_RejectPendingDiff();
			_AppendOutput(B_TRANSLATE("Pending patch rejected."));
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
	fPrompt = new BTextControl("haikode_ai_prompt", B_TRANSLATE("Prompt"), "",
		new BMessage(kMsgAsk));

	fSaveProvider = new BButton("haikode_ai_save_provider",
		B_TRANSLATE("Save provider"), new BMessage(kMsgSaveProvider));
	fSetupButton = new BButton("haikode_ai_setup",
		B_TRANSLATE("AI Setup"), new BMessage(kMsgOpenSetup));
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
		.End()
		.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
			.Add(fPrompt)
			.Add(fSetupButton)
			.Add(fSaveProvider)
			.Add(fAskButton)
			.Add(fPatchButton)
			.Add(fApplyPatchButton)
			.Add(fRejectPatchButton)
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
}


void
AIChatPanel::_SaveProviderToConfig()
{
	gCFG["haikode_ai_base_url"] = fBaseUrl->Text();
	gCFG["haikode_ai_model"] = fModel->Text();
	gCFG["haikode_ai_auth_mode"] = fAuthMode->Text();
	gCFG["haikode_ai_api_key"] = fApiKey->Text();
	gCFG["haikode_ai_oauth_token"] = fOAuthToken->Text();

	_AppendOutput(B_TRANSLATE("Provider settings saved."));
}


void
AIChatPanel::_OpenProviderSettings()
{
	ConfigWindow* window = new ConfigWindow(gCFG);
	window->Show();
	_AppendOutput(B_TRANSLATE("Opened Haikode settings. Select the Haikode AI section, paste your API key, and close the settings window to save it."));
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

	if (!provider.HasUsableCredentials()) {
		if (provider.authMode == Haikode::AI::AuthMode::ApiKey) {
			_AppendOutput(B_TRANSLATE("Paste your OpenAI-compatible API key into the API key field or click AI Setup. Haikode stores it in the app settings, so no Terminal export is required."));
		} else if (provider.authMode == Haikode::AI::AuthMode::OAuth) {
			_AppendOutput(B_TRANSLATE("Paste an OAuth bearer token into the OAuth token field or click AI Setup."));
		} else {
			_AppendOutput(B_TRANSLATE("Choose api-key, oauth, or local auth and fill the matching provider field."));
		}
		return;
	}

	fRequestRunning = true;
	fAskButton->SetEnabled(false);
	fPatchButton->SetEnabled(false);
	fSaveProvider->SetEnabled(false);
	fSetupButton->SetEnabled(false);
	fApplyPatchButton->SetEnabled(false);
	fRejectPatchButton->SetEnabled(false);
	fPendingDiff = Haikode::AI::UnifiedDiff();
	fPendingRawDiff = "";

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
