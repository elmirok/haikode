#pragma once

#include <InterfaceDefs.h>
#include <View.h>


class OverScrollBar : public BView {
	public:
		OverScrollBar(BRect rect) : BView(rect,"over_VSB_",
										 B_FOLLOW_ALL,B_WILL_DRAW /*| B_FRAME_EVENTS*/ | B_TRANSPARENT_BACKGROUND)
										 {
											if (get_scroll_bar_info(&info) != B_OK) {
												debugger("Ouch!");
											}

										 }

	void				MouseDown(BPoint where) override { if (Parent()) Parent()->MouseDown(where); }
	void				MouseUp(BPoint where) override { if (Parent()) Parent()->MouseUp(where); }
	void				MouseMoved(BPoint where, uint32 code,
									const BMessage* dragMessage) override { if (Parent())
										Parent()->MouseMoved(where, code, dragMessage); }
	void				KeyDown(const char* bytes, int32 numBytes) override { if (Parent()) Parent()->KeyDown(bytes, numBytes); }
	void				KeyUp(const char* bytes, int32 numBytes) override { if (Parent()) Parent()->KeyUp(bytes, numBytes); }



	void				Draw(BRect rect) override {
							BRect r = Bounds();
							float startPoint = r.Width() * (_DoubleArrows(r) ? 2: 1);
							float endPoint = r.Height() - startPoint;
							StrokeLine(BPoint(r.left, startPoint), BPoint(r.right, startPoint));
							StrokeLine(BPoint(r.left, endPoint), BPoint(r.right, endPoint));
							float width = endPoint - startPoint;

							float R = 50;
							float l = 100;
							float coef = R/l;

							float redline = coef*width;

							//StrokeLine(BPoint(r.left, redline), BPoint(r.right, redline));
							FillRect(BRect(r.left, redline, r.right, redline + 1));

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
			scroll_bar_info info;
};
