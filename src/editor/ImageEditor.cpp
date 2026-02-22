/*
 * Copyright 2025 Andrea Anzani <andrea.anzani@gmail.com>
 * All rights reserved. Distributed under the terms of the MIT license.
 */

#include "ImageEditor.h"

#include <Bitmap.h>
#include <BitmapStream.h>
#include <Catalog.h>
#include <Entry.h>
#include <File.h>
#include <NodeMonitor.h>
#include <Path.h>
#include <TranslatorRoster.h>

#include "ProjectFolder.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "ImageEditor"

ImageEditor::ImageEditor(entry_ref* ref, const BMessenger& target)
	: BView(ref->name, B_WILL_DRAW | B_FRAME_EVENTS)
	, fId(GenerateEditorId())
	, fFileRef(*ref)
	, fFileName(ref->name)
	, fFileType("image")
	, fTarget(target)
	, fProjectFolder(nullptr)
	, fBitmap(nullptr)
	, fScale(1.0f)
	, fOffset(0, 0)
	, fDragging(false)
{
	SetViewColor(ui_color(B_PANEL_BACKGROUND_COLOR));
	SetLowUIColor(B_PANEL_BACKGROUND_COLOR);
	SetHighUIColor(B_PANEL_TEXT_COLOR);
}


ImageEditor::~ImageEditor()
{
	StopMonitoring();
	delete fBitmap;
}


void
ImageEditor::SetProjectFolder(ProjectFolder* project)
{
	fProjectFolder = project;
}


void
ImageEditor::GrabFocus()
{
	MakeFocus(true);
}


const BString
ImageEditor::FilePath() const
{
	BPath path(&fFileRef);
	return BString(path.Path());
}


status_t
ImageEditor::SetFileRef(entry_ref* ref)
{
	if (ref == nullptr)
		return B_BAD_VALUE;

	fFileRef = *ref;
	fFileName = ref->name;
	SetName(ref->name);

	return B_OK;
}


status_t
ImageEditor::LoadFromFile()
{
	status_t status = _LoadImage();
	if (status == B_OK) {
		_ZoomToFit();
		Invalidate();

		// Get node_ref for monitoring
		BEntry entry(&fFileRef);
		if (entry.InitCheck() == B_OK) {
			entry.GetNodeRef(&fNodeRef);
		}
	}
	return status;
}


status_t
ImageEditor::Reload()
{
	// Save current zoom and position
	float oldScale = fScale;
	BPoint oldOffset = fOffset;

	delete fBitmap;
	fBitmap = nullptr;

	status_t status = _LoadImage();
	if (status == B_OK) {
		// Restore zoom and position
		fScale = oldScale;
		_UpdateBitmapPosition();
		fOffset = oldOffset;
		Invalidate();
	}

	return status;
}


status_t
ImageEditor::StartMonitoring()
{
	return watch_node(&fNodeRef, B_WATCH_STAT, fTarget);
}


status_t
ImageEditor::StopMonitoring()
{
	return watch_node(&fNodeRef, B_STOP_WATCHING, fTarget);
}


void
ImageEditor::Draw(BRect updateRect)
{
	if (fBitmap == nullptr) {
		// Show error message centered
		const char* message = B_TRANSLATE("Unable to load image");
		font_height fh;
		GetFontHeight(&fh);
		float height = fh.ascent + fh.descent + fh.leading;
		float width = StringWidth(message);

		BRect bounds = Bounds();
		BPoint center(bounds.Width() / 2.0f, bounds.Height() / 2.0f);

		SetHighColor(ui_color(B_PANEL_TEXT_COLOR));
		DrawString(message, BPoint(center.x - width / 2.0f, center.y + height / 2.0f));
		return;
	}

	// Draw the bitmap with zoom and pan
	SetDrawingMode(B_OP_ALPHA);
	SetBlendingMode(B_PIXEL_ALPHA, B_ALPHA_OVERLAY);

	BRect destRect = fBitmapRect;
	destRect.OffsetBy(fOffset);

	DrawBitmap(fBitmap, fBitmap->Bounds(), destRect);
}


void
ImageEditor::FrameResized(float width, float height)
{
	BView::FrameResized(width, height);
	_UpdateBitmapPosition();
	Invalidate();
}


void
ImageEditor::MessageReceived(BMessage* message)
{
	switch (message->what) {
		case B_NODE_MONITOR:
		{
			int32 opcode;
			if (message->FindInt32("opcode", &opcode) == B_OK) {
				if (opcode == B_STAT_CHANGED) {
					// File was modified, reload
					Reload();
				}
			}
			break;
		}
		case B_MOUSE_WHEEL_CHANGED:
		{
			float deltaY;
			if (message->FindFloat("be:wheel_delta_y", &deltaY) == B_OK) {
				BPoint where;
				uint32 buttons;
				GetMouse(&where, &buttons);

				if (deltaY > 0)
					_ZoomOut(where);
				else
					_ZoomIn(where);
			}
			break;
		}
        default:
			BView::MessageReceived(message);
			break;
	}
}


void
ImageEditor::MouseDown(BPoint where)
{
	SetMouseEventMask(B_POINTER_EVENTS, B_NO_POINTER_HISTORY);
	fDragging = true;
	fDragStart = where;
	fOffsetStart = fOffset;
}


void
ImageEditor::MouseUp(BPoint where)
{
	fDragging = false;
}


void
ImageEditor::MouseMoved(BPoint where, uint32 code, const BMessage* dragMessage)
{
	if (fDragging) {
		BPoint delta = where - fDragStart;
		fOffset = fOffsetStart + delta;
		Invalidate();
	}
}


