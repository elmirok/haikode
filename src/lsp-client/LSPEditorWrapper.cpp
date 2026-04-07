/*
 * Copyright 2023, Andrea Anzani
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "LSPEditorWrapper.h"

#include <Application.h>
#include <Path.h>
#include <Catalog.h>
#include <Window.h>

#include <algorithm>
#include <cstdio>
#include <debugger.h>
#include <unistd.h>

#include "Editor.h"
#include "EditorStatusView.h"
#include "Log.h"
#include "LSPJsonBridge.h"
#include "LSPProjectWrapper.h"
#include "JumpNavigator.h"
#include "TextUtils.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Editor"


#define IF_ID(METHOD_NAME, METHOD) if (id.compare(METHOD_NAME) == 0) { METHOD(result); return; }


LSPEditorWrapper::LSPEditorWrapper(BPath filenamePath, Editor* editor)
	:
	LSPTextDocument(filenamePath, editor->FileType().c_str()),
	fEditor(editor),
	fToolTip(nullptr),
	fLSPProjectWrapper(nullptr),
	fCallTip(editor),
	fInitialized(false),
	fLastWordStartPosition(-1),
	fLastWordEndPosition(-1)
{
	assert(fEditor);
}


void
LSPEditorWrapper::ApplySettings()
{
	fEditor->SendMessage(SCI_INDICSETFORE,  IND_DIAG, 0x0000ff);
	fEditor->SendMessage(SCI_INDICSETSTYLE, IND_DIAG, INDIC_SQUIGGLE);

#ifdef DOCUMENT_LINK
	fEditor->SendMessage(SCI_INDICSETFORE,  IND_LINK, 0xff0000);
	fEditor->SendMessage(SCI_INDICSETSTYLE, IND_LINK, INDIC_PLAIN);
#endif

	fEditor->SendMessage(SCI_INDICSETFORE,  IND_OVER, 0xff0000);
	fEditor->SendMessage(SCI_INDICSETSTYLE, IND_OVER, INDIC_PLAIN);

	fEditor->SendMessage(SCI_SETMOUSEDWELLTIME, 1000);

	// int margins = fEditor->SendMessage(SCI_GETMARGINS);
	// fEditor->SendMessage(SCI_SETMARGINS, margins + 1);
	// fEditor->SendMessage(SCI_SETMARGINTYPEN, margins, SC_MARGIN_SYMBOL);
	// fEditor->SendMessage(SCI_SETMARGINWIDTHN,margins, 16);
	// fEditor->SendMessage(SCI_SETMARGINMASKN, margins, 1 << 2);
	// fEditor->SendMessage(SCI_MARKERSETBACK, margins, kMarkerForeColor);
	// fEditor->SendMessage(SCI_MARKERADD, 1, 2);
}


void
LSPEditorWrapper::UnsetLSPServer()
{
	if (fLSPProjectWrapper == nullptr)
		return;

	didClose();
	fFileStatus = "";
	fLSPProjectWrapper->UnregisterTextDocument(this);
	fLSPProjectWrapper = nullptr;
}


bool
LSPEditorWrapper::HasLSPServer()
{
	return fLSPProjectWrapper != nullptr;
}


bool
LSPEditorWrapper::HasLSPServerCapability(const LSPCapability cap)
{
	return HasLSPServer() && fLSPProjectWrapper->HasCapability(cap);
}


void
LSPEditorWrapper::ApplyFix(BMessage* info)
{
	if (!HasLSPServer())
		return;

	if (!info->GetBool("quickFix", false))
		return;

	int32 diaIndex = info->GetInt32("index", -1);
	int32 actIndex = info->GetInt32("action", -1);
	if (diaIndex >= 0 && fLastDiagnostics.size() > (size_t)diaIndex) {
		auto& changes =
			fLastDiagnostics.at(diaIndex).codeActions.value()[actIndex].edit.value().changes.value();
		for (auto& [uri, edits] : changes) {
			if (GetFilenameURI().ICompare(uri.toString().c_str()) == 0) {
				_DoFormat(std::move(edits));
			}
		}
	}
}


void
LSPEditorWrapper::ApplyEdit(std::string info)
{
	if (!HasLSPServer())
		return;

	auto edits = LSPBridge::fromJson<ArrayTextEdit>(lsp::json::parse(info));
	_DoFormat(std::move(edits));
}


void
LSPEditorWrapper::SetLSPServer(LSPProjectWrapper* cW) {

	assert(cW);
	assert(fEditor);

	if (fLSPProjectWrapper != nullptr)
		UnsetLSPServer();

	SetFileType(fEditor->FileType().c_str());

	fLSPProjectWrapper = cW;
	if (!cW->RegisterTextDocument(this)) {
		fLSPProjectWrapper = nullptr;
	}
}


bool
LSPEditorWrapper::IsInitialized()
{
	return fInitialized && fLSPProjectWrapper != nullptr;
}


void
LSPEditorWrapper::didOpen()
{
	if (!IsInitialized())
		return;

	const char* text = reinterpret_cast<const char*>(fEditor->SendMessage(SCI_GETCHARACTERPOINTER));

	fLSPProjectWrapper->DidOpen(this, text, FileType().String());

}


void
LSPEditorWrapper::didClose()
{
	if (!IsInitialized())
		return;

	flushChanges();

	if (fEditor) {
		_RemoveAllDiagnostics();
		_RemoveAllDocumentLinks();
	}

	fLSPProjectWrapper->DidClose(this);
}


void
LSPEditorWrapper::didSave()
{
	if (!IsInitialized())
		return;

	flushChanges();
	fLSPProjectWrapper->DidSave(this);
}


void
LSPEditorWrapper::didChange(
	const char* text, long len, Sci_Position start_pos, Sci_Position poslength)
{
	if (!IsInitialized() || fEditor == nullptr)
		return;

	Sci_Position end_pos = fEditor->SendMessage(SCI_POSITIONRELATIVE, start_pos, poslength);

	lsp::TextDocumentContentChangeEvent_Range_Text event;
	Range range;
	FromSciPositionToLSPPosition(start_pos, &range.start);
	FromSciPositionToLSPPosition(end_pos, &range.end);

	event.range = range;
	event.text.assign(text, len);

	fChanges.push_back(lsp::TextDocumentContentChangeEvent{std::move(event)});
}


void
LSPEditorWrapper::flushChanges()
{
	if (fChanges.size() > 0) {
		fLSPProjectWrapper->DidChange(this, fChanges, false);
		fChanges.clear();
	}
}

/*
void
LSPEditorWrapper::_DoFormat(value& params)
{
	fEditor->SendMessage(SCI_BEGINUNDOACTION, 0, 0);
	auto& items = params.array();
	for (auto it = items.rbegin(); it != items.rend(); ++it) {
		ApplyTextEdit((*it));
	}
	fEditor->SendMessage(SCI_ENDUNDOACTION, 0, 0);

	// Invalidate the current diagnostics and links to avoid any race-condition
	// between the list of fixes and the updated document.
	// a new list will arrive from LSP in few instants!
	_RemoveAllDiagnostics();
	_RemoveAllDocumentLinks();
}
*/

