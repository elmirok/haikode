/*
 * Copyright 2026, The Haikode Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include <GroupView.h>
#include <String.h>

#include <memory>
#include <vector>

#include "AIProvider.h"
#include "CancellationToken.h"
#include "CodexBridge.h"
#include "OAuthClient.h"
#include "OpenAICompatibleClient.h"
#include "PanelTabManager.h"
#include "ProcessCapture.h"
#include "UnifiedDiff.h"
#include "VibeCoding.h"

class BButton;
class BMessage;
class BTextControl;
class BTextView;

class AIChatPanel : public BGroupView {
public:
	AIChatPanel(PanelTabManager* panelTabManager, tab_id id);

	void AttachedToWindow() override;
	void DetachedFromWindow() override;
	void MessageReceived(BMessage* message) override;

	void SetActiveContext(const BString& projectRoot, const BString& filePath,
		const BString& selection, const BString& fileText,
		const std::vector<Haikode::AI::ContextFile>& openFiles = {});
	void SetTabLabel(BString label);
	void OpenProviderSettings();

private:
	void _BuildInterface();
	void _LoadProviderFromConfig();
	void _SaveProviderToConfig();
	void _ApplyProviderPreset(Haikode::AI::ProviderPreset preset);
	void _OpenProviderSettings();
	void _TestProvider();
	int64 _BeginRequest();
	bool _IsCurrentRequest(BMessage* message, const char* label);
	void _CancelRequest();
	void _FinishRequest();
	void _SetRequestControlsEnabled(bool enabled);
	void _FinishProviderTest(const BString& text, const BString& error,
		long status);
	void _StartOAuth();
	void _ExchangeOAuthCode();
	void _FinishOAuthExchange(const BString& token, const BString& error,
		long status);
	void _SendPrompt(Haikode::AI::PromptMode mode);
	void _FinishResponse(const BString& text, const BString& error, long status);
	void _QueueCodexLoginStatus();
	void _QueueCodexDeviceLogin();
	void _QueueCodexReadOnlyAsk();
	bool _FindCodexExecutable(std::string& executable, std::string& error) const;
	void _QueueCodexCommand(const Haikode::AI::CommandRequest& command,
		const char* queuedText);
	void _RunCodexCommandCaptured(const Haikode::AI::CommandRequest& command);
	void _RunCommandCaptured(const Haikode::AI::CommandRequest& command);
	void _FinishCodexCapture(const BString& output, const BString& error,
		int32 exitCode, bool timedOut, bool cancelled, const BString& logPath,
		const BString& logError);
	void _FinishCommandCapture(const BString& output, const BString& error,
		int32 exitCode, bool timedOut, bool cancelled, const BString& logPath,
		const BString& logError);
	void _SelectPatchFile(int32 delta);
	void _SelectPatchHunk(int32 delta);
	size_t _SelectedPatchHunkIndex(const std::string& path) const;
	void _SetPatchControlsEnabled(bool enabled);
	void _ApplyPendingDiff();
	void _ApplyFirstPendingFile();
	void _ApplySelectedPendingHunk();
	void _RejectFirstPendingFile();
	void _RejectSelectedPendingHunk();
	void _RejectPendingDiff();
	void _RunPendingCommand();
	void _RejectPendingCommand();
	void _ClearPendingCommands();
	void _ListProjectFiles();
	void _OpenSelectedProjectFile();
	void _ListRecentProjectRecords();
	void _ShowSelectedProjectRecord();
	void _UpdatePendingActions();
	void _SaveSessionRecord(const BString& responseText);
	void _AppendOutput(const char* text);
	Haikode::AI::ProviderSettings _ProviderFromFields() const;
	Haikode::AI::OAuthSettings _OAuthSettingsFromFields() const;
	Haikode::AI::VibeCodingRequest _RequestFromContext(
		Haikode::AI::PromptMode mode) const;

	PanelTabManager* fPanelTabManager;
	tab_id fTabId;

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
	BTextControl* fOAuthCode;
	BTextControl* fPrompt;
	BTextControl* fProjectFilePath;
	BTextControl* fPatchPath;
	BTextControl* fPatchHunk;
	BTextControl* fRecordPath;
	BTextView* fPendingActions;
	BTextView* fOutput;
	BButton* fSaveProvider;
	BButton* fSetupButton;
	BButton* fOpenAIPresetButton;
	BButton* fOllamaPresetButton;
	BButton* fLMStudioPresetButton;
	BButton* fOpenRouterPresetButton;
	BButton* fLlamaCppPresetButton;
	BButton* fTestProviderButton;
	BButton* fStartOAuthButton;
	BButton* fExchangeOAuthButton;
	BButton* fCancelButton;
	BButton* fAskButton;
	BButton* fExplainButton;
	BButton* fSummarizeButton;
	BButton* fPatchButton;
	BButton* fCodexStatusButton;
	BButton* fCodexLoginButton;
	BButton* fCodexAskButton;
	BButton* fPreviousPatchFileButton;
	BButton* fNextPatchFileButton;
	BButton* fPreviousPatchHunkButton;
	BButton* fNextPatchHunkButton;
	BButton* fApplyFirstFileButton;
	BButton* fRejectFirstFileButton;
	BButton* fApplyHunkButton;
	BButton* fRejectHunkButton;
	BButton* fReviewPatchButton;
	BButton* fApplyPatchButton;
	BButton* fRejectPatchButton;
	BButton* fRunCommandButton;
	BButton* fRejectCommandButton;
	BButton* fProjectFilesButton;
	BButton* fOpenProjectFileButton;
	BButton* fRecentRecordsButton;
	BButton* fShowRecordButton;
	bool fRequestRunning;
	int64 fActiveRequestId;
	std::shared_ptr<Haikode::AI::CancellationToken> fActiveCancellation;
	Haikode::AI::UnifiedDiff fPendingDiff;
	BString fPendingRawDiff;
	BString fSavedPendingPatchPath;
	std::vector<Haikode::AI::CommandRequest> fPendingCommands;

	BString fProjectRoot;
	BString fFilePath;
	BString fSelection;
	BString fFileText;
	std::vector<Haikode::AI::ContextFile> fOpenFiles;
	BString fLastUserPrompt;
	Haikode::AI::ProviderSettings fLastProvider;
};
