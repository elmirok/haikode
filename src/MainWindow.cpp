#include "MainWindow.h"

#include "core/ChatRequest.h"
#include "core/CommandSafety.h"
#include "core/Crypto.h"
#include "core/DiffProposal.h"
#include "core/ProcessRunner.h"
#include "core/PromptBuilder.h"
#include "core/ProjectScanner.h"
#include "core/TimeUtils.h"

#ifdef HAIKODE_AI_NETWORK
#include "core/AiProvider.h"
#include "core/ChatClient.h"
#include "core/OAuthCallbackServer.h"
#include "core/OAuthHttpClient.h"
#endif

#include <Application.h>
#include <Button.h>
#include <Directory.h>
#include <Entry.h>
#include <FilePanel.h>
#include <GroupView.h>
#include <LayoutBuilder.h>
#include <ListItem.h>
#include <Message.h>
#include <Messenger.h>
#include <Node.h>
#include <OutlineListView.h>
#include <Path.h>
#ifdef HAIKODE_AI_NETWORK
#include <Roster.h>
#endif
#include <ScrollView.h>
#include <Size.h>
#include <SplitView.h>
#include <String.h>
#include <StringItem.h>
#include <StringView.h>
#include <TextControl.h>
#include <TextView.h>
#include <Alert.h>

#include <ctime>
#include <cstdlib>
#include <sstream>

#include <thread>

static const uint32 kMsgChooseFolder = 'chsf';
static const uint32 kMsgFolderChosen = 'flch';
static const uint32 kMsgFileSelected = 'fsel';
static const uint32 kMsgScanProject = 'scan';
static const uint32 kMsgRadarRow = 'radr';
static const uint32 kMsgLogin = 'logn';
#ifdef HAIKODE_AI_NETWORK
static const uint32 kMsgOAuthCode = 'oa2c';
#endif
static const uint32 kMsgSendChat = 'send';
static const uint32 kMsgApplyProposal = 'aply';
static const uint32 kMsgRejectPatch = 'rjpt';
static const uint32 kMsgBuildCommand = 'bldc';
static const uint32 kMsgTestCommand = 'tstc';
static const uint32 kMsgCommandApproved = 'cmda';
static const uint32 kMsgCommandDone = 'cmdd';
#ifdef HAIKODE_CODEX_BRIDGE
static const uint32 kMsgStopCodex = 'stcx';
static const uint32 kMsgCodexDone = 'cxdn';
static const uint32 kMsgCodexLoginDone = 'cxlg';
#endif

static const int32 kMaxTreeDepth = 5;
static const int32 kMaxTreeItems = 1200;
static const size_t kMaxPreviewBytes = 2 * 1024 * 1024;
static const size_t kMaxAiFileBytes = 200 * 1024;

enum {
	kRadarNeedsReview = 1,
	kRadarTodo,
	kRadarHighRisk,
	kRadarRecentlyChanged,
	kRadarMissingDocs,
	kRadarStaleNotes
};

namespace {

class PathItem final : public BStringItem {
public:
	PathItem(const char* label, const char* path, bool isDirectory)
		:
		BStringItem(label),
		fPath(path == nullptr ? "" : path),
		fIsDirectory(isDirectory)
	{
	}

	const std::string& Path() const { return fPath; }
	bool IsDirectory() const { return fIsDirectory; }

private:
	std::string fPath;
	bool fIsDirectory;
};

bool
hasPrefix(const char* text, const char* prefix)
{
	if (text == nullptr || prefix == nullptr)
		return false;
	while (*prefix != '\0') {
		if (*text++ != *prefix++)
			return false;
	}
	return true;
}

BMessage*
radarMessage(int32 filter)
{
	BMessage* message = new BMessage(kMsgRadarRow);
	message->AddInt32("filter", filter);
	return message;
}

}

MainWindow::MainWindow()
	:
	BWindow(BRect(100, 100, 1120, 760), "Haikode", B_TITLED_WINDOW,
		B_ASYNCHRONOUS_CONTROLS | B_QUIT_ON_WINDOW_CLOSE),
	fPendingDiff(),
	fPendingPatchPath(),
	fHasProposal(false),
	fCommandRunning(false),
#ifdef HAIKODE_CODEX_BRIDGE
	fCodexBridge("codex"),
	fHasDiffProposal(false),
	fCodexRunning(false),
#endif
	fFolderPanel(nullptr),
	fFileTree(nullptr),
	fPreviewView(nullptr),
	fOutputView(nullptr),
	fChatInput(nullptr),
	fProjectLabel(nullptr),
	fAuthLabel(nullptr),
	fFolderButton(nullptr),
	fScanButton(nullptr),
	fLoginButton(nullptr),
	fStopButton(nullptr),
	fSendButton(nullptr),
	fApplyButton(nullptr),
	fRejectButton(nullptr),
	fBuildCommand(nullptr),
	fTestCommand(nullptr),
	fBuildButton(nullptr),
	fTestButton(nullptr),
	fNeedsReviewButton(nullptr),
	fTodoButton(nullptr),
	fHighRiskButton(nullptr),
	fRecentlyChangedButton(nullptr),
	fMissingDocsButton(nullptr),
	fStaleNotesButton(nullptr)
{
	std::string error;
	fSettingsStore.EnsureDirectory(error);
	if (!fSettingsStore.LoadProvider(fSettings))
		fSettingsStore.SaveProvider(fSettings, error);
	fSettingsStore.LoadToken(fToken);
#ifdef HAIKODE_CODEX_BRIDGE
	std::string codexPath;
	if (!fSettingsStore.LoadCodexPath(codexPath)) {
		codexPath = "codex";
		fSettingsStore.SaveCodexPath(codexPath, error);
	}
	fCodexBridge.SetCodexPath(codexPath);
#endif

	BuildInterface();
	UpdateAuthLabel();
	AppendLog("Haikode started. Open a folder, select a file, then ask a question.");
	AppendLog("No shell commands are executed by Haikode.");
}

