/*
 * Copyright 2023, Andrea Anzani
 * All rights reserved. Distributed under the terms of the MIT license.
 */


#include "ProblemsPanel.h"

#include "EditorMessages.h"
#include "GMessage.h"
#include "LSPEditorWrapper.h"

#include <Catalog.h>
#include <ColumnTypes.h>
#include <MenuItem.h>
#include <PopUpMenu.h>
#include <TabView.h>
#include <Window.h>

#include <string>

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "ProblemsPanel"

enum  {
	COLUMNVIEW_CLICK = 'clIV',
	COLUMNVIEW_SELECT = 'clSE'
};

enum {
	kCategoryColumn = 0,
	kMessageColumn,
	kSourceColumn,
	kPositionColumn
};

class RangeRow : public BRow {
public:
	RangeRow()
		:
		editor(nullptr)
	{
	}

	IEditor*		editor;
	lsp::Range 		range;
};

#define ProblemLabel B_TRANSLATE("Problems")

ProblemsPanel::ProblemsPanel(PanelTabManager* panelTabManager, tab_id id)
	:
	BColumnListView(ProblemLabel, B_NAVIGABLE, B_FANCY_BORDER, true),
	fPanelTabManager(panelTabManager),
	fPopUpMenu(nullptr),
	fTabId(id)
{
	AddColumn(new BStringColumn( B_TRANSLATE("Category"),
								200.0, 20.0, 800.0, 0), kCategoryColumn);
	AddColumn(new BStringColumn( B_TRANSLATE("Message"),
								600.0, 20.0, 2000.0, 0), kMessageColumn);
	AddColumn(new BStringColumn( B_TRANSLATE("Source"),
								100.0, 20.0, 200.0, 0), kSourceColumn);
	AddColumn(new BStringColumn( B_TRANSLATE("Line"),
								100.0, 20.0, 200.0, 0), kPositionColumn);
}


ProblemsPanel::~ProblemsPanel()
{
}


void
ProblemsPanel::AttachedToWindow()
{
	BColumnListView::AttachedToWindow();
	SetInvocationMessage(new BMessage(COLUMNVIEW_CLICK));
	SetSelectionMessage(new BMessage(COLUMNVIEW_SELECT));
	SetTarget(this);
}


void
ProblemsPanel::MessageReceived(BMessage* msg)
{
	switch (msg->what) {
		case COLUMNVIEW_CLICK: {
			RangeRow* range = dynamic_cast<RangeRow*>(CurrentSelection());
			if (range) {
				GMessage refs = {
					{"what", B_REFS_RECEIVED},
					{"start:line", (int32)range->range.start.line + 1},
					{"start:character", (int32)range->range.start.character }
				};
				refs.AddRef("refs",  range->editor->FileRef());
				Window()->PostMessage(&refs);
			}
			break;
		}
		case COLUMNVIEW_SELECT: {
			BPoint where;
			uint32 buttons = 0;
			GetMouse(&where, &buttons);
			where.x += 2; // to prevent occasional select
			if (buttons & B_SECONDARY_MOUSE_BUTTON) {
				RangeRow* row = dynamic_cast<RangeRow*>(CurrentSelection());
				if (!row)
					return;

				LSPEditorWrapper* lsp = row->editor->GetLSPEditorWrapper();
				if (lsp) {
					LSPDiagnostic dia;
					int32 index = lsp->DiagnosticFromRange(row->range, dia);
					fPopUpMenu = new BPopUpMenu("_popup");
					fPopUpMenu->SetRadioMode(false);
					if (index > -1 && dia.codeActions.has_value()
						&& dia.codeActions->isArray() && !dia.codeActions->array().empty()) {
						auto& actionsArray = dia.codeActions->array();
						for (int i = 0; i < static_cast<int>(actionsArray.size()); i++) {
							std::string title = actionsArray[i].object().get("title").string();
							auto item = new BMenuItem(title.c_str(),
								new GMessage({{"what", kApplyFix}, {"index", index}, {"action", i},
								{"quickFix", true}}));
							fPopUpMenu->AddItem(item);
						}
					} else {
						auto item = new BMenuItem(B_TRANSLATE("No fix available"), nullptr);
						item->SetEnabled(false);
						fPopUpMenu->AddItem(item);
					}
					fPopUpMenu->SetTargetForItems(row->editor->View());
					fPopUpMenu->Go(ConvertToScreen(where), true);
					delete fPopUpMenu;
					fPopUpMenu = nullptr;
				}
			}
			break;
		}
		default:
			BColumnListView::MessageReceived(msg);
			break;
	};
}


void
ProblemsPanel::UpdateProblems(IEditor* editor)
{
	Clear();

	LSPEditorWrapper* lsp = editor->GetLSPEditorWrapper();
	if (lsp) {
		std::vector<LSPDiagnostic> diagnostics;
		lsp->GetDiagnostics(diagnostics);
		for (auto& dia: diagnostics) {

			RangeRow* row = new RangeRow();
			row->range = dia.diagnostic.range;
			row->editor = editor;
			row->SetField(new BStringField(dia.category.value_or("").c_str()), kCategoryColumn);
			row->SetField(new BStringField(dia.diagnostic.message.c_str()), kMessageColumn);
			row->SetField(new BStringField(dia.diagnostic.source.value_or("").c_str()), kSourceColumn);
			BString line;
			line.SetToFormat("%d", dia.diagnostic.range.start.line + 1);
			row->SetField(new BStringField(line), kPositionColumn);
			AddRow(row);
		}
	}
	_UpdateTabLabel();
}


void
ProblemsPanel::ClearProblems()
{
	Clear();
	_UpdateTabLabel();
}


void
ProblemsPanel::_UpdateTabLabel()
{
	if (!fPanelTabManager)
		return;

	BString label = ProblemLabel;
	if (CountRows() > 0) {
		label.Append(" (");
		label.Append(std::to_string(CountRows()).c_str());
		label.Append(")");
	}

	fPanelTabManager->SetLabelForTab(fTabId, label.String());
}
