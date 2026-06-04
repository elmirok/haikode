/*
 * Copyright 2023-2024, the Genio team
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include <CardLayout.h>
#include <ListView.h>
#include <TextControl.h>
#include <View.h>


class BListItem;
class BOutlineListView;
class BStringView;
class ToolBar;
class FunctionsOutlineView : public BView {
public:
	// TODO: Maybe reuse LSP definition ?
	struct Symbol {
		BString name;
		int32 kind;
	};
			FunctionsOutlineView();

	void	AttachedToWindow() override;
	void	DetachedFromWindow() override;
	void	MessageReceived(BMessage* msg) override;
	void	ClearFilter();

private:
	BListItem*  _RecursiveSymbolByCaretPosition(int32 position, BListItem* parent);
	void        _UpdateDocumentSymbols(const BMessage& msg, const entry_ref* newRef);
	void	    _RecursiveAddSymbols(BListItem* parent, const BMessage* msg);
    status_t    _GoToSymbol(BMessage *msg);
	void        _RenameSymbol(BMessage *msg);
	void		_SelectSymbolByCaretPosition(int32 position);


	void			_ApplyFilter(bool force = false);
	void			_ClearFilter();
	void			_PopulateFilterResults();
	bool			_IsFilterActive() const;

	BOutlineListView* fListView;
	BScrollView* fScrollView;
	ToolBar*	fToolBar;
	Symbol		fSelectedSymbol;
	entry_ref	fCurrentRef;
	BTextControl*	fFilterTextControl;
	BListView*		fFilterListView;
	BScrollView*	fFilterScrollView;
	BCardLayout*	fCardLayout;
	BString			fFilterString;
};