MainWindow::~MainWindow()
{
	delete fFolderPanel;
}

void
MainWindow::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kMsgChooseFolder:
			ShowFolderPanel();
			break;

		case kMsgFolderChosen:
		{
			entry_ref ref;
			if (message->FindRef("refs", &ref) == B_OK)
				LoadFolder(ref);
			break;
		}

		case kMsgFileSelected:
			HandleFileSelected();
			break;

		case kMsgScanProject:
			HandleScanProject();
			break;

		case kMsgRadarRow:
		{
			int32 filter = 0;
			if (message->FindInt32("filter", &filter) == B_OK)
				HandleRadarRow(filter);
			break;
		}

		case kMsgLogin:
			StartLogin();
			break;

#ifdef HAIKODE_CODEX_BRIDGE
		case kMsgStopCodex:
			HandleStopCodex();
			break;

		case kMsgCodexDone:
		{
			const char* output = "";
			const char* error = "";
			int32 exitCode = -1;
			message->FindString("output", &output);
			message->FindString("error", &error);
			message->FindInt32("exit_code", &exitCode);
			FinishCodexRun(output, error, exitCode);
			break;
		}

		case kMsgCodexLoginDone:
		{
			const char* output = "";
			const char* error = "";
			int32 exitCode = -1;
			message->FindString("output", &output);
			message->FindString("error", &error);
			message->FindInt32("exit_code", &exitCode);
			FinishCodexLogin(output, error, exitCode);
			break;
		}
#endif

#ifdef HAIKODE_AI_NETWORK
		case kMsgOAuthCode:
		{
			const char* code = nullptr;
			const char* verifier = nullptr;
			const char* error = nullptr;
			if (message->FindString("error", &error) == B_OK) {
				AppendLog(error);
				break;
			}
			if (message->FindString("code", &code) == B_OK
				&& message->FindString("verifier", &verifier) == B_OK) {
				FinishOAuthCode(code, verifier);
			}
			break;
		}
#endif

		case kMsgSendChat:
			HandleChatSubmit();
			break;

		case kMsgApplyProposal:
			HandleApplyProposal();
			break;

		case kMsgRejectPatch:
			HandleRejectPatch();
			break;

		case kMsgBuildCommand:
			HandleBuildCommand();
			break;

		case kMsgTestCommand:
			HandleTestCommand();
			break;

		case kMsgCommandApproved:
		{
			const char* command = nullptr;
			if (message->FindString("command", &command) == B_OK)
				HandleCommandApproved(command);
			break;
		}

		case kMsgCommandDone:
		{
			const char* command = "";
			const char* output = "";
			const char* error = "";
			int32 exitCode = -1;
			int32 duration = 0;
			message->FindString("command", &command);
			message->FindString("output", &output);
			message->FindString("error", &error);
			message->FindInt32("exit_code", &exitCode);
			message->FindInt32("duration", &duration);
			FinishCommandRun(command, output, error, exitCode, duration);
			break;
		}

		default:
			BWindow::MessageReceived(message);
			break;
	}
}

bool
MainWindow::QuitRequested()
{
	be_app->PostMessage(B_QUIT_REQUESTED);
	return true;
}