void
LSPEditorWrapper::_DoFormat(ArrayTextEdit&& edits)
{
	fEditor->SendMessage(SCI_BEGINUNDOACTION, 0, 0);
	for (auto it = edits.rbegin(); it != edits.rend(); ++it) {
		ApplyTextEdit(*it);
	}
	fEditor->SendMessage(SCI_ENDUNDOACTION, 0, 0);

	_RemoveAllDiagnostics();
	_RemoveAllDocumentLinks();
}


void
LSPEditorWrapper::_DoRename(lsp::WorkspaceEdit&& edit)
{
	if (edit.changes.has_value()) {
		for (auto& [uri, edits] : edit.changes.value()) {
			OpenFileURI(uri.toString(), -1, -1, std::move(edits));
		}
	} else if (edit.documentChanges.has_value()) {
		for (auto& change : edit.documentChanges.value()) {
			auto* textDocEdit = std::get_if<lsp::TextDocumentEdit>(&change);
			if (textDocEdit) {
				std::string uri = textDocEdit->textDocument.uri.toString();
				OpenFileURI(uri, -1, -1, (ArrayTextEdit&&)(std::move(textDocEdit->edits)));
			}
		}
	}
}


void
LSPEditorWrapper::Format()
{
	if (!IsInitialized() || fEditor == nullptr)
		return;

	flushChanges();

	// format a range or format the whole doc?
	Sci_Position s_start = fEditor->SendMessage(SCI_GETSELECTIONSTART, 0, 0);
	Sci_Position s_end = fEditor->SendMessage(SCI_GETSELECTIONEND, 0, 0);

	if (s_start < s_end) {
		Range range;
		FromSciPositionToRange(s_start, s_end, &range);
		fLSPProjectWrapper->RangeFomatting(this, range);
	} else {
		fLSPProjectWrapper->Formatting(this);
	}
}


void
LSPEditorWrapper::GoTo(LSPEditorWrapper::GoToType type)
{
	if (!IsInitialized()|| !fEditor || !IsStatusValid())
		return;

	flushChanges();

	Position position;
	GetCurrentLSPPosition(&position);

	switch (type) {
		case GOTO_DEFINITION:
			fLSPProjectWrapper->GoToDefinition(this, position);
			break;
		case GOTO_DECLARATION:
			fLSPProjectWrapper->GoToDeclaration(this, position);
			break;
		case GOTO_IMPLEMENTATION:
			fLSPProjectWrapper->GoToImplementation(this, position);
			break;
	}
}


void
LSPEditorWrapper::Rename(std::string newName)
{
	if (!IsInitialized()|| !fEditor || !IsStatusValid())
		return;

	flushChanges();

	Position position;
	GetCurrentLSPPosition(&position);
	fLSPProjectWrapper->Rename(this, position, newName);
}


void
LSPEditorWrapper::StartHover(Sci_Position sci_position)
{
	if (!IsInitialized() || sci_position < 0 || !IsStatusValid()) {
		return;
	}

	LSPDiagnostic dia;
	if (DiagnosticFromPosition(sci_position, dia) > -1) {
		_ShowToolTip(dia.range.info.c_str());
		return;
	}

	Position position;
	FromSciPositionToLSPPosition(sci_position, &position);
	fLSPProjectWrapper->Hover(this, position);
}


