/*
 * Copyright The Genio Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */


#include "RepositoryView.h"

#include <Catalog.h>
#include <Debug.h>
#include <Looper.h>

#include <filesystem>

#include "BranchItem.h"
#include "ConfigManager.h"
#include "GenioApp.h"
#include "GitRepository.h"
#include "GMessage.h"
#include "ProjectFolder.h"
#include "SourceControlPanel.h"
#include "StringFormatter.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "SourceControlPanel"


RepositoryView::RepositoryView()
	:
	GOutlineListView("RepositoryView", B_SINGLE_SELECTION_LIST)
{
}


RepositoryView::~RepositoryView()
{
}


void
RepositoryView::MouseMoved(BPoint point, uint32 transit, const BMessage* message)
{
	GOutlineListView::MouseMoved(point, transit, message);
}


void
RepositoryView::AttachedToWindow()
{
	GOutlineListView::AttachedToWindow();
	SetTarget(this);
	if (Target()->LockLooper()) {
		Target()->StartWatching(this, MsgChangeProject);
		Target()->StartWatching(this, MsgSwitchBranch);
		Target()->UnlockLooper();
	}

	SetInvocationMessage(new BMessage(kInvocationMessage));
}


/* virtual */
void
RepositoryView::DetachedFromWindow()
{
	GOutlineListView::DetachedFromWindow();
	if (Target()->LockLooper()) {
		Target()->StopWatching(this, MsgChangeProject);
		Target()->StopWatching(this, MsgSwitchBranch);
		Target()->UnlockLooper();
	}
}


/* virtual */
void
RepositoryView::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case kInvocationMessage: {
			auto item = dynamic_cast<BranchItem*>(ItemAt(CurrentSelection()));
			if (item == nullptr)
				break;
			if (item->IsCurrentBranch())
				break;

			GMessage switchMessage = {
				{"what", MsgSwitchBranch},
				{"value", item->BranchName()},
				{"type", (item->BranchType() == kLocalBranch) ?
					GitRepository::BRANCH_LOCAL : GitRepository::BRANCH_REMOTE },
				{"sender", kSenderRepositoryPopupMenu}
			};
			BMessenger messenger(Target());
			messenger.SendMessage(&switchMessage);
			break;
		}
		default:
			GOutlineListView::MessageReceived(message);
			break;
	}
}


/* virtual */
void
RepositoryView::SelectionChanged()
{
}


void
RepositoryView::BuildBranchTree(const BString &branch, uint32 branchType, const bool &current)
{
	// Do not show an outline
	if (!gCFG["repository_outline"]) {
		BranchItem* item = new BranchItem(branch.String(), branch.String(), branchType, 1);
		if (current) {
			item->SetCurrentBranch(true);
		}
		AddItem(item);
		return;
	}

	// show the outline
	std::filesystem::path path = branch.String();
	std::vector<std::string> parts(path.begin(), path.end());
	uint32 lastIndex = parts.size() - 1;
	uint32 i = 0;

	BranchItem* lastItem = static_cast<BranchItem*>(FullListLastItem());
	if (lastItem->OutlineLevel() > 0) {
		path = lastItem->BranchName();
		std::vector<std::string> lastItemParts(path.begin(), path.end());
		uint32 lastCompareIndex = std::min<uint32>(lastIndex, parts.size() - 1);
		while (i < lastCompareIndex) {
			if (parts.at(i) != lastItemParts.at(i))
				break;
			i++;
		}
	}

	while (i < lastIndex) {
		BString partName = parts.at(i).c_str();
		auto newItem = new BranchItem(branch.String(), partName, kHeader, i + 1);
		AddItem(newItem);
		i++;
	}

	BString partName = parts.at(i).c_str();
	auto newItem = new BranchItem(branch.String(), partName, branchType, i + 1);
	if (current) {
		newItem->SetCurrentBranch(true);
	}
	AddItem(newItem);
}


