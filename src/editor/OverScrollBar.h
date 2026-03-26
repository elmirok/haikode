#pragma once

#include <Messenger.h>
#include <View.h>

#include <climits>
#include <string>
#include <vector>

class BToolTip;


class OverScrollBar : public BView {
	public:
		// severity:
		//	1=Error,
		//	2=Warning,
		//	3=Information,
		//	4=Hint (0=Unsupported by LSP server)
		//	100 = Cursor position.

		struct ScrollMarker {
			float       ratio;    // normalised position in [0.0, 1.0]
			int         severity; // as per LSP DiagnosticSeverity
			int32       line;     // 1-based line number for navigation
			std::string message;  // human-readable diagnostic text for tooltip
		};

		struct Lane {
			uint8 index;
			BRect rect;
			std::vector<ScrollMarker> markers;
		};
			OverScrollBar(BRect rect, BMessenger target);

	void	SetProblemsData(std::vector<ScrollMarker> markers);
	void	UpdateSciMarkers(std::vector<ScrollMarker> markers);
	void	UpdateHighlightMarkers(std::vector<ScrollMarker> markers);

	void	SetCursorPosition(float ratio, int32 line);

	void	AttachedToWindow() override;
	void	MouseDown(BPoint where) override;
	void	MouseUp(BPoint where) override;
	void	MouseMoved(BPoint where, uint32 code, const BMessage* dragMessage) override;
	void	KeyDown(const char* bytes, int32 numBytes) override;
	void	KeyUp(const char* bytes, int32 numBytes) override;

	bool	GetToolTipAt(BPoint point, BToolTip** tip) override;

	void	Draw(BRect /*rect*/) override;

private:
	const ScrollMarker* _NearestMarker(const BPoint& point, float tolerance) const;

	bool	_DoubleArrows(const BRect& bounds) const;

	void	_DrawMarkers(Lane& markers, BRect& bounds,
							float startPoint,
							float trackHeight);

	void	_DrawCaret(BRect& bounds, float startPoint, float trackHeight);

	scroll_bar_info             fScrollBarInfo;
	BMessenger                  fTarget;
	Lane 						fLanes[3];
	ScrollMarker				fCaretMarker;
};