void
MainWindow::BuildInterface()
{
	fFolderButton = new BButton("open_folder", "Open Folder",
		new BMessage(kMsgChooseFolder));
	fScanButton = new BButton("scan_project", "Scan Project",
		new BMessage(kMsgScanProject));
	fScanButton->SetEnabled(false);
	fLoginButton = new BButton("login",
#ifdef HAIKODE_CODEX_BRIDGE
		"Codex Login",
#else
		"AI Setup",
#endif
		new BMessage(kMsgLogin));
#ifdef HAIKODE_CODEX_BRIDGE
	fStopButton = new BButton("stop_codex", "Stop",
		new BMessage(kMsgStopCodex));
	fStopButton->SetEnabled(false);
#endif
	fProjectLabel = new BStringView("project_label", "No folder selected");
	fAuthLabel = new BStringView("auth_label", "Not logged in");

	fFileTree = new BOutlineListView("file_tree", B_SINGLE_SELECTION_LIST);
	fFileTree->SetSelectionMessage(new BMessage(kMsgFileSelected));
	fFileTree->SetExplicitMinSize(BSize(260, 260));
	BScrollView* fileScroll = new BScrollView("file_tree_scroll", fFileTree,
		B_FOLLOW_ALL, 0, false, true);

	fPreviewView = new BTextView("file_preview");
	fPreviewView->MakeEditable(false);
	fPreviewView->SetStylable(false);
	fPreviewView->SetText("Select a text file to preview it.");
	BScrollView* previewScroll = new BScrollView("file_preview_scroll",
		fPreviewView, B_FOLLOW_ALL, 0, true, true);

	fOutputView = new BTextView("output_log");
	fOutputView->MakeEditable(false);
	fOutputView->SetStylable(false);
	BScrollView* outputScroll = new BScrollView("output_log_scroll", fOutputView,
		B_FOLLOW_ALL, 0, true, true);

	fChatInput = new BTextControl("chat_input", nullptr, "",
		new BMessage(kMsgSendChat));
	fChatInput->SetExplicitMinSize(BSize(320, B_SIZE_UNSET));
	fSendButton = new BButton("send", "Send", new BMessage(kMsgSendChat));
	fApplyButton = new BButton("apply", "Apply Proposed Edit",
		new BMessage(kMsgApplyProposal));
	fApplyButton->SetEnabled(false);
	fRejectButton = new BButton("reject_patch", "Reject Patch",
		new BMessage(kMsgRejectPatch));
	fRejectButton->SetEnabled(false);
	fBuildCommand = new BTextControl("build_command", "Build", "make",
		new BMessage(kMsgBuildCommand));
	fTestCommand = new BTextControl("test_command", "Test", "make test",
		new BMessage(kMsgTestCommand));
	fBuildButton = new BButton("run_build", "Run Build",
		new BMessage(kMsgBuildCommand));
	fTestButton = new BButton("run_test", "Run Test",
		new BMessage(kMsgTestCommand));

	fNeedsReviewButton = new BButton("radar_needs_review", "Needs review: 0",
		radarMessage(kRadarNeedsReview));
	fTodoButton = new BButton("radar_todo", "TODO files: 0",
		radarMessage(kRadarTodo));
	fHighRiskButton = new BButton("radar_high_risk", "High-risk files: 0",
		radarMessage(kRadarHighRisk));
	fRecentlyChangedButton = new BButton("radar_recent", "Recently changed: 0",
		radarMessage(kRadarRecentlyChanged));
	fMissingDocsButton = new BButton("radar_missing_docs", "Missing docs: 0",
		radarMessage(kRadarMissingDocs));
	fStaleNotesButton = new BButton("radar_stale", "AI notes stale: 0",
		radarMessage(kRadarStaleNotes));

	BGroupView* radarPanel = new BGroupView(B_VERTICAL);
	BLayoutBuilder::Group<>(radarPanel, B_VERTICAL, B_USE_DEFAULT_SPACING)
		.Add(new BStringView("radar_title", "Project Radar"))
		.Add(fNeedsReviewButton)
		.Add(fTodoButton)
		.Add(fHighRiskButton)
		.Add(fRecentlyChangedButton)
		.Add(fMissingDocsButton)
		.Add(fStaleNotesButton);

	BGroupView* leftPane = new BGroupView(B_VERTICAL);
	BLayoutBuilder::Group<>(leftPane, B_VERTICAL, B_USE_DEFAULT_SPACING)
		.Add(radarPanel)
		.Add(fileScroll);

	BGroupView* rightPane = new BGroupView(B_VERTICAL);
	BSplitView* verticalSplit = new BSplitView(B_VERTICAL);
	verticalSplit->AddChild(previewScroll);
	verticalSplit->AddChild(outputScroll);
	verticalSplit->SetCollapsible(0, false);
	verticalSplit->SetCollapsible(1, false);

	BLayoutBuilder::Group<>(rightPane, B_VERTICAL, B_USE_DEFAULT_SPACING)
		.Add(verticalSplit)
		.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
			.Add(fChatInput)
			.Add(fSendButton)
			.Add(fApplyButton)
			.Add(fRejectButton)
		.End();

	BSplitView* splitView = new BSplitView(B_HORIZONTAL);
	splitView->AddChild(leftPane);
	splitView->AddChild(rightPane);
	splitView->SetCollapsible(0, false);
	splitView->SetCollapsible(1, false);

	BLayoutBuilder::Group<>(this, B_VERTICAL, B_USE_DEFAULT_SPACING)
		.SetInsets(B_USE_WINDOW_SPACING)
		.AddGroup(B_HORIZONTAL, B_USE_DEFAULT_SPACING)
			.Add(fFolderButton)
			.Add(fScanButton)
			.Add(fLoginButton)
#ifdef HAIKODE_CODEX_BRIDGE
			.Add(fStopButton)
#endif
			.Add(fBuildCommand)
			.Add(fBuildButton)
			.Add(fTestCommand)
			.Add(fTestButton)
			.Add(fAuthLabel)
			.Add(fProjectLabel)
		.End()
		.Add(splitView);
}

void
MainWindow::ShowFolderPanel()
{
	if (fFolderPanel == nullptr) {
		BMessenger target(this);
		fFolderPanel = new BFilePanel(B_OPEN_PANEL, &target, nullptr,
			B_DIRECTORY_NODE, false, new BMessage(kMsgFolderChosen));
		fFolderPanel->Window()->SetTitle("Open Haikode Project Folder");
	}

	fFolderPanel->Show();
}

void
MainWindow::LoadFolder(const entry_ref& ref)
{
	BEntry entry(&ref, true);
	if (!entry.IsDirectory()) {
		AppendLog("Selected item is not a folder.");
		return;
	}

	BPath path;
	if (entry.GetPath(&path) != B_OK) {
		AppendLog("Could not read selected folder path.");
		return;
	}

	if (!fProject.SetRoot(path.Path())) {
		AppendLog("Could not use selected folder as a project root.");
		return;
	}

	std::string memoryError;
	if (!fMemory.Open(path.Path(), memoryError)) {
		BString log("Could not initialize .haikode project memory: ");
		log << memoryError.c_str();
		AppendLog(log.String());
	} else {
		fBuildCommand->SetText(fMemory.DefaultBuildCommand().empty()
			? "make" : fMemory.DefaultBuildCommand().c_str());
		fTestCommand->SetText(fMemory.DefaultTestCommand().empty()
			? "make test" : fMemory.DefaultTestCommand().c_str());
	}

	BDirectory directory(&entry);
	if (directory.InitCheck() != B_OK) {
		AppendLog("Could not open selected folder.");
		return;
	}

	fFileTree->MakeEmpty();
	fProjectLabel->SetText(path.Path());
	SetPreviewText("Select a text file to preview it.");
	fLastScan = ProjectScanResult();
	fPendingDiff.clear();
	fPendingPatchPath.clear();
	UpdateRadarLabels();
	fHasProposal = false;
	fApplyButton->SetEnabled(false);
	fApplyButton->SetLabel("Apply Proposed Edit");
	fRejectButton->SetEnabled(false);
	fScanButton->SetEnabled(true);

	BString rootLabel(path.Leaf());
	if (rootLabel.IsEmpty())
		rootLabel = path.Path();
	rootLabel << "/";

	PathItem* rootItem = new PathItem(rootLabel.String(), path.Path(), true);
	fFileTree->AddItem(rootItem);

	int32 count = 1;
	AddDirectoryItems(directory, rootItem, 0, count);
	fFileTree->Expand(rootItem);

	BString log("Opened folder: ");
	log << path.Path();
	if (count >= kMaxTreeItems)
		log << "\nFile tree stopped at the current safety limit.";
	AppendLog(log.String());
}

