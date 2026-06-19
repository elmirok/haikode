/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "AIChatPanel.h"

#include <Button.h>
#include <Catalog.h>
#include <LayoutBuilder.h>
#include <Message.h>
#include <ScrollView.h>
#include <TextControl.h>
#include <TextView.h>

#include "ConfigManager.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "AIChatPanel"

extern ConfigManager gCFG;

namespace {

const uint32 kMsgSaveProvider = 'hisp';
const uint32 kMsgAsk = 'hiak';
const uint32 kMsgProposePatch = 'hipa';

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
	fAskButton(nullptr),
	fPatchButton(nullptr)
{
	_BuildInterface();
}


void
AIChatPanel::AttachedToWindow()
{
	BGroupView::AttachedToWindow();
	_LoadProviderFromConfig();
	SetTabLabel(B_TRANSLATE("Haikode AI"));
}


void
AIChatPanel::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgSaveProvider:
			_SaveProviderToConfig();
			break;
		case kMsgAsk:
			_BuildPrompt(Haikode::AI::PromptMode::Ask);
			break;
		case kMsgProposePatch:
			_BuildPrompt(Haikode::AI::PromptMode::ProposePatch);
			break;
		default:
			BGroupView::MessageReceived(message);
			break;
	}
}


void
AIChatPanel::SetActiveContext(const BString& projectRoot, const BString& filePath,
	const BString& selection)
{
	fProjectRoot = projectRoot;
	fFilePath = filePath;
	fSelection = selection;
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
	fAskButton = new BButton("haikode_ai_ask", B_TRANSLATE("Ask"),
		new BMessage(kMsgAsk));
	fPatchButton = new BButton("haikode_ai_patch", B_TRANSLATE("Propose patch"),
		new BMessage(kMsgProposePatch));

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
			.Add(fSaveProvider)
			.Add(fAskButton)
			.Add(fPatchButton)
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

	_AppendOutput(B_TRANSLATE("Provider settings saved. Network transport is the next integration slice."));
}


void
AIChatPanel::_BuildPrompt(Haikode::AI::PromptMode mode)
{
	Haikode::AI::PromptBuilder builder;
	Haikode::AI::PromptBuildResult result = builder.Build(
		_RequestFromContext(mode), 200 * 1024, 10);

	fOutput->SetText("");
	_AppendOutput(B_TRANSLATE("Prompt preview. Haikode will send this to the configured provider once the transport is enabled."));
	_AppendOutput("");

	Haikode::AI::ProviderSettings provider = _ProviderFromFields();
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
	_AppendOutput(result.prompt.c_str());
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

	if (!fFilePath.IsEmpty() || !fSelection.IsEmpty()) {
		Haikode::AI::ContextFile file;
		file.path = fFilePath.String();
		file.text = fSelection.String();
		request.files.push_back(file);
	}

	return request;
}
