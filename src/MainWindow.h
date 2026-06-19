#ifndef MAIN_WINDOW_H
#define MAIN_WINDOW_H

#include "core/EditProposal.h"
#include "core/PatchManager.h"
#include "core/OAuthClient.h"
#include "core/ProjectAttributes.h"
#include "core/ProjectMemory.h"
#include "core/ProjectModel.h"
#include "core/ProjectScanner.h"
#include "core/SettingsStore.h"

#ifdef HAIKODE_CODEX_BRIDGE
#include "core/CodexBridge.h"
#include "core/DiffProposal.h"
#endif

#include <Window.h>

#include <string>

class BButton;
class BFilePanel;
class BListItem;
class BOutlineListView;
class BStringView;
class BTextControl;
class BTextView;
class BDirectory;
struct entry_ref;

class MainWindow final : public BWindow {
public:
	MainWindow();
	~MainWindow() override;

	void MessageReceived(BMessage* message) override;
	bool QuitRequested() override;

private:
	void BuildInterface();
	void ShowFolderPanel();
	void LoadFolder(const entry_ref& ref);
	void AddDirectoryItems(BDirectory& directory, BListItem* parent, int32 depth,
		int32& count);
	void HandleFileSelected();
	void HandleScanProject();
	void ShowScannedFilesInTree();
	void HandleRadarRow(int32 filter);
	void StartLogin();
	void FinishOAuthCode(const char* code, const char* verifier);
	void HandleChatSubmit();
	void HandleApplyProposal();
	void HandleRejectPatch();
	void HandleBuildCommand();
	void HandleTestCommand();
	void HandleCommandApproved(const char* command);
	void HandleStopCodex();
	void FinishCodexRun(const char* output, const char* error, int32 exitCode);
	void FinishCodexLogin(const char* output, const char* error, int32 exitCode);
	void FinishCommandRun(const char* command, const char* output,
		const char* error, int32 exitCode, int32 duration);
	void SetPendingDiff(const std::string& diff);
	void UpdateAuthLabel();
	void UpdateRadarLabels();
	void SetPreviewText(const char* text);
	void AppendLog(const char* text);

	ProjectAttributes fAttributes;
	SettingsStore fSettingsStore;
	ProviderSettings fSettings;
	OAuthToken fToken;
	ProjectModel fProject;
	ProjectMemory fMemory;
	ProjectScanResult fLastScan;
	EditProposal fProposal;
	PatchManager fPatchManager;
	std::string fPendingDiff;
	std::string fPendingPatchPath;
	bool fHasProposal;
	bool fCommandRunning;
#ifdef HAIKODE_CODEX_BRIDGE
	CodexBridge fCodexBridge;
	DiffProposal fDiffProposal;
	bool fHasDiffProposal;
	bool fCodexRunning;
#endif
	std::string fPendingVerifier;
	std::string fPendingState;

	BFilePanel* fFolderPanel;
	BOutlineListView* fFileTree;
	BTextView* fPreviewView;
	BTextView* fOutputView;
	BTextControl* fChatInput;
	BTextControl* fApiKeyInput;
	BStringView* fProjectLabel;
	BStringView* fAuthLabel;
	BButton* fFolderButton;
	BButton* fScanButton;
	BButton* fLoginButton;
	BButton* fStopButton;
	BButton* fSendButton;
	BButton* fApplyButton;
	BButton* fRejectButton;
	BTextControl* fBuildCommand;
	BTextControl* fTestCommand;
	BButton* fBuildButton;
	BButton* fTestButton;
	BButton* fNeedsReviewButton;
	BButton* fTodoButton;
	BButton* fHighRiskButton;
	BButton* fRecentlyChangedButton;
	BButton* fMissingDocsButton;
	BButton* fStaleNotesButton;
};

#endif
