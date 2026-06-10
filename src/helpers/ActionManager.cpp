/*
 * Copyright 2023-2024, Andrea Anzani
 * All rights reserved. Distributed under the terms of the MIT license.
 */


#include "ActionManager.h"

#include <Button.h>
#include <vector>

#include "ToolBar.h"

ActionManager ActionManager::sInstance;

class ActionMenuItem;
class Action {
public:
	BString	label;
	BString iconResourceName;
	BString toolTip;
	uint32  modifiers;
	char	shortcut;
	bool	enabled;
	bool	pressed;

	std::vector<ActionMenuItem*> menuItemList;
	std::vector<ToolBar*> toolBarList;
};


class ActionMenuItem : public BMenuItem {
public:
	ActionMenuItem(Action* action, BMessage* message)
	:
	BMenuItem(action->label, message, action->shortcut, action->modifiers),
	fAction(action)
	{
	}

	virtual ~ActionMenuItem()
	{
		std::vector<ActionMenuItem*> &v = fAction->menuItemList;
		std::erase(v, this);
	}
private:
	Action* fAction;
};


ActionManager::~ActionManager()
{
	ActionMap::reverse_iterator it;
	for (it = sInstance.fActionMap.rbegin(); it != sInstance.fActionMap.rend(); it++) {
		delete it->second;
	}
	sInstance.fActionMap.clear();
}


status_t
ActionManager::RegisterAction(int32   msgWhat,
								BString label,
								BString toolTip,
								BString iconResource,
								char shortcut,
								uint32 modifiers)
{
	Action* action = new Action();
	action->enabled = true; //by default?
	action->pressed = false;
	action->label = label;
	action->iconResourceName = iconResource;
	action->toolTip = toolTip;
	action->shortcut = shortcut;
	action->modifiers = modifiers;
	sInstance.fActionMap[msgWhat] = action;
	return B_OK;
}


status_t
ActionManager::AddItem(int32 msgWhat, BMenu* menu, BMessage* extraFields)
{
	ActionMap::iterator iterator = sInstance.fActionMap.find(msgWhat);
	if (iterator == sInstance.fActionMap.end())
		return B_ERROR;

	Action* action = iterator->second;
	if (extraFields == nullptr) {
		extraFields = new BMessage(msgWhat);
	}
	extraFields->what = msgWhat;
	ActionMenuItem* item = new ActionMenuItem(action, extraFields);
	menu->AddItem(item);
	item->SetEnabled(action->enabled);
	item->SetMarked(action->pressed);
	action->menuItemList.push_back(item);
	return B_OK;
}


status_t
ActionManager::AddItem(int32 msgWhat, ToolBar* bar, BMessage* extraFields)
{
	ActionMap::iterator iterator = sInstance.fActionMap.find(msgWhat);
	if (iterator == sInstance.fActionMap.end())
		return B_ERROR;

	Action* action = iterator->second;
	if (extraFields == nullptr) {
		extraFields = new BMessage(msgWhat);
	}
	extraFields->what = msgWhat;
	bar->AddAction(extraFields, action->toolTip, action->iconResourceName, true);
	bar->SetActionEnabled(msgWhat, action->enabled);
	bar->SetActionPressed(msgWhat, action->pressed);
	action->toolBarList.push_back(bar);
	return B_OK;
}


/* static */
void
ActionManager::RemoveItem(int32 msgWhat, BMenu* menu)
{
	ActionMap::iterator iterator = sInstance.fActionMap.find(msgWhat);
	if (iterator == sInstance.fActionMap.end())
		return;

	Action* action = iterator->second;
	BMenuItem* item = menu->FindItem(msgWhat);
	std::vector<ActionMenuItem*> &v = action->menuItemList;
	std::erase(v, item);

	menu->RemoveItem(item);
}


status_t
ActionManager::SetEnabled(int32 msgWhat, bool enabled)
{
	ActionMap::iterator iterator = sInstance.fActionMap.find(msgWhat);
	if (iterator == sInstance.fActionMap.end())
		return B_ERROR;

	Action* action = iterator->second;
	action->enabled = enabled;

	for (auto menuItem : action->menuItemList)
		menuItem->SetEnabled(enabled);
	for (auto toolBar : action->toolBarList)
		toolBar->SetActionEnabled(msgWhat, enabled);

	return B_OK;
}


status_t
ActionManager::SetPressed(int32 msgWhat, bool pressed)
{
	ActionMap::iterator iterator = sInstance.fActionMap.find(msgWhat);
	if (iterator == sInstance.fActionMap.end())
		return B_ERROR;

	Action* action = iterator->second;
	action->pressed = pressed;

	for (auto menuItem : action->menuItemList)
		menuItem->SetMarked(pressed);
	for (auto toolBar : action->toolBarList)
		toolBar->SetActionPressed(msgWhat, pressed);

	return B_OK;

}


status_t
ActionManager::SetToolTip(int32 msgWhat, const char* tooltip)
{
	ActionMap::iterator iterator = sInstance.fActionMap.find(msgWhat);
	if (iterator == sInstance.fActionMap.end())
		return B_ERROR;

	Action* action = iterator->second;
	action->toolTip = tooltip;

	for (auto toolBar : action->toolBarList) {
		BButton* button = toolBar->FindButton(msgWhat);
		if (button)
			button->SetToolTip(action->toolTip);
	}

	return B_OK;
}


bool
ActionManager::IsPressed(int32 msgWhat)
{
	ActionMap::iterator iterator = sInstance.fActionMap.find(msgWhat);
	if (iterator == sInstance.fActionMap.end())
		return false;

	Action* action = iterator->second;
	return action->pressed;
}


bool
ActionManager::IsEnabled(int32 msgWhat)
{
	ActionMap::iterator iterator = sInstance.fActionMap.find(msgWhat);
	if (iterator == sInstance.fActionMap.end())
		return false;

	Action* action = iterator->second;
	return action->enabled;
}


/*static*/
BMessage*
ActionManager::GetMessage(int32 msgWhat, BMenu* menu)
{
	ActionMap::iterator iterator = sInstance.fActionMap.find(msgWhat);
	if (iterator == sInstance.fActionMap.end())
		return nullptr;

	Action* action = iterator->second;
	for (auto menuItem : action->menuItemList)
		if (menuItem->Menu() == menu)
			return menuItem->Message();

	return nullptr;
}