void
MainWindow::AddDirectoryItems(BDirectory& directory, BListItem* parent,
	int32 depth, int32& count)
{
	if (depth > kMaxTreeDepth || count >= kMaxTreeItems)
		return;

	BEntry entry;
	while (directory.GetNextEntry(&entry, false) == B_OK) {
		if (count >= kMaxTreeItems)
			return;

		char name[B_FILE_NAME_LENGTH];
		if (entry.GetName(name) != B_OK)
			continue;

		BPath path;
		if (entry.GetPath(&path) != B_OK)
			continue;
		if (!fProject.RootPath().empty()) {
			std::string relative = path.Path();
			if (relative.compare(0, fProject.RootPath().size(),
				fProject.RootPath()) == 0) {
				relative = relative.substr(fProject.RootPath().size());
				if (!relative.empty() && relative[0] == '/')
					relative = relative.substr(1);
				if (fMemory.Rules().ShouldIgnore(relative))
					continue;
			}
		}

		const bool isDirectory = entry.IsDirectory();
		BString label(name);
		if (isDirectory)
			label << "/";

		PathItem* item = new PathItem(label.String(), path.Path(), isDirectory);
		if (parent != nullptr)
			fFileTree->AddUnder(item, parent);
		else
			fFileTree->AddItem(item);

		count++;

		if (isDirectory && depth < kMaxTreeDepth) {
			BDirectory child(&entry);
			if (child.InitCheck() == B_OK)
				AddDirectoryItems(child, item, depth + 1, count);
		}
	}
}

void
MainWindow::HandleFileSelected()
{
	const int32 index = fFileTree->CurrentSelection();
	if (index < 0)
		return;

	PathItem* item = dynamic_cast<PathItem*>(fFileTree->ItemAt(index));
	if (item == nullptr || item->IsDirectory())
		return;

	if (!fProject.SelectFile(item->Path())) {
		SetPreviewText("Could not select this file safely.");
		return;
	}

	std::string text;
	if (!fProject.ReadSelectedText(kMaxPreviewBytes, text)) {
		SetPreviewText("This file is empty, binary, too large, or unsupported.");
		return;
	}

	SetPreviewText(text.c_str());
	fHasProposal = false;
	fApplyButton->SetEnabled(false);
	fApplyButton->SetLabel("Apply Proposed Edit");

	BString log("Selected file: ");
	log << fProject.SelectedRelativePath().c_str();
	AppendLog(log.String());
}

void
MainWindow::HandleScanProject()
{
	if (fProject.RootPath().empty()) {
		AppendLog("Open a project folder before scanning.");
		return;
	}

	AppendLog("Scanning project and writing haikode:* BFS attributes...");

	ProjectScanner scanner;
	fLastScan = scanner.Scan(fProject.RootPath(), fMemory.Rules());

	int writeFailures = 0;
	std::string firstError;
	for (const ProjectFileMetadata& file : fLastScan.files) {
		std::string error;
		if (!fAttributes.WriteMetadata(file, error)) {
			++writeFailures;
			if (firstError.empty())
				firstError = error;
		}
	}

	UpdateRadarLabels();
	std::string memoryError;
	if (!fMemory.UpdateFiles(fLastScan.files, memoryError)) {
		BString memoryLog("Could not update .haikode/project.json: ");
		memoryLog << memoryError.c_str();
		AppendLog(memoryLog.String());
	}

	BString log("Scan complete: ");
	log << static_cast<int32>(fLastScan.files.size()) << " files indexed.";
	if (writeFailures > 0) {
		log << " Attribute write failures: " << writeFailures;
		if (!firstError.empty()) {
			log << " First error: ";
			log << firstError.c_str();
		}
	}
	AppendLog(log.String());
	ShowScannedFilesInTree();
}

void
MainWindow::ShowScannedFilesInTree()
{
	if (fProject.RootPath().empty())
		return;

	fFileTree->MakeEmpty();

	BString rootLabel("Scanned files: ");
	rootLabel << static_cast<int32>(fLastScan.files.size());
	PathItem* rootItem = new PathItem(rootLabel.String(), fProject.RootPath().c_str(),
		true);
	fFileTree->AddItem(rootItem);

	for (const ProjectFileMetadata& file : fLastScan.files) {
		PathItem* item = new PathItem(file.relativePath.c_str(), file.path.c_str(),
			false);
		fFileTree->AddUnder(item, rootItem);
	}

	fFileTree->Expand(rootItem);
	if (!fLastScan.files.empty()) {
		BString hint("Scanned files are now listed in the left pane. Select one to preview it.");
		AppendLog(hint.String());
	} else {
		AppendLog("Scan found no text/source files. Hidden, generated, binary, and oversized files are skipped.");
	}
}