void
LSPEditorWrapper::MouseMoved(BMessage* message)
{
	if (!IsInitialized()) {
		return;
	}

	if (fLastWordEndPosition > -1 && fLastWordStartPosition > -1) {
		fEditor->SendMessage(SCI_SETINDICATORCURRENT, IND_OVER);
		fEditor->SendMessage(SCI_INDICATORCLEARRANGE, fLastWordStartPosition, fLastWordEndPosition - fLastWordStartPosition);
		fLastWordEndPosition = fLastWordStartPosition = -1;
	}

	if (message->GetInt32("buttons", 0) != 0 || !(modifiers() & B_COMMAND_KEY)) {
		return;
	}

	if (message->GetInt32("be:transit", 0) == 1) {
		BPoint point = message->GetPoint("be:view_where", BPoint(0,0));
		Sci_Position sci_position = fEditor->SendMessage(SCI_POSITIONFROMPOINTCLOSE, point.x, point.y);
		fLastWordStartPosition = fEditor->SendMessage(SCI_WORDSTARTPOSITION, sci_position, true);
		fLastWordEndPosition   = fEditor->SendMessage(SCI_WORDENDPOSITION,   sci_position, true);
		fEditor->SendMessage(SCI_SETINDICATORCURRENT, IND_OVER);
		fEditor->SendMessage(SCI_INDICATORFILLRANGE, fLastWordStartPosition, fLastWordEndPosition - fLastWordStartPosition);
	}
}




int32
LSPEditorWrapper::DiagnosticFromPosition(Sci_Position sci_position, LSPDiagnostic& dia)
{
	int32 index = -1;
	if (fEditor->SendMessage(SCI_INDICATORVALUEAT, IND_DIAG, sci_position) == 1) {
		for (auto& ir : fLastDiagnostics) {
			index++;
			if (sci_position >= ir.range.from && sci_position < ir.range.to) {
				dia = ir;
				return index;
			}
		}
	}
	return -1;
}


int32
LSPEditorWrapper::DiagnosticFromRange(Range& range, LSPDiagnostic& dia)
{
	int32 index = -1;
	for (auto& ir : fLastDiagnostics) {
		index++;
		if (ir.diagnostic.range == range) {
			dia = ir;
			return index;
		}
	}
	return index;
}


void
LSPEditorWrapper::EndHover()
{
	if (!IsInitialized())
		return;

	BAutolock l(fEditor->Looper());
	if (l.IsLocked()) {
		fEditor->HideToolTip();
	}
}


static lsp::TextEdit*
_GetTextEdit(lsp::CompletionItem& item)
{
	if (!item.textEdit)
		return nullptr;
	return std::get_if<lsp::TextEdit>(&*item.textEdit);
}


void
LSPEditorWrapper::SelectedCompletion(const char* text)
{
	if (!IsInitialized() || fEditor == nullptr)
		return;

	flushChanges();

	if (fCurrentCompletion.items.size() > 0) {
		for (auto& item : fCurrentCompletion.items) {
			if (item.label.compare(std::string(text)) == 0) {
				auto* te = _GetTextEdit(item);
				if (!te) break;

				const Sci_Position s_pos = FromLSPPositionToSciPosition(&te->range.start);
				const Sci_Position e_pos = FromLSPPositionToSciPosition(&te->range.end);
				const Sci_Position pos = fEditor->SendMessage(SCI_GETCURRENTPOS);
				Sci_Position cursorPos = e_pos;

				std::string textToAdd = te->newText;

				// algo to remove the ${} stuff
				size_t dollarPos = textToAdd.find_first_of('$');

				if (dollarPos != std::string::npos) {
					size_t lastPos = dollarPos;
					// single value case: check the case is $0
					if (dollarPos < textToAdd.length() - 1 && textToAdd.at(dollarPos + 1) == '0') {
						lastPos += 2;

					} else {
						size_t endMarket = textToAdd.find_last_of('}');
						if (endMarket != std::string::npos)
							lastPos = endMarket + 1;
					}
					textToAdd.erase(dollarPos, lastPos - dollarPos);

					cursorPos = s_pos + dollarPos;
				} else {
					cursorPos = s_pos + textToAdd.length();
				}

				fEditor->SendMessage(
					SCI_SETTARGETRANGE, s_pos, std::max(pos, std::max(e_pos, fCompletionPosition)));
				fEditor->SendMessage(SCI_REPLACETARGET, -1, (sptr_t) "");
				fEditor->SendMessage(SCI_INSERTTEXT, s_pos, (sptr_t) textToAdd.c_str());

				fEditor->SendMessage(SCI_SETCURRENTPOS, cursorPos, 0);
				fEditor->SendMessage(SCI_SETANCHOR, cursorPos, 0);

				fEditor->SendMessage(SCI_SCROLLCARET, 0, 0);

				if (dollarPos != std::string::npos && dollarPos > 0) {
					char posChar = textToAdd.at(dollarPos - 1);
					CharAdded(posChar);
				}

				break;
			}
		}
	}
	fEditor->SendMessage(SCI_AUTOCCANCEL, 0, 0);
	fCurrentCompletion = CompletionList{};
}


