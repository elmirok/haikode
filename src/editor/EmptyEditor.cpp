/*
 * Copyright 2025 Andrea Anzani <andrea.anzani@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "EmptyEditor.h"

#include <Rect.h>

EmptyEditor::EmptyEditor()
	:
	BTextView("EmptyEditor", B_WILL_DRAW | B_FRAME_EVENTS),
	fId(0),
	fName("Empty")
{
}


EmptyEditor::~EmptyEditor()
{
}