void
ImageEditor::AttachedToWindow()
{
	BView::AttachedToWindow();
	SetEventMask(B_POINTER_EVENTS, B_NO_POINTER_HISTORY);

	// If we have a bitmap but haven't positioned it yet (because view wasn't ready),
	// position it now
	if (fBitmap != nullptr && !fBitmapRect.IsValid()) {
		_ZoomToFit();
		Invalidate();
	}
}


BMessage
ImageEditor::GetModifiedState()
{
	BMessage state;
	state.AddBool("modified", false);
	return state;
}


BMessage
ImageEditor::GetDocumentInfo()
{
	BMessage info;
	info.AddString("name", fFileName);
	info.AddString("path", FilePath());
	info.AddString("type", "image");

	if (fBitmap != nullptr) {
		BRect bounds = fBitmap->Bounds();
		info.AddInt32("width", (int32)(bounds.Width() + 1));
		info.AddInt32("height", (int32)(bounds.Height() + 1));
		info.AddInt32("color_space", (int32)fBitmap->ColorSpace());
	}

	return info;
}


void
ImageEditor::_UpdateBitmapPosition()
{
	if (fBitmap == nullptr)
		return;

	BRect bounds = Bounds();
	BRect bitmapBounds = fBitmap->Bounds();

	float imageWidth = bitmapBounds.Width() + 1;
	float imageHeight = bitmapBounds.Height() + 1;

	float scaledWidth = imageWidth * fScale;
	float scaledHeight = imageHeight * fScale;

	// Center the bitmap (without offset)
	// Handle case where view doesn't have valid bounds yet
	float x = 0;
	float y = 0;
	if (bounds.IsValid() && bounds.Width() >= 1 && bounds.Height() >= 1) {
		x = (bounds.Width() - scaledWidth) / 2.0f;
		y = (bounds.Height() - scaledHeight) / 2.0f;
	}

	fBitmapRect.Set(0, 0, scaledWidth - 1, scaledHeight - 1);
	fBitmapRect.OffsetTo(x, y);
}


void
ImageEditor::_ZoomIn(BPoint center)
{
	if (fBitmap == nullptr)
		return;

	// Calculate zoom around the cursor position
	BPoint imagePoint = center - fBitmapRect.LeftTop() - fOffset;

	float oldScale = fScale;
	fScale *= 1.2f;
	if (fScale > 10.0f)
		fScale = 10.0f;

	_UpdateBitmapPosition();

	// Adjust offset to zoom toward cursor
	float scaleFactor = fScale / oldScale;
	fOffset.x = center.x - fBitmapRect.left - (imagePoint.x * scaleFactor);
	fOffset.y = center.y - fBitmapRect.top - (imagePoint.y * scaleFactor);

	Invalidate();
}


void
ImageEditor::_ZoomOut(BPoint center)
{
	if (fBitmap == nullptr)
		return;

	// Calculate zoom around the cursor position
	BPoint imagePoint = center - fBitmapRect.LeftTop() - fOffset;

	float oldScale = fScale;
	fScale /= 1.2f;
	if (fScale < 0.1f)
		fScale = 0.1f;

	_UpdateBitmapPosition();

	// Adjust offset to zoom toward cursor
	float scaleFactor = fScale / oldScale;
	fOffset.x = center.x - fBitmapRect.left - (imagePoint.x * scaleFactor);
	fOffset.y = center.y - fBitmapRect.top - (imagePoint.y * scaleFactor);

	Invalidate();
}


void
ImageEditor::_ZoomToFit()
{
	if (fBitmap == nullptr)
		return;

	BRect bounds = Bounds();
	BRect bitmapBounds = fBitmap->Bounds();

	// Don't try to fit if we don't have valid bounds yet
	if (!bounds.IsValid() || bounds.Width() < 1 || bounds.Height() < 1) {
		fScale = 1.0f;
		fOffset.Set(0, 0);
		return;
	}

	float imageWidth = bitmapBounds.Width() + 1;
	float imageHeight = bitmapBounds.Height() + 1;
	float viewWidth = bounds.Width();
	float viewHeight = bounds.Height();

	// Calculate scale to fit
	fScale = 1.0f;
	if (imageWidth > viewWidth || imageHeight > viewHeight) {
		float widthScale = viewWidth / imageWidth;
		float heightScale = viewHeight / imageHeight;
		fScale = (widthScale < heightScale) ? widthScale : heightScale;
		fScale *= 0.95f; // Leave some margin
	}

	_UpdateBitmapPosition();
	fOffset.Set(0, 0);
	Invalidate();
}


void
ImageEditor::_ZoomToActual()
{
	if (fBitmap == nullptr)
		return;

	fScale = 1.0f;
	_UpdateBitmapPosition();
	fOffset.Set(0, 0);
	Invalidate();
}


status_t
ImageEditor::_LoadImage()
{
	BFile file(&fFileRef, B_READ_ONLY);
	status_t status = file.InitCheck();
	if (status != B_OK)
		return status;

	// Use BTranslatorRoster to load the image
	BTranslatorRoster* roster = BTranslatorRoster::Default();
	BBitmapStream stream;

	status = roster->Translate(&file, nullptr, nullptr, &stream, B_TRANSLATOR_BITMAP);
	if (status != B_OK)
		return status;

	BBitmap* bitmap = nullptr;
	status = stream.DetachBitmap(&bitmap);
	if (status != B_OK || bitmap == nullptr)
		return B_ERROR;

	// Replace old bitmap
	delete fBitmap;
	fBitmap = bitmap;

	return B_OK;
}