void
MainWindow::HandleRadarRow(int32 filter)
{
	const char* title = "Project Radar";
	BString matches;
	int count = 0;

	for (const ProjectFileMetadata& file : fLastScan.files) {
		bool include = false;
		switch (filter) {
			case kRadarNeedsReview:
				title = "Needs review";
				include = file.needsReview;
				break;
			case kRadarTodo:
				title = "TODO files";
				include = file.hasTodo;
				break;
			case kRadarHighRisk:
				title = "High-risk files";
				include = file.highRisk;
				break;
			case kRadarRecentlyChanged:
				title = "Recently changed";
				include = file.recentlyChanged;
				break;
			case kRadarMissingDocs:
				title = "Missing docs";
				include = file.missingDocs;
				break;
			case kRadarStaleNotes:
				title = "AI notes stale";
				include = file.aiNotesStale;
				break;
			default:
				break;
		}

		if (!include)
			continue;

		if (count < 40) {
			matches << "\n";
			matches << file.relativePath.c_str();
		}
		++count;
	}

	BString log(title);
	log << ": " << count << " file";
	if (count != 1)
		log << "s";
	if (count > 0)
		log << matches;
	if (count > 40)
		log << "\n...";
	AppendLog(log.String());
}

void
MainWindow::StartLogin()
{
#ifdef HAIKODE_CODEX_BRIDGE
	if (fCodexRunning) {
		AppendLog("Codex is already running.");
		return;
	}

	AppendLog("Starting Codex device login through the Codex CLI.");
	AppendLog("If no code appears here before timeout, run this in Terminal: codex login --device-auth");
	fCodexRunning = true;
	fLoginButton->SetEnabled(false);
	fSendButton->SetEnabled(false);
	fStopButton->SetEnabled(true);
	UpdateAuthLabel();

	BMessenger messenger(this);
	CodexBridge* bridge = &fCodexBridge;
	std::thread([messenger, bridge]() {
		ProcessResult result;
		std::string error;
		bridge->StartDeviceLogin(result, error);
		BMessage message(kMsgCodexLoginDone);
		message.AddString("output", result.stdoutText.c_str());
		message.AddString("error", error.empty() ? result.stderrText.c_str()
			: error.c_str());
		message.AddInt32("exit_code", result.exitCode);
		messenger.SendMessage(&message);
	}).detach();
#else
#ifndef HAIKODE_AI_NETWORK
	AppendLog("AI is disabled in this build.");
	AppendLog("To enable OpenAI-compatible AI on Haiku, install packages and rebuild:");
	AppendLog("pkgman install curl_devel jsoncpp_devel openssl_devel");
	AppendLog("make clean");
	AppendLog("make AI_NETWORK=1");
	AppendLog("Then set an API key before running Haikode:");
	AppendLog("export HAIKODE_API_KEY=your_api_key");
#else
	const char* haikodeKey = getenv("HAIKODE_API_KEY");
	const char* openaiKey = getenv("OPENAI_API_KEY");
	if ((haikodeKey != nullptr && haikodeKey[0] != '\0')
		|| (openaiKey != nullptr && openaiKey[0] != '\0')) {
		AppendLog("AI API key found. Select a file, type a prompt, and click Send.");
		return;
	}

	AppendLog("AI network support is enabled, but no API key was found.");
	AppendLog("Set HAIKODE_API_KEY or OPENAI_API_KEY before launching Haikode.");
	AppendLog("Example: export HAIKODE_API_KEY=your_api_key");
	AppendLog("Optional provider defaults are read from:");
	AppendLog(fSettingsStore.ProviderPath().c_str());
	return;
#endif
#endif
}

void
MainWindow::FinishOAuthCode(const char* code, const char* verifier)
{
#ifndef HAIKODE_AI_NETWORK
	(void)code;
	(void)verifier;
	AppendLog("Network AI support was not enabled at build time. Rebuild with: make AI_NETWORK=1");
#else
	OAuthHttpClient client;
	OAuthToken token;
	std::string error;
	if (!client.ExchangeCode(fSettings, code == nullptr ? "" : code,
		verifier == nullptr ? "" : verifier, token, error)) {
		AppendLog(error.c_str());
		return;
	}

	fToken = token;
	fSettingsStore.SaveToken(fToken, error);
	UpdateAuthLabel();
	AppendLog("OAuth login complete.");
#endif
}

