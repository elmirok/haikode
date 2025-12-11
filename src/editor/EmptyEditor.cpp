/*
 * Copyright 2025 Andrea Anzani <andrea.anzani@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "EmptyEditor.h"
#include "GenioWindowMessages.h"
#include <Rect.h>

EmptyEditor::EmptyEditor()
	:
	BTextView("EmptyEditor", B_WILL_DRAW | B_FRAME_EVENTS),
	fId(GenerateEditorId()),
	fName("Empty")
{
}


EmptyEditor::~EmptyEditor()
{
}

void
EmptyEditor::MessageReceived(BMessage* message)
{
    switch(message->what) {
        case MSG_DUPLICATE_LINE:
            printf("EmptyEditor::MessageReceived - MSG_DUPLICATE_LINE\n");
            break;
        default:    
        BTextView::MessageReceived(message);
    }
}


void
EmptyEditor::GrabFocus()
{
	MakeFocus(true);
}