void
LSPEditorWrapper::StartCompletion()
{

	if (!IsInitialized() || !fEditor || !IsStatusValid())
		return;

	flushChanges();

	// let's check if a completion is ongoing
	if (fCurrentCompletion.items.size() > 0) {
		// let's close the current Scintilla listbox
		fEditor->SendMessage(SCI_AUTOCCANCEL, 0, 0);
		// let's cancel any previous request running on clangd
		// --> TODO: cancel previous clangd request!

		// let's clean-up current request details:
		this->fCurrentCompletion = CompletionList{};
	}

	Position position;
	GetCurrentLSPPosition(&position);
	lsp::CompletionContext context;
	context.triggerKind = lsp::CompletionTriggerKind::Invoked;

	fCompletionPosition = fEditor->SendMessage(SCI_GETCURRENTPOS, 0, 0);
	fLSPProjectWrapper->Completion(this, position, context);
}


void
LSPEditorWrapper::NextCallTip()
{
	fCallTip.NextCallTip();
}


void
LSPEditorWrapper::PrevCallTip()
{
	fCallTip.PrevCallTip();
}


void
LSPEditorWrapper::HideCallTip()
{
	fCallTip.HideCallTip();
}


void
LSPEditorWrapper::RequestDocumentSymbols()
{
	if (fLSPProjectWrapper == nullptr || fEditor == nullptr)
		return;

	fLSPProjectWrapper->DocumentSymbol(this);
}


void
LSPEditorWrapper::CharAdded(const char ch /*utf-8?*/)
{
	// printf("on char %c\n", ch);
	if (!IsInitialized() || fEditor == nullptr)
		return;

	if (ch != 0) {
		if (fEditor->SendMessage(SCI_AUTOCACTIVE) &&
			!Contains(kWordCharacters, ch)) {
			fEditor->SendMessage(SCI_AUTOCCANCEL);
		}

		if (fLSPProjectWrapper->HasCapability(kLCapCompletion) &&
				Contains(fLSPProjectWrapper->triggerCharacters(), ch)) {

			if (fCallTip.IsVisible())
				fCallTip.HideCallTip();

			flushChanges();
			StartCompletion();
		}
	}

	if (fLSPProjectWrapper->HasCapability(kLCapSignatureHelp)) {
		CallTipAction action = fCallTip.UpdateCallTip(ch, ch == 0);
		if (action == CALLTIP_NEWDATA) {

			flushChanges();
			Position lsp_position;
			FromSciPositionToLSPPosition(fCallTip.Position(), &lsp_position);
			fLSPProjectWrapper->SignatureHelp(this, lsp_position);

		} else if (action == CALLTIP_UPDATE) {
			fCallTip.ShowCallTip();
		}
	}
}


void
LSPEditorWrapper::IndicatorClick(Sci_Position sci_position)
{
	Sci_Position s_start = fEditor->SendMessage(SCI_GETSELECTIONSTART, 0, 0);
	Sci_Position s_end = fEditor->SendMessage(SCI_GETSELECTIONEND, 0, 0);
	if (s_start != s_end)
		return;

	if (fEditor->SendMessage(SCI_INDICATORVALUEAT, IND_OVER, sci_position) == 1) {
		Position position;
		FromSciPositionToLSPPosition(sci_position, &position);
		fLSPProjectWrapper->GoToDefinition(this, position);
	}
#ifdef IND_LINK
	if (fEditor->SendMessage(SCI_INDICATORVALUEAT, IND_LINK, sci_position) == 1) {
		for (auto& ir : fLastDocumentLinks) {
			if (sci_position > ir.from && sci_position <= ir.to) {
				LogTrace("Opening file: [%s]", ir.info.c_str());
				OpenFileURI(ir.info);
				break;
			}
		}
	}
#endif
}


void
LSPEditorWrapper::_ShowToolTip(const char* text)
{
	if (fToolTip == nullptr)
		fToolTip = new BTextToolTip(text);

	fToolTip->SetText(text);
	if (fEditor->Looper()->Lock()) {
		fEditor->ShowToolTip(fToolTip);
		fEditor->Looper()->Unlock();
	}
}


static std::string
_MarkedStringToText(const lsp::MarkedString& ms)
{
	if (auto* s = std::get_if<lsp::String>(&ms))
		return *s;
	return std::get<lsp::MarkedString_Language_Value>(ms).value;
}


static std::string
_ExtractHoverText(const lsp::Hover& hover)
{
	if (auto* mc = std::get_if<lsp::MarkupContent>(&hover.contents))
		return mc->value;

	if (auto* ms = std::get_if<lsp::MarkedString>(&hover.contents))
		return _MarkedStringToText(*ms);

	auto& arr = std::get<lsp::Array<lsp::MarkedString>>(hover.contents);
	std::string combined;
	for (auto& ms : arr) {
		if (!combined.empty())
			combined += "\n\n";
		combined += _MarkedStringToText(ms);
	}
	return combined;
}


void
LSPEditorWrapper::_DoHover(lsp::TextDocument_HoverResult&& result)
{
	if (fEditor == nullptr || !fEditor->Window()->IsActive())
		return;

	if (result.isNull()) {
		EndHover();
		return;
	}

	std::string tip = _ExtractHoverText(result.value());

	if (tip.empty()) {
		EndHover();
		return;
	}
	_ShowToolTip(tip.c_str());
}