void
MainWindow::HandleChatSubmit()
{
	const char* prompt = fChatInput->Text();
	if (prompt == nullptr || prompt[0] == '\0')
		return;

	if (hasPrefix(prompt, "/oauth ")) {
		const char* code = prompt + 7;
		FinishOAuthCode(code, fPendingVerifier.c_str());
		fChatInput->SetText("");
		return;
	}

#ifdef HAIKODE_CODEX_BRIDGE
	if (fProject.RootPath().empty()) {
		AppendLog("Open a project folder before asking Codex.");
		return;
	}
	if (fCodexRunning) {
		AppendLog("Codex is already running.");
		return;
	}

	std::string selectedText;
	if (!fProject.SelectedPath().empty())
		fProject.ReadSelectedText(kMaxAiFileBytes, selectedText);

	std::string requestText;
	requestText += "Project root: " + fProject.RootPath() + "\n";
	if (!fProject.SelectedRelativePath().empty()) {
		requestText += "Selected file: " + fProject.SelectedRelativePath() + "\n\n";
		requestText += "Selected file content:\n```text\n";
		requestText += selectedText;
		requestText += "\n```\n\n";
	}
	requestText += "User request:\n";
	requestText += prompt;

	BString userLine("You: ");
	userLine << prompt;
	AppendLog(userLine.String());
	AppendLog("Running Codex in read-only sandbox mode. Haikode will not apply changes automatically.");

	fHasProposal = false;
	fHasDiffProposal = false;
	fApplyButton->SetEnabled(false);
	fCodexRunning = true;
	fLoginButton->SetEnabled(false);
	fSendButton->SetEnabled(false);
	fStopButton->SetEnabled(true);
	UpdateAuthLabel();

	BMessenger messenger(this);
	CodexBridge* bridge = &fCodexBridge;
	const std::string root = fProject.RootPath();
	std::thread([messenger, bridge, root, requestText]() {
		ProcessResult result;
		std::string error;
		bridge->RunReadOnlyPrompt(root, requestText, result, error);
		BMessage message(kMsgCodexDone);
		message.AddString("output", result.stdoutText.c_str());
		message.AddString("error", error.empty() ? result.stderrText.c_str()
			: error.c_str());
		message.AddInt32("exit_code", result.exitCode);
		messenger.SendMessage(&message);
	}).detach();

	fChatInput->SetText("");
	return;
#else
#ifndef HAIKODE_AI_NETWORK
	BString userLine("You: ");
	userLine << prompt;
	AppendLog(userLine.String());
	AppendLog("AI is disabled in this build. Click AI Setup for build instructions.");
	fChatInput->SetText("");
	return;
#else
	std::string selectedText;
	if (!fProject.SelectedPath().empty())
		fProject.ReadSelectedText(kMaxAiFileBytes, selectedText);

	PromptContext promptContext;
	promptContext.mode = PromptMode::ExplainSelectedFile;
	promptContext.projectRoot = fProject.RootPath();
	promptContext.userPrompt = prompt;
	if (!fProject.SelectedRelativePath().empty())
		promptContext.files.push_back({fProject.SelectedRelativePath(),
			selectedText, false});
	PromptBuilder promptBuilder;
	PromptBuildResult promptResult = promptBuilder.Build(promptContext,
		200 * 1024, 10);
	for (const std::string& warning : promptResult.warnings)
		AppendLog(warning.c_str());

	ChatRequest request;
	request.model = fSettings.model.empty() ? "gpt-4.1-mini" : fSettings.model;
	request.projectRoot = fProject.RootPath();
	request.selectedPath = fProject.SelectedRelativePath();
	request.selectedText = selectedText;
	request.userPrompt = promptResult.prompt;

	BString userLine("You: ");
	userLine << prompt;
	AppendLog(userLine.String());

	std::string response;
	std::string error;
	ProviderSettings settings = fSettings;
	if (settings.baseUrl.empty())
		settings.baseUrl = "https://api.openai.com";
	OpenAICompatibleProvider provider;
	if (!provider.Send(settings, request, response, error)) {
		AppendLog(error.c_str());
		return;
	}

	BString assistantLine("Haikode: ");
	assistantLine << response.c_str();
	AppendLog(assistantLine.String());

	EditProposal proposal;
	if (EditProposal::ParseFromResponse(response, proposal)
		&& proposal.IsForSelectedFile(fProject)) {
		fProposal = proposal;
		fHasProposal = true;
		fApplyButton->SetEnabled(true);
		BString editLine("Proposed edit ready: ");
		editLine << fProposal.summary.c_str();
		AppendLog(editLine.String());
	} else {
		DiffProposal diff;
		if (DiffProposal::ParseUnifiedDiff(response, diff))
			SetPendingDiff(response);
		else {
			fHasProposal = false;
			fApplyButton->SetEnabled(false);
		}
	}

	fChatInput->SetText("");
#endif
#endif
}

void
MainWindow::HandleApplyProposal()
{
	if (!fPendingDiff.empty()) {
		PatchApplyResult result;
		std::string error;
		if (!fPatchManager.Apply(fProject.RootPath(), fPendingDiff, result, error)) {
			AppendLog(error.c_str());
			return;
		}
		fPendingDiff.clear();
		fPendingPatchPath.clear();
		fApplyButton->SetEnabled(false);
		fApplyButton->SetLabel("Apply Proposed Edit");
		fRejectButton->SetEnabled(false);

		BString log("Applied patch. Backup directory: ");
		log << result.backupDirectory.c_str();
		AppendLog(log.String());
		return;
	}

	if (!fHasProposal) {
#ifdef HAIKODE_CODEX_BRIDGE
		if (fHasDiffProposal) {
			AppendLog("A unified diff proposal is available for review, but automatic multi-file patch apply is not enabled in this slice.");
			AppendLog("Ask Codex for a haikode-edit block for the selected file to use the guarded Apply flow.");
			return;
		}
#endif
		AppendLog("No valid edit proposal is ready to apply.");
		return;
	}

	std::string error;
	if (!fProposal.Apply(fProject, error)) {
		AppendLog(error.c_str());
		return;
	}

	std::string text;
	fProject.ReadSelectedText(kMaxPreviewBytes, text);
	SetPreviewText(text.c_str());
	fHasProposal = false;
	fApplyButton->SetEnabled(false);
	fApplyButton->SetLabel("Apply Proposed Edit");
	AppendLog("Applied proposed edit and wrote a .haikode.bak backup.");
}

void
MainWindow::HandleRejectPatch()
{
	if (fPendingDiff.empty()) {
		AppendLog("No patch is ready to reject.");
		return;
	}
	fPendingDiff.clear();
	fPendingPatchPath.clear();
	fApplyButton->SetEnabled(false);
	fApplyButton->SetLabel("Apply Proposed Edit");
	fRejectButton->SetEnabled(false);
	AppendLog("Rejected pending patch.");
}

