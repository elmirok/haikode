/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include <GroupView.h>
#include <String.h>

#include <vector>

#include "AIProvider.h"
#include "OpenAICompatibleClient.h"
#include "PanelTabManager.h"
#include "UnifiedDiff.h"
#include "VibeCoding.h"

class BButton;
class BTextControl;
class BTextView;

class AIChatPanel : public BGroupView {
public:
	AIChatPanel(PanelTabManager* panelTabManager, tab_id id);

	void AttachedToWindow() override;
	void DetachedFromWindow() override;
	void MessageReceived(BMessage* message) override;

	void SetActiveContext(const BString& projectRoot, const BString& filePath,
		const BString& selection, const BString& fileText);
	void SetTabLabel(BString label);

private:
	void _BuildInterface();
	void _LoadProviderFromConfig();
	void _SaveProviderToConfig();
	void _OpenProviderSettings();
	void _SendPrompt(Haikode::AI::PromptMode mode);
	void _FinishResponse(const BString& text, const BString& error, long status);
	void _ApplyPendingDiff();
	void _RejectPendingDiff();
	void _RunPendingCommand();
	void _ClearPendingCommands();
	void _AppendOutput(const char* text);
	Haikode::AI::ProviderSettings _ProviderFromFields() const;
	Haikode::AI::VibeCodingRequest _RequestFromContext(
		Haikode::AI::PromptMode mode) const;

	PanelTabManager* fPanelTabManager;
	tab_id fTabId;

	BTextControl* fBaseUrl;
	BTextControl* fModel;
	BTextControl* fAuthMode;
	BTextControl* fApiKey;
	BTextControl* fOAuthToken;
	BTextControl* fPrompt;
	BTextView* fOutput;
	BButton* fSaveProvider;
	BButton* fSetupButton;
	BButton* fAskButton;
	BButton* fPatchButton;
	BButton* fApplyPatchButton;
	BButton* fRejectPatchButton;
	BButton* fRunCommandButton;
	bool fRequestRunning;
	Haikode::AI::UnifiedDiff fPendingDiff;
	BString fPendingRawDiff;
	std::vector<Haikode::AI::CommandRequest> fPendingCommands;

	BString fProjectRoot;
	BString fFilePath;
	BString fSelection;
	BString fFileText;
};
