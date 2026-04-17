/*
 * Copyright 2023-2026, Andrea Anzani
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include "Editor.h"

#include <lsp/types.h>
#include <cstdio>
#include <cstring>


enum CallTipAction {
	CALLTIP_NOTHING = 0, //nothing to do
	CALLTIP_NEWDATA = 2, //request LSP for new data (using the Position())
	CALLTIP_UPDATE  = 3  //just call ShowCalltip() to refresh the current calltip
};

#define MAX_LINE_DATA	256

class CallTipContext {
public:
	explicit CallTipContext(Editor* editor);

	CallTipAction UpdateCallTip(int ch, bool forceUpdate = false);

	void NextCallTip();
	void PrevCallTip();
	bool IsVisible();
	void HideCallTip();
	void ShowCallTip();

	void UpdateSignatures(const lsp::Array<lsp::SignatureInformation>& signatures);

	int32 Position() const { return fPosition; };

private:
	CallTipAction _FindFunction();
	void 		  _Reset();

	Editor* fEditor;
	int32	fCallTipPosition;
	int32 	fPosition;
	BString fCurrentFunctionName;
	int32 	fCurrentFunction;
	size_t 	fCurrentParam;

	// LSP signatures
	lsp::Array<lsp::SignatureInformation> fSignatures;
};
