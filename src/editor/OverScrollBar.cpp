#include "OverScrollBar.h"

#include <Catalog.h>
#include <ToolTip.h>

#include <map>

#include "Editor.h"
#include "Log.h"

#undef B_TRANSLATION_CONTEXT
#define B_TRANSLATION_CONTEXT "Scrollbar Overlay"

OverScrollBar::OverScrollBar(BRect rect, BMessenger target)
	:
	BView(rect, "over_VSB_", B_FOLLOW_ALL, B_WILL_DRAW | B_TRANSPARENT_BACKGROUND),
	fTarget(target)
{
	fCursorPosition.ratio = -1; //invalid
	fCursorPosition.severity = 100;
	fCursorPosition.message = B_TRANSLATE("Cursor position");

	if (get_scroll_bar_info(&info) != B_OK) {
		LogError("get_scroll_bar_info failed!");
		info.min_knob_size = -1;
	}
}


void
OverScrollBar::SetProblemsData(std::vector<ProblemMarker> markers)
{
	fMarkers = std::move(markers);
	fMarkers.insert(fMarkers.begin(), fCursorPosition);
	Invalidate();
}


void
OverScrollBar::SetCursorPosition(float ratio, int32 line)
{
	fCursorPosition.ratio = ratio;
	fCursorPosition.line = line;

	if (fMarkers.empty() || fMarkers[0].severity != 100) {
		fMarkers.push_back(fCursorPosition);
	} else {
		fMarkers[0].ratio = ratio;
		fMarkers[0].line  = line;
	}

	Invalidate();
}


/* virtual */
void
OverScrollBar::MouseDown(BPoint where)
{
	// Snap to nearest marker and ask the editor to navigate there.
	const ProblemMarker* hit = _NearestMarker(where.y, 6.0f);
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
	const ProblemMarker* hit = _NearestMarker(point.y, 6.0f);
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

	if (fMarkers.empty() == false) {
		// Cluster markers into pixel rows; keep worst severity per bucket.
		// severity 1 (Error) is "worst", higher numbers are less severe.
		std::map<int, int> buckets; // pixel_y -> worst severity
		for (const auto& m : fMarkers) {
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
				case 100: color = ui_color(B_MENU_ITEM_TEXT_COLOR); break; // Black? - the cursor.
				default:  color = { 60, 120, 220, 255}; break; // Blue - Info
			}
			SetHighColor(color);
			float y = (float)kv.first;
			FillRect(BRect(r.left + 1, y, r.right + 1, y + 1));
		}
	}
}


// Returns the marker whose pixel Y is closest to 'y' and within
// 'tolerance' pixels, or nullptr if none qualifies.
const OverScrollBar::ProblemMarker*
OverScrollBar::_NearestMarker(float y, float tolerance) const
{
	if (fMarkers.empty())
		return nullptr;

	BRect r = Bounds();
	float startPoint = r.Width() * (_DoubleArrows(r) ? 2 : 1);
	float trackHeight = r.Height() - startPoint * 2;

	const ProblemMarker* best = nullptr;
	float bestDist = tolerance + 1.0f;
	for (const auto& m : fMarkers) {
		float markerY = startPoint + m.ratio * trackHeight;
		float dist = std::abs(markerY - y);
		if (dist < bestDist) {
			bestDist = dist;
			best = &m;
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