void
LSPEditorWrapper::_DoGoTo(lsp::TextDocument_DefinitionResult&& result)
{
	// NullOrOneOf<Definition, Array<DefinitionLink>>;
	if (result.isNull())
		return;

	// OneOf<Definition, Array<DefinitionLink>>
	auto* definition = std::get_if<lsp::Definition>(&result.value());
	if (definition) {
		//using Definition = OneOf<Location, Array<Location>>;
		lsp::Location* location = std::get_if<lsp::Location>(definition);
		if (location) {
		} else {
			auto* array = std::get_if<lsp::Array<lsp::Location>>(definition);
			location = &(*array)[0];
		}

		if (location) {
			std::string uri = location->uri.toString();
			lsp::Position pos = location->range.start;

			OpenFileURI(uri, pos.line + 1, pos.character);
		}
	} else {
		//Array<DefinitionLink>
		auto* array = std::get_if<lsp::Array<lsp::DefinitionLink>>(&result.value());
		if (array) {
			lsp::LocationLink& location = (*array)[0];

			std::string uri = location.targetUri.toString();
			lsp::Position pos = location.targetRange.start;

			OpenFileURI(uri, pos.line + 1, pos.character);
		}
	}
}

void
LSPEditorWrapper::_DoGoTo(value& items)
{
	if (items.isNull())
		return;

	Location location;

	// TODO if more than one match??
	// clangd sends an array of Locations while OmniSharp seems to conform to the standard
	// and sends just one.
	if (items.isArray()) {
		if (items.array().empty())
			return;
		lsp::fromJson(lsp::json::Value(items.array()[0]), location);
	} else {
		lsp::fromJson(lsp::json::Value(items), location);
	}

	std::string uri = location.uri.toString();
	Position pos = location.range.start;

	OpenFileURI(uri, pos.line + 1, pos.character);
}


void
LSPEditorWrapper::_DoSignatureHelp(lsp::SignatureHelp&& signatureHelp)
{
	fCallTip.UpdateSignatures(signatureHelp.signatures);
	fCallTip.ShowCallTip();
}


void
LSPEditorWrapper::_DoCompletion(lsp::TextDocument_CompletionResult&& result)
{
	if (result.isNull())
		return;


	lsp::Array<lsp::CompletionItem>* items = nullptr;

	auto* allItems = std::get_if<lsp::CompletionList>(&result.value());
	if (allItems)
		items = &allItems->items;
	else
		items = std::get_if<lsp::Array<lsp::CompletionItem>>(&result.value());

	if (items == nullptr)
		return;

	std::string line;
	Position position;
	bool positionResolved = false;

	std::string list;
	for (auto& item : *items) {
		std::string label = item.label;
		LeftTrim(label);
		if (list.length() > 0)
			list += "\n";
		list += label;
		item.label = label;
		// if the server is not providing us the textEdit (like pylsp)
		// let's try to create it.
		auto* te = _GetTextEdit(item);
		if (!te || te->newText.empty()) {
			std::string insertText = item.insertText.value_or("");

			lsp::TextEdit newTe;
			newTe.newText = insertText;

			Position pos;
			FromSciPositionToLSPPosition(fCompletionPosition, &pos);
			newTe.range.end = pos;

			// fancy algo to find insertText before current position.
			if (!positionResolved) {
				line = GetCurrentLine();
				GetCurrentLSPPosition(&position);
				positionResolved = true;
			}

			Sci_Position current = static_cast<Sci_Position>(position.character) - 1;
			int32 points = 0;
			for (size_t i = 0 ; i < insertText.length() ; i++) {
				if (current - i >= 0) {
					if (strncasecmp(insertText.c_str(), line.c_str() + current-i, i+1) == 0){
						points = i+1;
					}
				}
			}
			FromSciPositionToLSPPosition(fCompletionPosition - points, &pos);
			newTe.range.start = pos;

			item.textEdit = std::move(newTe);
		}
	}

	if (list.length() > 0) {
		fCurrentCompletion = *allItems; //uhm not sure here
		fEditor->SendMessage(SCI_AUTOCSETSEPARATOR, (int) '\n', 0);
		fEditor->SendMessage(SCI_AUTOCSETIGNORECASE, true);
		fEditor->SendMessage(SCI_AUTOCGETCANCELATSTART, false);
		fEditor->SendMessage(SCI_AUTOCSETORDER, SC_ORDER_CUSTOM, 0);

		// whats' the text already selected so far?
		auto* firstTe = _GetTextEdit(fCurrentCompletion.items[0]);
		if (firstTe) {
			const Sci_Position s_pos = FromLSPPositionToSciPosition(&firstTe->range.start);
			Sci_Position len = fCompletionPosition - s_pos;
			if (len < 0)
				len = 0;
			fEditor->SendMessage(SCI_AUTOCSHOW, len, (sptr_t) list.c_str());
		}
	}
}



void
LSPEditorWrapper::_RemoveAllDiagnostics()
{
	// remove all the indicators..
	fEditor->SendMessage(SCI_SETINDICATORCURRENT, IND_DIAG);
	fEditor->SendMessage(SCI_INDICATORCLEARRANGE, 0, fEditor->SendMessage(SCI_GETTEXTLENGTH));
	fLastDiagnostics.clear();
}