void
MainWindow::HandleBuildCommand()
{
	HandleCommandApproved(fBuildCommand == nullptr ? "" : fBuildCommand->Text());
}

void
MainWindow::HandleTestCommand()
{
	HandleCommandApproved(fTestCommand == nullptr ? "" : fTestCommand->Text());
}

void
MainWindow::HandleCommandApproved(const char* commandText)
{
	if (fProject.RootPath().empty()) {
		AppendLog("Open a project before running a command.");
		return;
	}
	if (fCommandRunning) {
		AppendLog("A command is already running.");
		return;
	}

	const std::string command = commandText == nullptr ? "" : commandText;
	std::vector<std::string> argv;
	std::string error;
	if (!CommandSafety::SplitCommand(command, argv, error)) {
		AppendLog(error.c_str());
		return;
	}

	CommandSafety safety;
	CommandSafetyResult safetyResult = safety.Check(command);
	BString prompt("Run this command inside the project?\n\n");
	prompt << command.c_str();
	if (!safetyResult.warnings.empty()) {
		prompt << "\n\nWarnings:";
		for (const std::string& warning : safetyResult.warnings) {
			prompt << "\n";
			prompt << warning.c_str();
		}
	}

	BAlert* alert = new BAlert("Confirm command", prompt.String(), "Cancel",
		safetyResult.requiresStrongConfirmation ? "Run Anyway" : "Run", nullptr,
		B_WIDTH_AS_USUAL, B_WARNING_ALERT);
	if (alert->Go() != 1) {
		AppendLog("Command cancelled.");
		return;
	}

	if (fBuildCommand != nullptr && command == fBuildCommand->Text())
		fMemory.SetDefaultBuildCommand(command, error);
	if (fTestCommand != nullptr && command == fTestCommand->Text())
		fMemory.SetDefaultTestCommand(command, error);

	fCommandRunning = true;
	AppendLog("Running approved command...");

	BMessenger messenger(this);
	const std::string root = fProject.RootPath();
	std::thread([messenger, root, command, argv]() {
		ProcessRunner runner;
		ProcessRequest request;
		request.argv = argv;
		request.workingDirectory = root;
		ProcessResult result;
		std::string runError;
		const time_t start = time(nullptr);
		runner.RunInsideProject(root, request, result, runError);
		const int32 duration = static_cast<int32>(time(nullptr) - start);
		BMessage message(kMsgCommandDone);
		message.AddString("command", command.c_str());
		message.AddString("output", result.stdoutText.c_str());
		message.AddString("error", runError.empty() ? result.stderrText.c_str()
			: runError.c_str());
		message.AddInt32("exit_code", result.exitCode);
		message.AddInt32("duration", duration);
		messenger.SendMessage(&message);
	}).detach();
}

void
MainWindow::FinishCommandRun(const char* command, const char* output,
	const char* error, int32 exitCode, int32 duration)
{
	fCommandRunning = false;
	BString log("Command finished with exit code ");
	log << exitCode << " in " << duration << "s";
	AppendLog(log.String());
	if (output != nullptr && output[0] != '\0')
		AppendLog(output);
	if (error != nullptr && error[0] != '\0')
		AppendLog(error);

	std::ostringstream saved;
	saved << "timestamp=" << TimeUtils::IsoTimestamp() << "\n"
		<< "working_directory=" << fProject.RootPath() << "\n"
		<< "command=" << (command == nullptr ? "" : command) << "\n"
		<< "exit_code=" << exitCode << "\n"
		<< "duration=" << duration << "\n"
		<< "\nstdout\n" << (output == nullptr ? "" : output)
		<< "\nstderr\n" << (error == nullptr ? "" : error);
	std::string path;
	std::string saveError;
	if (fMemory.SaveCommandLog(saved.str(), path, saveError)) {
		BString savedLog("Saved command log: ");
		savedLog << path.c_str();
		AppendLog(savedLog.String());
	} else {
		AppendLog(saveError.c_str());
	}
}

void
MainWindow::HandleStopCodex()
{
#ifdef HAIKODE_CODEX_BRIDGE
	if (!fCodexRunning) {
		AppendLog("Codex is not running.");
		return;
	}
	fCodexBridge.Cancel();
	AppendLog("Stop requested for the running Codex job.");
#else
	AppendLog("Codex bridge was not enabled at build time.");
#endif
}

void
MainWindow::FinishCodexRun(const char* output, const char* error, int32 exitCode)
{
#ifdef HAIKODE_CODEX_BRIDGE
	fCodexRunning = false;
	fLoginButton->SetEnabled(true);
	fSendButton->SetEnabled(true);
	fStopButton->SetEnabled(false);
	UpdateAuthLabel();

	if (error != nullptr && error[0] != '\0') {
		BString errorLine("Codex error: ");
		errorLine << error;
		AppendLog(errorLine.String());
	}

	BString done("Codex finished with exit code ");
	done << exitCode;
	AppendLog(done.String());

	if (output != nullptr && output[0] != '\0') {
		BString assistantLine("Codex: ");
		assistantLine << output;
		AppendLog(assistantLine.String());
	}

	const std::string response = output == nullptr ? "" : output;
	EditProposal proposal;
	if (EditProposal::ParseFromResponse(response, proposal)
		&& proposal.IsForSelectedFile(fProject)) {
		fProposal = proposal;
		fHasProposal = true;
		fHasDiffProposal = false;
		fApplyButton->SetEnabled(true);
		BString editLine("Proposed single-file edit ready: ");
		editLine << fProposal.summary.c_str();
		AppendLog(editLine.String());
		return;
	}

	DiffProposal diff;
	std::string validationError;
	if (DiffProposal::ParseUnifiedDiff(response, diff)) {
		if (diff.ValidatePaths(fProject.RootPath(), validationError)) {
			fDiffProposal = diff;
			fHasDiffProposal = true;
			fHasProposal = false;
			BString diffLine("Unified diff proposal ready for review: ");
			diffLine << static_cast<int32>(diff.Files().size()) << " file";
			if (diff.Files().size() != 1)
				diffLine << "s";
			AppendLog(diffLine.String());
			SetPendingDiff(response);
		} else {
			fHasDiffProposal = false;
			AppendLog(validationError.c_str());
		}
	} else {
		fHasProposal = false;
		fHasDiffProposal = false;
		fApplyButton->SetEnabled(false);
	}
#else
	(void)output;
	(void)error;
	(void)exitCode;
#endif
}

