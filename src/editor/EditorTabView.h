/*
 * Copyright 2025, Andrea Anzani 
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include <functional>

#include <Messenger.h>
#include <PopUpMenu.h>

#include "EditorId.h"
#include "GTabView.h"

class IEditor;
class GTabEditor;

#define deprecated_

class EditorTabView : public GTabView {
public:

	enum {
		kETVNewTab = 'etnt',
		kETVSelectedTab = 'etst',
		kETVCloseTab = 'etct'
	};

			 EditorTabView(BMessenger target);
			~EditorTabView();

	void	AddEditor(const char* label, IEditor* editor, BMessage* info = nullptr);

	IEditor* SelectedEditor() const;

	void	SetTabColor(IEditor*, const rgb_color& color);
	void	SetTabLabel(IEditor*, const char* label);
	BString	TabLabel(IEditor* editor) const;

	void	SelectTab(const entry_ref* ref, BMessage* selInfo = nullptr);

	void	RemoveEditor(IEditor* editor);

	void	SelectNext();
	void	SelectPrev();

//deprecated :

	deprecated_ int32	SelectedTabIndex() const;

	deprecated_ int32	CountTabs() const;

	deprecated_ void	SelectTab(int32 index, BMessage* selInfo = nullptr);
	deprecated_ IEditor* EditorAt(int32 index);

				IEditor* EditorBy(const node_ref* nodeRef);
				IEditor*	EditorBy(const entry_ref* ref);
				IEditor*	EditorById(editor_id id);

	void	ForEachEditor(const std::function<bool(IEditor*)>& op);
	void 	ReverseForEachEditor(const std::function<bool(IEditor*)>& op);

	void	AttachedToWindow() override;
	void	MessageReceived(BMessage*) override;

protected:
friend GTabEditor;

	GTab*	CreateTabView(GTab* clone) override;

	void	OnTabSelected(GTab* tab) override;

	void	ShowTabMenu(GTabEditor* tab, BPoint where);
	BString	GetToolTipText(GTabEditor* tab);

private:
			GTab*	CreateTabView(const char* label) override;
			BMenuItem* CreateMenuItem(GTab* tab) override;

			IEditor*		_GetEditor_(const entry_ref* ref) const;
			deprecated_ GTabEditor*	_GetTab_(const entry_ref* ref) const;
			GTabEditor* _GetTab(IEditor* editor) const;
			GTabEditor* _GetTab(editor_id id) const;

			BMessenger	fTarget;
			BPopUpMenu* fPopUpMenu;
			BMessage 	fLastSelectedInfo;
};