/* virtual */
void
RepositoryView::ShowPopupMenu(BPoint where)
{
	auto index = IndexOf(where);
	if (index < 0)
		return;

	const auto item = dynamic_cast<BranchItem*>(ItemAt(index));
	if (item == nullptr)
		return;

	auto itemType = item->BranchType();
	BString selectedBranch(item->BranchName());
	selectedBranch.RemoveLast("*");

	StringFormatter fmt;
	fmt.Substitutions["%selected_branch%"] = selectedBranch;

	auto optionsMenu = new BPopUpMenu("Options", false, false);

	switch (itemType) {
		case kLocalBranch:
		{
			if (!item->IsCurrentBranch()) {
				optionsMenu->AddItem(
					new BMenuItem(
						fmt << B_TRANSLATE("Switch to \"%selected_branch%\""),
						new GMessage{
							{"what", MsgSwitchBranch},
							{"value", selectedBranch},
							{"type", GitRepository::BRANCH_LOCAL},
							{"sender", kSenderRepositoryPopupMenu}}));
			}

			optionsMenu->AddItem(
				new BMenuItem(
					fmt << B_TRANSLATE("Rename \"%selected_branch%\""),
					new GMessage{
						{"what", MsgRenameBranch},
						{"value", selectedBranch},
						{"type", GitRepository::BRANCH_LOCAL}}));

			optionsMenu->AddItem(
				new BMenuItem(
					fmt << B_TRANSLATE("Delete \"%selected_branch%\""),
					new GMessage{
						{"what", MsgDeleteBranch},
						{"value", selectedBranch},
						{"type", GitRepository::BRANCH_LOCAL}}));

			optionsMenu->AddSeparatorItem();

			optionsMenu->AddItem(
				new BMenuItem(
					fmt << B_TRANSLATE("Create new branch from \"%selected_branch%\""),
					new GMessage{
						{"what", MsgNewBranch},
						{"value", selectedBranch},
						{"type", GitRepository::BRANCH_LOCAL}}));

			// optionsMenu->AddItem(
				// new BMenuItem(
					// fmt << B_TRANSLATE("Create new tag from \"%selected_branch%\""),
					// new GMessage{
						// {"what", MsgNewTag},
						// {"value", selected_branch}}));

			break;
		}
		case kRemoteBranch:
		{
			/*fmt.Substitutions["%current_branch%"] = fCurrentBranch;
			LogInfo("fmt.Substitutions[%current_branch%] = %s", fCurrentBranch.String());
*/
			optionsMenu->AddItem(
				new BMenuItem(
					fmt << B_TRANSLATE("Switch to \"%selected_branch%\""),
					new GMessage{
						{"what", MsgSwitchBranch},
						{"value", selectedBranch},
						{"type", GitRepository::BRANCH_REMOTE},
						{"sender", kSenderRepositoryPopupMenu}}));

			// Deleting a remote branch is disabled for now
			// the code in GitRepository deletes only the local ref to the remote branch and
			// git fetch --all brings the remote branch back again
			// TODO: A different approach is required to delete a remote branch using push
			// optionsMenu->AddItem(
				// new BMenuItem(
					// fmt << B_TRANSLATE("Delete \"%selected_branch%\""),
					// new GMessage{
						// {"what", MsgDeleteBranch},
						// {"value", selected_branch},
						// {"type", GitRepository::BRANCH_REMOTE}}));


			optionsMenu->AddSeparatorItem();

			// We don't allow to merge a local branch into its origin
			// if (!selected_branch.EndsWith(fCurrentBranch)) {
				// optionsMenu->AddItem(
					// new BMenuItem(
						// fmt << B_TRANSLATE("Merge \"%selected_branch%\" into \"%current_branch%\""),
						// new GMessage{
							// {"what", MsgMerge},
							// {"selected_branch", selected_branch},
							// {"current_branch", fCurrentBranch}}));
				// optionsMenu->AddSeparatorItem();
			// }

			optionsMenu->AddItem(
				new BMenuItem(
					fmt << B_TRANSLATE("Create new branch from \"%selected_branch%\""),
					new GMessage{
						{"what", MsgNewBranch},
						{"value", selectedBranch},
						{"type", GitRepository::BRANCH_REMOTE}}));

			// optionsMenu->AddItem(
				// new BMenuItem(
					// fmt << B_TRANSLATE("Create new tag from \"%selected_branch%\""),
					// new GMessage{
						// {"what", MsgNewTag},
						// {"value", selected_branch}}));

			break;
		}
		case kTag:
		{
			// TODO
			break;
		}
		default:
		{
			break;
		}
	}

	optionsMenu->AddItem(
		new BMenuItem(
			fmt << B_TRANSLATE("Copy name"),
			new GMessage{
				{"what", MsgCopyRefName},
				{"value", selectedBranch}}));

	optionsMenu->SetTargetForItems(Target());
	optionsMenu->SetAsyncAutoDestruct(true);
	optionsMenu->Go(ConvertToScreen(where), true, false, true);
}


BranchItem*
RepositoryView::InitEmptySuperItem(const BString &label)
{
	auto item = new BranchItem(label, label, kHeader);
	item->SetTextFontFace(B_BOLD_FACE);
	AddItem(item);
	return item;
}