void
LSPEditorWrapper::_DoDiagnostics(lsp::PublishDiagnosticsParams&& params)
{
	if (params.version.has_value()) {
		int32 serverVersion = static_cast<int32>(params.version.value());
		if (serverVersion < Version()) {
			LogTrace("Discarding stale diagnostics: server=%ld local=%ld", serverVersion, Version());
			return;
		}
	}

	_RemoveAllDiagnostics();

	for (auto& diag : params.diagnostics) {
		LSPDiagnostic lspDiag;
		lspDiag.diagnostic = std::move(diag);

		Range& r = lspDiag.diagnostic.range;
		InfoRange& ir = lspDiag.range;
		ir.from = FromLSPPositionToSciPosition(&r.start);
		ir.to = FromLSPPositionToSciPosition(&r.end);
		ir.info = lspDiag.diagnostic.message;

		LogTrace("Diagnostics [%ld->%ld] [%s][%s]\n", ir.from, ir.to, ir.info.c_str(),lspDiag.diagnostic.message.c_str());
		fEditor->SendMessage(SCI_INDICATORFILLRANGE, ir.from, ir.to - ir.from);

		RequestCodeActions(lspDiag.diagnostic);

		fLastDiagnostics.push_back(lspDiag);
	}


	if (fEditor->LockLooper()) {
		fEditor->SetProblems();
		fEditor->UnlockLooper();
	}

	if (fLSPProjectWrapper) {
#ifdef DOCUMENT_LINK
		fLSPProjectWrapper->DocumentLink(this);
#endif
		fLSPProjectWrapper->DocumentSymbol(this);
	}
}


void
LSPEditorWrapper::RequestCodeActions(Diagnostic& diagnostic)
{
	lsp::CodeActionContext context;
	context.diagnostics.push_back(diagnostic);
	fLSPProjectWrapper->CodeAction(this, diagnostic.range, context);
}


void
LSPEditorWrapper::CodeActionResolve(lsp::CodeAction& params)
{
	fLSPProjectWrapper->CodeActionResolve(this, params);
}

void
LSPEditorWrapper::_DoCodeActions(lsp::TextDocument_CodeActionResult&& codeAction)
{
	for (auto& act : codeAction.value()) {
		CodeAction* action = std::get_if<lsp::CodeAction>(&act);
		if (action && action->diagnostics.has_value()) {
			for (auto& dia: action->diagnostics.value()) {
				Range& range = dia.range;

				for (auto& d: fLastDiagnostics) {

					if (d.diagnostic.range == range) {
						if (!d.codeActions)
							d.codeActions.emplace();
						d.codeActions->push_back(*action);
						if (!action->diagnostics)
							action->diagnostics.emplace();
						action->diagnostics->push_back(d.diagnostic);

						if (action->edit.has_value() == false)
							CodeActionResolve(*action);;
					}
				}
			}
		}
	}
}/*
void
LSPEditorWrapper::_DoCodeActions(value& params)
{
	printf("DOCODEACTION %s\n", lsp::json::stringify(params).c_str());
	for (auto& v : params.array()) {
		auto action = LSPBridge::fromJson<lsp::CodeAction>(v);

		debugger("old code action");

		// Extract range from clangd-specific data field
		auto& dataObj = action.data.value().object();
		auto& rangeObj = dataObj.get("Range").object();
		auto& startObj = rangeObj.get("Start").object();
		auto& endObj = rangeObj.get("End").object();

		Range range;
		range.start.character = static_cast<unsigned int>(startObj.get("Character").integer());
		range.start.line = static_cast<unsigned int>(startObj.get("Line").integer());
		range.end.character = static_cast<unsigned int>(endObj.get("Character").integer());
		range.end.line = static_cast<unsigned int>(endObj.get("Line").integer());

		for (auto& d: fLastDiagnostics) {
			if (d.diagnostic.range == range) {
				if (!d.codeActions)
					d.codeActions.emplace();
				d.codeActions->push_back(action);
				if (!action.diagnostics)
					action.diagnostics.emplace();
				action.diagnostics->push_back(d.diagnostic);

				auto* editField = v.object().find("edit");
				if (!editField || editField->isNull())
					CodeActionResolve(v);

			}
		}
	}
}*/


void
LSPEditorWrapper::_DoCodeActionResolve(CodeAction&& action)
{
	// Extract range from clangd-specific data field
	for (auto& dia: action.diagnostics.value()) {
		Range& range = dia.range;

		for (auto& d: fLastDiagnostics) {
			if (d.diagnostic.range == range) {
				if (d.codeActions.has_value()) {
					for (auto& ca: d.codeActions.value()) {
						auto& caIdentifier = ca.data.value().object().get("Identifier").string();
						auto& actionIdentifier = action.data.value().object().get("Identifier").string();
						if (caIdentifier == actionIdentifier) {
							ca.edit = action.edit;
						}
					}
				}
			}
		}
	}
}


void
LSPEditorWrapper::_RemoveAllDocumentLinks()
{
	// remove all the indicators..
	fEditor->SendMessage(SCI_SETINDICATORCURRENT, IND_LINK);
	fEditor->SendMessage(SCI_INDICATORCLEARRANGE, 0, fEditor->SendMessage(SCI_GETTEXTLENGTH));
	fLastDocumentLinks.clear();
}

