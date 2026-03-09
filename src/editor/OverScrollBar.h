#pragma once

#include <InterfaceDefs.h>
#include <View.h>

#include <map>
#include <vector>


class OverScrollBar : public BView {
	public:
		// LSP severity: 1=Error, 2=Warning, 3=Information, 4=Hint
		struct ProblemMarker {
			float ratio;    // normalised position in [0.0, 1.0]
			int   severity; // as per LSP DiagnosticSeverity
		};

		OverScrollBar(BRect rect) : BView(rect, "over_VSB_",
										 B_FOLLOW_ALL, B_WILL_DRAW | B_TRANSPARENT_BACKGROUND)
		{
			if (get_scroll_bar_info(&info) != B_OK) {
				debugger("Ouch!");
			}
		}

	void				SetProblemsData(std::vector<ProblemMarker> markers)
						{
							fMarkers = std::move(markers);
							Invalidate();
						}

	void				MouseDown(BPoint where) override { if (Parent()) Parent()->MouseDown(where); }
	void				MouseUp(BPoint where) override { if (Parent()) Parent()->MouseUp(where); }
	void				MouseMoved(BPoint where, uint32 code,
									const BMessage* dragMessage) override { if (Parent())
										Parent()->MouseMoved(where, code, dragMessage); }
	void				KeyDown(const char* bytes, int32 numBytes) override { if (Parent()) Parent()->KeyDown(bytes, numBytes); }
	void				KeyUp(const char* bytes, int32 numBytes) override { if (Parent()) Parent()->KeyUp(bytes, numBytes); }

	void				Draw(BRect /*rect*/) override {

							if (fMarkers.empty())
								return;

							BRect r = Bounds();
							float startPoint = r.Width() * (_DoubleArrows(r) ? 2 : 1);
							float endPoint   = r.Height() - startPoint;



							float trackHeight = endPoint - startPoint;

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
									case 0: //unsupported severity parameter.
									case 1:  color = {220,  50,  50, 255}; break; // Error – red
									case 2:  color = {220, 180,  40, 255}; break; // Warning – yellow
									default: color = { 60, 120, 220, 255}; break; // Info/Hint – blue
								}
								SetHighColor(color);
								float y = (float)kv.first;
								FillRect(BRect(r.left+1, y, r.right-1, y + 2));
							}
						}

	private:

	bool				_DoubleArrows(const BRect& bounds) const
						{
							if (!info.double_arrows)
								return false;

							// From BScrollBar source:
							// if there is not enough room, switch to single arrows even though
							// double arrows is specified
							return bounds.Height() > (bounds.Width() + 1) * 4 + info.min_knob_size * 2;
						}

private:
			scroll_bar_info             info;
			std::vector<ProblemMarker>  fMarkers;
};
