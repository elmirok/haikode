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

			OverScrollBar(BRect rect, BMessenger target);

	void	SetProblemsData(std::vector<ScrollMarker> markers);

	void	SetCursorPosition(float ratio, int32 line);

	void	MouseDown(BPoint where) override;
	void	MouseUp(BPoint where) override;
	void	MouseMoved(BPoint where, uint32 code, const BMessage* dragMessage) override;
	void	KeyDown(const char* bytes, int32 numBytes) override;
	void	KeyUp(const char* bytes, int32 numBytes) override;

	bool	GetToolTipAt(BPoint point, BToolTip** tip) override;

	void	Draw(BRect /*rect*/) override;

private:
	const ScrollMarker* _NearestMarker(float y, float tolerance) const;

	bool	_DoubleArrows(const BRect& bounds) const;

	scroll_bar_info             info;
	BMessenger                  fTarget;
	std::vector<ScrollMarker>  fMarkers;
	ScrollMarker				fCursorPosition;
};
