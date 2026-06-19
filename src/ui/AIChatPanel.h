/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include <GroupView.h>
#include <String.h>

#include "AIProvider.h"
#include "PanelTabManager.h"
#include "VibeCoding.h"

class BButton;
class BTextControl;
class BTextView;

class AIChatPanel : public BGroupView {
public:
	AIChatPanel(PanelTabManager* panelTabManager, tab_id id);

	void AttachedToWindow() override;
	void MessageReceived(BMessage* message) override;

	void SetActiveContext(const BString& projectRoot, const BString& filePath,
		const BString& selection);
	void SetTabLabel(BString label);

private:
	void _BuildInterface();
	void _LoadProviderFromConfig();
	void _SaveProviderToConfig();
	void _BuildPrompt(Haikode::AI::PromptMode mode);
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
	BButton* fAskButton;
	BButton* fPatchButton;

	BString fProjectRoot;
	BString fFilePath;
	BString fSelection;
};
