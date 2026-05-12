/*
 * Copyright 2025, Andrea Anzani
 * All rights reserved. Distributed under the terms of the MIT license.
 */
#pragma once

#include <stack>

#include <Entry.h>

class BMessage;
// Once per project or a single one?

struct JumpPosition {
	entry_ref ref;
	int32 line;
	int32 character;
};

bool operator==(const JumpPosition& a, const JumpPosition& b);

class JumpNavigator {
public:

    static JumpNavigator* getInstance() {
		static JumpNavigator	instance;
        return &instance;
    }

	void	JumpToFile(BMessage* message, JumpPosition* currentPosition);
	void	JumpingTo(JumpPosition& newPosition, JumpPosition& fromPosition);

	bool	HasNext() const;
	bool	HasPrev() const;

	void	JumpToNext();
	void	JumpToPrev();

private:
			JumpNavigator();
	void	_GoToCurrentPosition();

	std::stack<JumpPosition>	history;
	std::stack<JumpPosition>	forwardStack;
	JumpPosition				fCurrentPosition;
};