void
MainWindow::FinishCodexLogin(const char* output, const char* error, int32 exitCode)
{
#ifdef HAIKODE_CODEX_BRIDGE
	fCodexRunning = false;
	fLoginButton->SetEnabled(true);
	fSendButton->SetEnabled(true);
	fStopButton->SetEnabled(false);
	UpdateAuthLabel();

	if (output != nullptr && output[0] != '\0')
		AppendLog(output);
	if (error != nullptr && error[0] != '\0')
		AppendLog(error);

	BString done("Codex login command finished with exit code ");
	done << exitCode;
	AppendLog(done.String());
	AppendLog("If login did not complete, run this in Terminal: codex login --device-auth");
#else
	(void)output;
	(void)error;
	(void)exitCode;
#endif
}

void
MainWindow::SetPendingDiff(const std::string& diff)
{
	if (fProject.RootPath().empty()) {
		AppendLog("Open a project before accepting a patch.");
		return;
	}

	PatchValidation validation;
	std::string error;
	if (!fPatchManager.Validate(fProject.RootPath(), diff, validation, error)) {
		AppendLog(error.c_str());
		return;
	}

	std::string patchPath;
	if (!fPatchManager.SavePatch(fProject.RootPath(), diff, patchPath, error)) {
		AppendLog(error.c_str());
		return;
	}

	fPendingDiff = diff;
	fPendingPatchPath = patchPath;
	fHasProposal = false;
	fApplyButton->SetLabel("Apply Patch");
	fApplyButton->SetEnabled(true);
	fRejectButton->SetEnabled(true);
	SetPreviewText(diff.c_str());

	BString log("Saved patch for preview: ");
	log << patchPath.c_str();
	if (!validation.warnings.empty()) {
		log << "\nWarnings:";
		for (const std::string& warning : validation.warnings) {
			log << "\n";
			log << warning.c_str();
		}
	}
	AppendLog(log.String());
}

void
MainWindow::UpdateAuthLabel()
{
#ifdef HAIKODE_CODEX_BRIDGE
	if (fCodexRunning) {
		fAuthLabel->SetText("Codex running");
		return;
	}

	CodexStatus status = fCodexBridge.Status();
	switch (status.state) {
		case CodexState::LoggedIn:
			fAuthLabel->SetText("Codex logged in");
			break;
		case CodexState::LoggedOut:
			fAuthLabel->SetText("Codex logged out");
			break;
		case CodexState::NotFound:
			fAuthLabel->SetText("Codex not found");
			break;
		default:
			fAuthLabel->SetText("Codex status error");
			break;
	}
#else
#ifndef HAIKODE_AI_NETWORK
	fAuthLabel->SetText("AI disabled");
#else
	const char* haikodeKey = getenv("HAIKODE_API_KEY");
	const char* openaiKey = getenv("OPENAI_API_KEY");
	if ((haikodeKey != nullptr && haikodeKey[0] != '\0')
		|| (openaiKey != nullptr && openaiKey[0] != '\0'))
		fAuthLabel->SetText("AI ready");
	else
		fAuthLabel->SetText("API key missing");
#endif
#endif
}

void
MainWindow::UpdateRadarLabels()
{
	BString label;
	label << "Needs review: " << fLastScan.radar.needsReview;
	fNeedsReviewButton->SetLabel(label.String());

	label = "";
	label << "TODO files: " << fLastScan.radar.todoFiles;
	fTodoButton->SetLabel(label.String());

	label = "";
	label << "High-risk files: " << fLastScan.radar.highRiskFiles;
	fHighRiskButton->SetLabel(label.String());

	label = "";
	label << "Recently changed: " << fLastScan.radar.recentlyChanged;
	fRecentlyChangedButton->SetLabel(label.String());

	label = "";
	label << "Missing docs: " << fLastScan.radar.missingDocs;
	fMissingDocsButton->SetLabel(label.String());

	label = "";
	label << "AI notes stale: " << fLastScan.radar.aiNotesStale;
	fStaleNotesButton->SetLabel(label.String());
}

void
MainWindow::SetPreviewText(const char* text)
{
	if (fPreviewView == nullptr)
		return;
	fPreviewView->SetText(text == nullptr ? "" : text);
	fPreviewView->ScrollToOffset(0);
}

void
MainWindow::AppendLog(const char* text)
{
	if (text == nullptr || fOutputView == nullptr)
		return;

	BString line(text);
	line << "\n";

	const int32 offset = fOutputView->TextLength();
	fOutputView->Insert(offset, line.String(), line.Length());
	fOutputView->ScrollToOffset(fOutputView->TextLength());
}
