/*
 * Copyright The Genio Contributors
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#pragma once


#include "GOutlineListView.h"


enum RepositoryViewMessages {
	kUndefinedMessage,
	kInvocationMessage
};

enum ItemType {
	kHeader,
	kLocalBranch,
	kRemoteBranch,
	kTag
};

namespace Genio::Git {
	class GitRepository;
}

class BranchItem;
class ProjectFolder;
class RepositoryView : public GOutlineListView {
public:

					 RepositoryView();
	virtual 		~RepositoryView();

			void	MouseMoved(BPoint point, uint32 transit, const BMessage* message) override;
			void	AttachedToWindow() override;
			void	DetachedFromWindow() override;
			void	MessageReceived(BMessage* message) override;
			void	SelectionChanged() override;

	BranchItem*		InitEmptySuperItem(const BString &label);
	void			BuildBranchTree(const BString &branch, uint32 branchType, const bool& current);

private:
	void			ShowPopupMenu(BPoint where) override;

};