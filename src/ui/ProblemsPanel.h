/*
 * Copyright 2023, Andrea Anzani 
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include <ColumnListView.h>

#include "PanelTabManager.h"

class BPopUpMenu;
class BMenuItem;
class BTabView;
class IEditor;
class ProblemsPanel : public BColumnListView {
public:
		ProblemsPanel(PanelTabManager*, tab_id id);
		virtual ~ProblemsPanel();

		void UpdateProblems(IEditor* editor);

		virtual void MessageReceived(BMessage* msg);
		virtual void AttachedToWindow();

		void ClearProblems();

private:
		void	_UpdateTabLabel();
		PanelTabManager* fPanelTabManager;
		BPopUpMenu* fPopUpMenu;
		tab_id		fTabId;
};
