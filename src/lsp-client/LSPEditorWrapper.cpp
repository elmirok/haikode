/*
 * Copyright 2023-2026, Andrea Anzani
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "LSPEditorWrapper.h"

#include <Alert.h>
#include <Application.h>
#include <Debug.h>
#include <Path.h>
#include <Catalog.h>
#include <Window.h>

#include <algorithm>
#include <cstdio>
#include <debugger.h>
#include <fstream>
#include <unistd.h>

#include "Editor.h"
#include "EditorMessages.h"
#include "EditorStatusView.h"
#include "GrepThread.h" // MSG_REPORT_RESULT definition
#include "JumpNavigator.h"
#include "LSPCompat.h"
#include "Log.h"
#include "LSPJsonBridge.h"
#include "LSPProjectWrapper.h"
#include "TextUtils.h"


#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Editor"


#define IF_ID(METHOD_NAME, METHOD) if (id.compare(METHOD_NAME) == 0) { METHOD(result); return; }


LSPEditorWrapper::LSPEditorWrapper(const BPath& filenamePath, Editor* editor)
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
	ASSERT(fEditor);
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

	fLSPProjectWrapper->UnregisterTextDocument(this);
	didClose();
	fFileStatus = "";
	fLSPProjectWrapper = nullptr;
}


bool
LSPEditorWrapper::HasLSPServer() const
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
		auto& diag = fLastDiagnostics.at(diaIndex);
		if (!diag.codeActions.has_value() || !diag.codeActions->isArray())
			return;

		auto& actionsArray = diag.codeActions->array();
		if (actIndex < 0 || (size_t)actIndex >= actionsArray.size())
			return;

		auto action = LSPBridge::fromJson<lsp::CodeAction>(actionsArray[actIndex]);
		if (!action.edit.has_value() || !action.edit->changes.has_value())
			return;

		for (auto& [uri, edits] : action.edit->changes.value()) {
			if (GetFilenameURI().ICompare(uri.toString().c_str()) == 0) {
				OnFormat(std::move(edits));
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
	OnFormat(std::move(edits));
}


void
LSPEditorWrapper::SetLSPServer(LSPProjectWrapper* cW)
{
	ASSERT(cW);
	ASSERT(fEditor);

	if (fLSPProjectWrapper != nullptr)
		UnsetLSPServer();

	SetFileType(fEditor->FileType().c_str());

	fLSPProjectWrapper = cW;

	// the document registration (didOpen, etc..)
	// is performed in a second step (when the editor is selected for the first time)
}

void
LSPEditorWrapper::RegisterDocument()
{
	if(fLSPProjectWrapper == nullptr)
		return;

	LogInfo("Registering Document to LSP [%s]\n", GetFilenameURI().String());

	if (!fLSPProjectWrapper->RegisterTextDocument(this)) {
		fLSPProjectWrapper = nullptr;
	}
}


bool
LSPEditorWrapper::IsInitialized() const
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

	// printf("DIDOPEN %s\n", GetFilenameURI().String());
}


void
LSPEditorWrapper::didClose()
{
	if (!IsInitialized())
		return;

	ResetVersion();
	fLSPProjectWrapper->DidClose(this);

	if (fEditor != nullptr) {
		_RemoveAllDiagnostics();
		_RemoveAllDocumentLinks();
	}
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

	NextVersion();

	Sci_Position end_pos = fEditor->SendMessage(SCI_POSITIONRELATIVE, start_pos, poslength);

	lsp::TextDocumentContentChangeEvent_Range_Text event;
	lsp::Range range;
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
		fLSPProjectWrapper->DidChange(this, fChanges);
		fChanges.clear();
	}
}

void
LSPEditorWrapper::OnFormat(ArrayTextEdit&& edits)
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
LSPEditorWrapper::OnRename(lsp::WorkspaceEdit&& edit)
{
	if (edit.changes.has_value()) {
		for (auto& [uri, edits] : edit.changes.value()) {
			OpenFileURI(uri, -1, -1, std::move(edits));
		}
	} else if (edit.documentChanges.has_value()) {
		for (auto& change : edit.documentChanges.value()) {
			auto* textDocEdit = std::get_if<lsp::TextDocumentEdit>(&change);
			if (textDocEdit) {
				OpenFileURI(textDocEdit->textDocument.uri, -1, -1, (ArrayTextEdit&&)(std::move(textDocEdit->edits)));
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
		lsp::Range range;
		FromSciPositionToRange(s_start, s_end, &range);
		fLSPProjectWrapper->RangeFomatting(this, range);
	} else {
		fLSPProjectWrapper->Formatting(this);
	}
}


void
LSPEditorWrapper::GoTo(LSPEditorWrapper::GoToType type)
{
	if (!IsInitialized()|| !fEditor)
		return;

	flushChanges();

	lsp::Position position;
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
LSPEditorWrapper::Rename(const lsp::Position& position)
{
	if (!IsInitialized()|| !fEditor)
		return;

	flushChanges();
	fLSPProjectWrapper->Rename(this, position);
}


void
LSPEditorWrapper::Rename()
{
	lsp::Position position;
	GetCurrentLSPPosition(&position);

	Rename(position);
}


void
LSPEditorWrapper::StartHover(Sci_Position sci_position)
{
	if (!IsInitialized() || sci_position < 0) {
		return;
	}

	LSPDiagnostic dia;
	if (DiagnosticFromPosition(sci_position, dia) > -1) {
		_ShowToolTip(dia.range.info.c_str());
		return;
	}

	lsp::Position position;
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
		for (const auto& ir : fLastDiagnostics) {
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
LSPEditorWrapper::DiagnosticFromRange(const lsp::Range& range, LSPDiagnostic& dia) const
{
	int32 index = -1;
	for (const auto& ir : fLastDiagnostics) {
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

	if (fCurrentCompletion.size() > 0) {
		for (auto& item : fCurrentCompletion) {
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
	fCurrentCompletion = {};
}


void
LSPEditorWrapper::StartCompletion()
{
	if (!IsInitialized() || !fEditor)
		return;

	flushChanges();

	// let's check if a completion is ongoing
	if (fCurrentCompletion.size() > 0) {
		// let's close the current Scintilla listbox
		fEditor->SendMessage(SCI_AUTOCCANCEL, 0, 0);
		// let's cancel any previous request running on clangd
		// --> TODO: cancel previous clangd request!

		// let's clean-up current request details:
		this->fCurrentCompletion = {};
	}

	lsp::Position position;
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
			lsp::Position lsp_position;
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
		lsp::Position position;
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

	const auto& arr = std::get<lsp::Array<lsp::MarkedString>>(hover.contents);
	std::string combined;
	for (const auto& ms : arr) {
		if (!combined.empty())
			combined += "\n\n";
		combined += _MarkedStringToText(ms);
	}
	return combined;
}


void
LSPEditorWrapper::OnHover(lsp::TextDocument_HoverResult&& result)
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
LSPEditorWrapper::OnGoTo(lsp::TextDocument_DefinitionResult&& result)
{
	lsp::Location location;
	if (LocationFromDefinition(std::move(result), location) == false)
		return;

	const lsp::Position& pos = location.range.start;
	OpenFileURI(location.uri, pos.line + 1, pos.character);
}


bool
LSPEditorWrapper::LocationFromDefinition(lsp::TextDocument_DefinitionResult&& result,
	lsp::Location& retLoc)
{
	// NullOrOneOf<Definition, Array<DefinitionLink>>;
	if (result.isNull())
		return false;

	// OneOf<Definition, Array<DefinitionLink>>
	auto* definition = std::get_if<lsp::Definition>(&result.value());
	if (definition) {
		//using Definition = OneOf<Location, Array<Location>>;
		const lsp::Location* location = std::get_if<lsp::Location>(definition);
		if (location != nullptr) {
		} else {
			auto* array = std::get_if<lsp::Array<lsp::Location>>(definition);
			location = &(*array)[0];
		}

		if (location) {
			retLoc = *location;
			return true;
		}
	} else {
		//Array<DefinitionLink>
		auto* array = std::get_if<lsp::Array<lsp::DefinitionLink>>(&result.value());
		if (array) {
			const lsp::LocationLink& location = (*array)[0];
			retLoc.uri = location.targetUri;
			retLoc.range = location.targetRange;
			return true;
		}
	}

	return false;
}


void
LSPEditorWrapper::OnFindReferences(lsp::TextDocument_ReferencesResult&& result, std::string symbolName)
{
	//FIXME, TODO: should me moved to the LSPProject?:
	// should appear if: invalid Definition at curson.
	//	no references found.

	if (result.isNull()) {

		auto alert = new BAlert("NoReferencesFound",
			B_TRANSLATE("No references found!"),
			B_TRANSLATE("OK"), NULL, NULL,
			B_WIDTH_AS_USUAL, B_OFFSET_SPACING, B_WARNING_ALERT);
		alert->Go();
		return;
	}

	//Simulating the Search panel results..

	BMessage references(kReferences);
	std::string currentFileName = "";
	std::string nextFileName;
	BMessage currentMessage;
	entry_ref currentRef;

	for (lsp::Location& location : result.value()) {

		nextFileName = location.uri.path().data();

		if (currentFileName.compare(nextFileName) != 0) {

			if (currentFileName.empty() != true)
				references.AddMessage("file", &currentMessage);

			currentFileName = nextFileName;
			BEntry entry(nextFileName.c_str());
			entry.GetRef(&currentRef);

			currentMessage.MakeEmpty();
			currentMessage.what = MSG_REPORT_RESULT;
			currentMessage.AddString("filename", currentFileName.c_str());
		}

		BMessage lineMessage;
		lineMessage.what = B_REFS_RECEIVED;
		BString txt;
		txt << location.range.start.line + 1 << ": " << symbolName.c_str();
		lineMessage.AddString("text", txt);
		lineMessage.AddRef("refs", &currentRef);
		lineMessage.AddInt32("start:line", location.range.start.line + 1);
		lineMessage.AddInt32("start:character", location.range.start.character);
		currentMessage.AddMessage("line", &lineMessage);
	}

	references.AddMessage("file", &currentMessage);
	fEditor->SetReferences(&references);
}


void
LSPEditorWrapper::OnSignatureHelp(lsp::SignatureHelp&& signatureHelp)
{
	fCallTip.UpdateSignatures(signatureHelp.signatures);
	fCallTip.ShowCallTip();
}


void
LSPEditorWrapper::OnCompletion(lsp::TextDocument_CompletionResult&& result)
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
	lsp::Position position;
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

			lsp::Position pos;
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
		fCurrentCompletion = *items;
		fEditor->SendMessage(SCI_AUTOCSETSEPARATOR, (int) '\n', 0);
		fEditor->SendMessage(SCI_AUTOCSETIGNORECASE, true);
		fEditor->SendMessage(SCI_AUTOCGETCANCELATSTART, false);
		fEditor->SendMessage(SCI_AUTOCSETORDER, SC_ORDER_CUSTOM, 0);

		// whats' the text already selected so far?
		auto* firstTe = _GetTextEdit(fCurrentCompletion[0]);
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
LSPEditorWrapper::OnDiagnostics(lsp::json::Value&& params)
{
	auto* versionField = params.object().find("version");
	if (versionField && !versionField->isNull()) {
		int32 serverVersion = static_cast<int32>(versionField->integer());
		if (serverVersion < Version()) {
			LogTrace("Discarding stale diagnostics: server=%ld local=%ld", serverVersion, Version());
			return;
		}
	}

	_RemoveAllDiagnostics();

	auto* diagnosticsArray = params.object().find("diagnostics");
	if (!diagnosticsArray || diagnosticsArray->isNull())
		return;

	for (auto& diagJson : diagnosticsArray->array()) {
		LSPDiagnostic lspDiag;

		// Parse the standard diagnostic fields
		lsp::fromJson(lsp::json::Value(diagJson), lspDiag.diagnostic);

		lsp::Range& r = lspDiag.diagnostic.range;
		InfoRange& ir = lspDiag.range;
		ir.from = FromLSPPositionToSciPosition(&r.start);
		ir.to = FromLSPPositionToSciPosition(&r.end);
		ir.info = lspDiag.diagnostic.message;

		// Extract clangd extension: category
		auto* category = diagJson.object().find("category");
		if (category && !category->isNull())
			lspDiag.category = category->string();

		// Extract clangd extension: inline code actions
		auto* codeActions = diagJson.object().find("codeActions");
		if (codeActions && codeActions->isArray() && !codeActions->array().empty())
			lspDiag.codeActions = lsp::json::Value(*codeActions);

		LogTrace("Diagnostics [%ld->%ld] [%s][%s]\n", ir.from, ir.to, ir.info.c_str(),
			lspDiag.diagnostic.message.c_str());
		fEditor->SendMessage(SCI_INDICATORFILLRANGE, ir.from, ir.to - ir.from);

		fLastDiagnostics.push_back(std::move(lspDiag));
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
LSPEditorWrapper::FindReferences()
{
	if (!IsInitialized() || !fEditor)
		return;

	lsp::Position position;
	GetCurrentLSPPosition(&position);

	fLSPProjectWrapper->References(this, position);
}


void
LSPEditorWrapper::OnCodeActions(lsp::json::Value&& codeActionsJson)
{
	if (!codeActionsJson.isArray())
		return;

	for (auto& actJson : codeActionsJson.array()) {
		if (!actJson.isObject())
			continue;

		// Match code actions to diagnostics by range
		auto* diagnosticsField = actJson.object().find("diagnostics");
		if (!diagnosticsField || !diagnosticsField->isArray())
			continue;

		for (auto& diagJson : diagnosticsField->array()) {
			lsp::Range range;
			auto* rangeField = diagJson.object().find("range");
			if (!rangeField)
				continue;
			lsp::fromJson(lsp::json::Value(*rangeField), range);

			for (auto& d : fLastDiagnostics) {
				if (d.diagnostic.range == range) {
					if (!d.codeActions.has_value())
						d.codeActions = lsp::json::Value(lsp::json::Array{});
					d.codeActions->array().push_back(lsp::json::Value(actJson));
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
LSPEditorWrapper::OnInitialize(lsp::json::Value& params)
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
LSPEditorWrapper::OnDocumentLink(lsp::TextDocument_DocumentLinkResult&& links)
{
	//FIXME? Check on Editor?

	_RemoveAllDocumentLinks();

	if (links.isNull())
		return;

	for (auto& l : links.value()) {
		const lsp::Range& r = l.range;
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


void
LSPEditorWrapper::OnFileStatus(lsp::json::Value& params)
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
LSPEditorWrapper::OnDocumentSymbol(lsp::TextDocument_DocumentSymbolResult&& result)
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
LSPEditorWrapper::_DoRecursiveDocumentSymbol(lsp::Array<lsp::DocumentSymbol>& vect, BMessage& msg)
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
		lsp::Range& symbolRange = sym.selectionRange;
		symbol.AddInt32("start:line", symbolRange.start.line + 1);
		symbol.AddInt32("start:character", symbolRange.start.character);

		const lsp::Range& range = sym.range;
		symbol.AddInt32("range:start:line", range.start.line + 1);
		symbol.AddInt32("range:end:line", 	range.end.line + 1);
		msg.AddMessage("symbol", &symbol);
	}
}


void
LSPEditorWrapper::_DoLinearSymbolInformation(lsp::Array<lsp::SymbolInformation>& vect, BMessage& msg)
{
	for (auto& sym: vect) {
		BMessage symbol;
		symbol.AddString("name", sym.name.c_str());
		symbol.AddInt32("kind", static_cast<int32>(sym.kind.value()));
		lsp::Range& symbolRange = sym.location.range;
		symbol.AddInt32("start:line", symbolRange.start.line + 1);
		symbol.AddInt32("start:character", symbolRange.start.character);
		msg.AddMessage("symbol", &symbol);
	}
}


// utility
void
LSPEditorWrapper::FromSciPositionToLSPPosition(const Sci_Position& pos, lsp::Position* lsp_position)
{
	lsp_position->line = static_cast<unsigned int>(fEditor->SendMessage(SCI_LINEFROMPOSITION, pos, 0));
	Sci_Position end_pos = fEditor->SendMessage(SCI_POSITIONFROMLINE, lsp_position->line, 0);
	lsp_position->character = static_cast<unsigned int>(fEditor->SendMessage(SCI_COUNTCHARACTERS, end_pos, pos));
}


Sci_Position
LSPEditorWrapper::FromLSPPositionToSciPosition(const lsp::Position* lsp_position)
{
	Sci_Position sci_position;
	sci_position = fEditor->SendMessage(SCI_POSITIONFROMLINE, lsp_position->line, 0);
	sci_position
		= fEditor->SendMessage(SCI_POSITIONRELATIVE, sci_position, lsp_position->character);
	return sci_position;
}


void
LSPEditorWrapper::GetCurrentLSPPosition(lsp::Position* lsp_position)
{
	const Sci_Position pos = fEditor->SendMessage(SCI_GETSELECTIONSTART, 0, 0);
	FromSciPositionToLSPPosition(pos, lsp_position);
}


void
LSPEditorWrapper::FromSciPositionToRange(Sci_Position s_start, Sci_Position s_end, lsp::Range* range)
{
	FromSciPositionToLSPPosition(s_start, &range->start);
	FromSciPositionToLSPPosition(s_end, &range->end);
}


Sci_Position
LSPEditorWrapper::ApplyTextEdit(lsp::json::Value& textEditJson)
{
	lsp::TextEdit textEdit;
	lsp::fromJson(lsp::json::Value(textEditJson), textEdit);
	return ApplyTextEdit(textEdit);
}


Sci_Position
LSPEditorWrapper::ApplyTextEdit(lsp::TextEdit &textEdit)
{
	lsp::Range range = textEdit.range;
	Sci_Position s_pos = FromLSPPositionToSciPosition(&range.start);
	Sci_Position e_pos = FromLSPPositionToSciPosition(&range.end);

	fEditor->SendMessage(SCI_SETTARGETRANGE, s_pos, e_pos);

	Sci_Position replaced
		= fEditor->SendMessage(SCI_REPLACETARGET, -1, (sptr_t) (textEdit.newText.c_str()));

	return s_pos + replaced;
}


void
LSPEditorWrapper::OpenFileURI(const lsp::FileUri& uri, int32 line, int32 character,
					ArrayTextEdit&& edits)
{
	if (uri.isValid()) {
		BEntry entry(uri.path().data());
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
				int32 charInLine = 0;
				int32 line = fEditor->GetCurrentLineNumber(&charInLine);
				JumpPosition jumpPos = { *fEditor->FileRef(),  line + 1, charInLine};
				JumpNavigator::getInstance()->JumpToFile(&refs, &jumpPos);
			}
		} else {
			LogError("OpenFileURI: file does not exist %s", uri.path().data());
		}
	} else {
		LogError("Invalid document URI (%s)", uri.path().data());
	}
}


std::string
LSPEditorWrapper::ExtractSymbolFromFile(const lsp::Location& loc)
{
    std::ifstream file(loc.uri.path().data());
    if (!file.is_open())
		return "no open";

    std::string line;
    lsp::uint currentLine = 0;

	//TODO: Multiline line symbols?
    while (std::getline(file, line)) {
        if (currentLine == loc.range.start.line) {

            size_t start = loc.range.start.character;
            size_t end = loc.range.end.character - 1;

            if (start == std::string::npos)
				return "";
            return line.substr(start, end - start + 1);
        }
        currentLine++;
    }

    return "";
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