void
LSPEditorWrapper::_DoInitialize(value& params)
{
	fInitialized = true;
	didOpen();
	BMessage symbols;
	if (HasLSPServerCapability(kLCapDocumentSymbols))
		fEditor->SetDocumentSymbols(&symbols, IEditor::STATUS_REQUESTED);
	else
		fEditor->SetDocumentSymbols(&symbols, IEditor::STATUS_NO_CAPABILITY);

	RequestDocumentSymbols();
}

void
LSPEditorWrapper::_DoDocumentLink(lsp::TextDocument_DocumentLinkResult&& links)
{
	//FIXME? Check on Editor?

	_RemoveAllDocumentLinks();

	if (links.isNull())
		return;


	for (auto& l : links.value()) {

		Range& r = l.range;
		InfoRange ir;
		ir.from = FromLSPPositionToSciPosition(&r.start);
		ir.to = FromLSPPositionToSciPosition(&r.end);
		if (l.target)
			ir.info = l.target->toString();

		LogTrace("DocumentLink [%ld->%ld] [%s]", ir.from, ir.to, ir.info.c_str());
		fEditor->SendMessage(SCI_INDICATORFILLRANGE, ir.from, ir.to - ir.from);
		fLastDocumentLinks.push_back(ir);
	}
}


/*
void
LSPEditorWrapper::_DoDocumentLink(value& result)
{
	_RemoveAllDocumentLinks();

	for (auto& element : result.array()) {
		auto l = LSPBridge::fromJson<lsp::DocumentLink>(element);
		Range& r = l.range;
		InfoRange ir;
		ir.from = FromLSPPositionToSciPosition(&r.start);
		ir.to = FromLSPPositionToSciPosition(&r.end);
		if (l.target)
			ir.info = l.target->toString();

		LogTrace("DocumentLink [%ld->%ld] [%s]", ir.from, ir.to, ir.info.c_str());
		fEditor->SendMessage(SCI_INDICATORFILLRANGE, ir.from, ir.to - ir.from);
		fLastDocumentLinks.push_back(ir);
	}
}*/


void
LSPEditorWrapper::_DoFileStatus(value& params)
{
	auto state = params.object().get("state").string();
	LogInfo("FileStatus [%s] -> [%s]", GetFileStatus().String(), state.c_str());
	SetFileStatus(state.c_str());
	if (fEditor != nullptr) {
		BMessage msg(editor::StatusView::UPDATE_STATUS);
		BMessenger(fEditor).SendMessage(&msg);
	}
}


void
LSPEditorWrapper::_DoDocumentSymbol(lsp::TextDocument_DocumentSymbolResult&& result)
{
	if (result.isNull())
		return;

	BMessage msg(EDITOR_UPDATE_SYMBOLS);
	auto* symbolArray = std::get_if<lsp::Array<lsp::DocumentSymbol>>(&result.value());
	if (symbolArray) {
		_DoRecursiveDocumentSymbol(*symbolArray, msg);
	} else {
		auto* infoArray = std::get_if<lsp::Array<lsp::SymbolInformation>>(&result.value());
		if (infoArray)
			_DoLinearSymbolInformation(*infoArray, msg);
	}

	if (fEditor != nullptr)
		fEditor->SetDocumentSymbols(&msg, IEditor::STATUS_HAS_SYMBOLS);

}


void
LSPEditorWrapper::_DoRecursiveDocumentSymbol(lsp::Array<DocumentSymbol>& vect, BMessage& msg)
{
	for (auto& sym: vect) {
		BMessage symbol;
		symbol.AddString("name", sym.name.c_str());
		symbol.AddInt32("kind", static_cast<int32>(sym.kind.value()));
		symbol.AddString("detail", sym.detail.value_or("").c_str());
		BMessage child;
		if (sym.children.has_value() && sym.children->size() > 0) {
			_DoRecursiveDocumentSymbol(*sym.children, child);
		}
		symbol.AddMessage("children", &child);
		Range& symbolRange = sym.selectionRange;
		symbol.AddInt32("start:line", symbolRange.start.line + 1);
		symbol.AddInt32("start:character", symbolRange.start.character);
		Range& range = sym.range;
		symbol.AddInt32("range:start:line", range.start.line + 1);
		symbol.AddInt32("range:end:line", 	range.end.line + 1);
		msg.AddMessage("symbol", &symbol);
	}
}

void
LSPEditorWrapper::_DoLinearSymbolInformation(lsp::Array<SymbolInformation>& vect, BMessage& msg)
{
	for (auto& sym: vect) {
		BMessage symbol;
		symbol.AddString("name", sym.name.c_str());
		symbol.AddInt32("kind", static_cast<int32>(sym.kind.value()));
		Range& symbolRange = sym.location.range;
		symbol.AddInt32("start:line", symbolRange.start.line + 1);
		symbol.AddInt32("start:character", symbolRange.start.character);
		msg.AddMessage("symbol", &symbol);
	}
}

bool
LSPEditorWrapper::IsStatusValid()
{
	BString status = GetFileStatus();
	bool value = status.IsEmpty() || (status.Compare("idle") == 0);
	if (!value)
		LogDebugF("Invalid status (%d) for [%s] (%s)", value, GetFilenameURI().String(),
			GetFileStatus().String());
	return value;
}


