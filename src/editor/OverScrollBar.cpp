#include "OverScrollBar.h"

#include <Catalog.h>
#include <ToolTip.h>

#include <map>

#include "Editor.h"
#include "Log.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Scrollbar Overlay"

const BRect markerLanes[3] = {BRect( 0, 0, 5, 3),
							  BRect( 6, 0,14, 3),
							  BRect(14, 0,20, 3)};

OverScrollBar::OverScrollBar(BRect rect, BMessenger target)
	:
	BView(rect, "over_VSB_", B_FOLLOW_ALL, B_WILL_DRAW | B_TRANSPARENT_BACKGROUND),
	fTarget(target)
{
	fCaretMarker.ratio = -1; //invalid
	fCaretMarker.severity = 100;
	fCaretMarker.message = B_TRANSLATE("Cursor position");

	if (get_scroll_bar_info(&info) != B_OK) {
		LogError("get_scroll_bar_info failed!");
		info.min_knob_size = -1;
	}
}


void
OverScrollBar::SetProblemsData(std::vector<ScrollMarker> markers)
{
	fProblemsMarkers = std::move(markers);
	Invalidate();
}


void
OverScrollBar::SetCursorPosition(float ratio, int32 line)
{
	fCaretMarker.ratio = ratio;
	fCaretMarker.line = line;
	Invalidate();
}


/* virtual */
void
OverScrollBar::MouseDown(BPoint where)
{
	// Snap to nearest marker and ask the editor to navigate there.
	const ScrollMarker* hit = _NearestMarker(fProblemsMarkers, 1, where.y, 6.0f);
	if (hit != nullptr) {
		BMessage msg(EDITOR_MARKER_GOTO);
		msg.AddInt32("line", hit->line);
		fTarget.SendMessage(&msg);
	}
	// Always let the BScrollBar handle the click too.
	if (Parent() != nullptr)
		Parent()->MouseDown(where);
}


/* virtual */
void
OverScrollBar::MouseUp(BPoint where)
{
	if (Parent() != nullptr)
		Parent()->MouseUp(where);
}


/* virtual */
void
OverScrollBar::MouseMoved(BPoint where, uint32 code, const BMessage* dragMessage)
{
	if (Parent() != nullptr)
		Parent()->MouseMoved(where, code, dragMessage);
}


/* virtual */
void
OverScrollBar::KeyDown(const char* bytes, int32 numBytes)
{
	if (Parent() != nullptr)
		Parent()->KeyDown(bytes, numBytes);
}


/* virtual */
void
OverScrollBar::KeyUp(const char* bytes, int32 numBytes)
{
	if (Parent() != nullptr)
		Parent()->KeyUp(bytes, numBytes);
}


/* virtual */
bool
OverScrollBar::GetToolTipAt(BPoint point, BToolTip** tip)
{
	const ScrollMarker* hit = _NearestMarker(fProblemsMarkers, 1, point.y, 6.0f);
	if (hit == nullptr)
		return false;
	*tip = new BTextToolTip(hit->message.c_str());
	return true;
}


/* virtual */
void
OverScrollBar::Draw(BRect /*rect*/)
{
	if (info.min_knob_size < 0 )
		return;

	BRect r = Bounds();

	float startPoint = r.Width() * (_DoubleArrows(r) ? 2 : 1);
	float endPoint   = r.Height() - startPoint;

	float trackHeight = endPoint - startPoint;

	_DrawCaret(r, startPoint, trackHeight);
	_DrawMarkers(fProblemsMarkers, 1, r, startPoint, trackHeight);
}


void
OverScrollBar::_DrawCaret(BRect& r, float startPoint, float trackHeight)
{
	if (fCaretMarker.ratio > -1 ) {
		float y = (float)(startPoint + fCaretMarker.ratio * trackHeight);
		SetHighColor({255, 255, 255, 255});
		FillRect(BRect(r.left + 1, y, r.right - 1, y + 1));
	}
}


void
OverScrollBar::_DrawMarkers(std::vector<ScrollMarker>& markers, uint lane, BRect& r,
							float startPoint,
							float trackHeight)
{
	if (markers.empty() == false) {

		// Cluster markers into pixel rows; keep worst severity per bucket.
		// severity 1 (Error) is "worst", higher numbers are less severe.
		std::map<int, int> buckets; // pixel_y -> worst severity
		for (const auto& m : markers) {
			int y = (int)(startPoint + m.ratio * trackHeight);
			auto it = buckets.find(y);
			if (it == buckets.end() || m.severity < it->second)
				buckets[y] = m.severity;
		}

		for (const auto& kv : buckets) {
			rgb_color color;
			switch (kv.second) {
				case 0:
				case 1:   color = {220,  50,  50, 255}; break; // Error   – red
				case 2:   color = {220, 180,  40, 255}; break; // Warning – yellow
				case 100: color = {255, 255, 255, 255}; break; // White - the caret
				default:  color = { 60, 120, 220, 255}; break; // Blue - Info
			}
			SetHighColor(color);
			float y = (float)kv.first;
			FillRect(BRect(r.left + 1, y, r.right - 1, y + 1));
		}
	}
}


// Returns the marker whose pixel Y is closest to 'y' and within
// 'tolerance' pixels, or nullptr if none qualifies.
const OverScrollBar::ScrollMarker*
OverScrollBar::_NearestMarker(std::vector<ScrollMarker>& markers, uint lane,
							float y, float tolerance) const
{
	// move outside?
	BRect r = Bounds();
	float startPoint = r.Width() * (_DoubleArrows(r) ? 2 : 1);
	float trackHeight = r.Height() - startPoint * 2;

	const ScrollMarker* best = nullptr;
	float bestDist = tolerance + 1.0f;

	if (markers.empty() == false) {
		for (const auto& m : markers) {
			float markerY = startPoint + m.ratio * trackHeight;
			float dist = std::abs(markerY - y);
			if (dist < bestDist) {
				bestDist = dist;
				best = &m;
			}
		}
	}

	// Check the Caret position as well!
	if (fCaretMarker.ratio > 0) {
		float markerY = startPoint + fCaretMarker.ratio * trackHeight;
		float dist = std::abs(markerY - y);
		if (dist < bestDist) {
			best = &fCaretMarker;
		}
	}
	return best;
}


bool
OverScrollBar::_DoubleArrows(const BRect& bounds) const
{
	if (!info.double_arrows)
		return false;

	// From BScrollBar source:
	// if there is not enough room, switch to single arrows even though
	// double arrows is specified
	return bounds.Height() > (bounds.Width() + 1) * 4 + info.min_knob_size * 2;
}
