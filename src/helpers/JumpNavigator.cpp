/*
 * Copyright 2025, Andrea Anzani
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "JumpNavigator.h"

#include <Application.h>
#include <Message.h>
#include <cstdio>

#include "Log.h"

bool
operator==(const JumpPosition& a, const JumpPosition& b)
{
	// NOTE: not checking character position to avoid stupid jumps to be recorded!
	return /*a.character == b.character && */ a.line == b.line && a.ref == b.ref;
}


JumpNavigator::JumpNavigator()
{
	fCurrentPosition.ref.device = -1;
	fCurrentPosition.character = -1;
	fCurrentPosition.line = -1;
}


void
JumpNavigator::JumpToFile(BMessage* message, JumpPosition* currentPosition)
{
	entry_ref ref;
	if (message->FindRef("refs", 0, &ref) == B_OK) {
		message->AddRef("jumpFromRef", &currentPosition->ref);
		message->what = B_REFS_RECEIVED;
		message->AddInt32("jumpFromLine", currentPosition->line);
		message->AddInt32("jumpFromCharacter", currentPosition->character);
		be_app->PostMessage(message);
	}
}


void
JumpNavigator::JumpingTo(JumpPosition& newPosition, JumpPosition& fromPosition)
{
	LogDebugF("from %s (line: %ld, char: %ld) to %s (line: %ld, char: %ld)\n",
		fromPosition.ref.name, fromPosition.line, fromPosition.character,
		newPosition.ref.name, newPosition.line, newPosition.character);

	if (newPosition == fromPosition || newPosition == fCurrentPosition)
		return;

	history.push(fromPosition);
	forwardStack = {};
	fCurrentPosition = newPosition;
}


bool
JumpNavigator::HasNext() const
{
	return !forwardStack.empty();
}


bool
JumpNavigator::HasPrev() const
{
	return !history.empty();
}


void
JumpNavigator::JumpToNext()
{
	if (HasNext()) {
		history.push(fCurrentPosition);
		fCurrentPosition = forwardStack.top();
		forwardStack.pop();
		_GoToCurrentPosition();
	}
}


void
JumpNavigator::JumpToPrev()
{
	if (HasPrev()) {
		forwardStack.push(fCurrentPosition);
		fCurrentPosition = history.top();
		history.pop();
		_GoToCurrentPosition();
	}
}


void
JumpNavigator::_GoToCurrentPosition()
{
	LogDebugF("%s %d:%d\n", fCurrentPosition.ref.name,fCurrentPosition.line,fCurrentPosition.character);
	BMessage ref(B_REFS_RECEIVED);
	ref.AddRef("refs", &fCurrentPosition.ref);
	ref.AddInt32("start:line", fCurrentPosition.line);
	ref.AddInt32("start:character", fCurrentPosition.character);

	be_app->PostMessage(&ref);
}