void
LSPEditorWrapper::onNotify(std::string id, value& result)
{
	IF_ID("textDocument/clangd.fileStatus", _DoFileStatus);

	LogError("LSPEditorWrapper::onNotify not handled! [%s]", id.c_str());
}


void
LSPEditorWrapper::onResponse(RequestID id, value& result)
{
	LogError("LSPEditorWrapper::onResponse not handled! [%s]", id.c_str());
}


void
LSPEditorWrapper::onError(RequestID id, value& error)
{
	LogError("onError [%s] [%s]", GetFileStatus().String(), lsp::json::stringify(error).c_str());
}

/*
void
LSPEditorWrapper::onRequest(std::string method, value& params, value& ID)
{
	// LogError("onRequest not implemented! [%s] [%s] [%s]", method.c_str(), params.dump().c_str(),
	// ID.dump().c_str());
}
*/
/*
void
LSPEditorWrapper::OnHover(lsp::TextDocument_HoverResult& hover_res)
{
	if (fEditor == nullptr) {
		return;
	}

	BAutolock lock(fEditor->Window());
	if (!fEditor->Window()->IsActive()) {
		return;
	}

	if (hover_res.isNull()){
		EndHover();
		return;
	}

	std::string tip = _ExtractHoverText(hover_res.value());

	if (tip.empty()) {
		EndHover();
		return;
	}

	_ShowToolTip(tip.c_str());
 }

*/

// utility
void
LSPEditorWrapper::FromSciPositionToLSPPosition(const Sci_Position& pos, Position* lsp_position)
{
	lsp_position->line = static_cast<unsigned int>(fEditor->SendMessage(SCI_LINEFROMPOSITION, pos, 0));
	Sci_Position end_pos = fEditor->SendMessage(SCI_POSITIONFROMLINE, lsp_position->line, 0);
	lsp_position->character = static_cast<unsigned int>(fEditor->SendMessage(SCI_COUNTCHARACTERS, end_pos, pos));
}


Sci_Position
LSPEditorWrapper::FromLSPPositionToSciPosition(const Position* lsp_position)
{
	Sci_Position sci_position;
	sci_position = fEditor->SendMessage(SCI_POSITIONFROMLINE, lsp_position->line, 0);
	sci_position
		= fEditor->SendMessage(SCI_POSITIONRELATIVE, sci_position, lsp_position->character);
	return sci_position;
}


void
LSPEditorWrapper::GetCurrentLSPPosition(Position* lsp_position)
{
	const Sci_Position pos = fEditor->SendMessage(SCI_GETSELECTIONSTART, 0, 0);
	FromSciPositionToLSPPosition(pos, lsp_position);
}


void
LSPEditorWrapper::FromSciPositionToRange(Sci_Position s_start, Sci_Position s_end, Range* range)
{
	FromSciPositionToLSPPosition(s_start, &range->start);
	FromSciPositionToLSPPosition(s_end, &range->end);
}


Sci_Position
LSPEditorWrapper::ApplyTextEdit(value& textEditJson)
{
	TextEdit textEdit;
	lsp::fromJson(lsp::json::Value(textEditJson), textEdit);
	return ApplyTextEdit(textEdit);
}


Sci_Position
LSPEditorWrapper::ApplyTextEdit(TextEdit &textEdit)
{
	Range range = textEdit.range;
	Sci_Position s_pos = FromLSPPositionToSciPosition(&range.start);
	Sci_Position e_pos = FromLSPPositionToSciPosition(&range.end);

	fEditor->SendMessage(SCI_SETTARGETRANGE, s_pos, e_pos);

	Sci_Position replaced
		= fEditor->SendMessage(SCI_REPLACETARGET, -1, (sptr_t) (textEdit.newText.c_str()));

	return s_pos + replaced;
}


void
LSPEditorWrapper::OpenFileURI(std::string uri, int32 line, int32 character,
					ArrayTextEdit&& edits)
{
	BUrl url = uri.c_str();
	if (url.IsValid() && url.HasPath()) {
		BEntry entry(url.Path().String());
		entry_ref ref;
		if (entry.Exists()) {
			BMessage refs(B_REFS_RECEIVED);
			if (entry.GetRef(&ref) == B_OK) {
				refs.AddRef("refs", &ref);
				if (line != -1) {
					refs.AddInt32("start:line", line);
					if (character != -1)
						refs.AddInt32("start:character", character);
				}
				if (edits.empty() == false) {
					refs.AddString("edit",  lsp::json::stringify(lsp::toJson(std::move(edits))).c_str());
				}

				JumpNavigator::getInstance()->JumpToFile(&refs, fEditor->FileRef());
			}
		} else {
			LogError("OpenFileURI: file does not exist %s", uri.c_str());
		}
	} else {
		LogError("Invalid document URI (%s)", uri.c_str());
	}
}


std::string
LSPEditorWrapper::GetCurrentLine()
{
	const Sci_Position len = fEditor->SendMessage(SCI_GETCURLINE, 0, (sptr_t) nullptr);
	std::string text(len, '\0');
	fEditor->SendMessage(SCI_GETCURLINE, len, (sptr_t) (&text[0]));
	return text;
}

// end - utility
