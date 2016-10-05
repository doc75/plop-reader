#ifndef GUI_GUI_LIST_ITEM_ENTRY_H_
#define GUI_GUI_LIST_ITEM_ENTRY_H_


#include "inkview.h"
#include <math.h>


#include "../entities/entry.h"


class GuiListItemEntry
{
public:
	GuiListItemEntry() {
		GuiListItemEntry(NULL, NULL);
	}

	GuiListItemEntry(ifont *ttitleFont, ifont *iinfosFont) {
		_hasEntry = false;

		screenWidth = ScreenWidth();
		screenHeight = ScreenHeight();

		titleFont = ttitleFont;
		infosFont = iinfosFont;

		x = y = 0;
	}

	void setCoordinates(int xx, int yy) {
		x = xx;
		y = yy;
	}

	void draw(bool clearBeforeDraw, bool updateScreen);

	void setEntry(Entry &e) {
		_hasEntry = true;
		entry = e;
	}

	Entry &getEntry() {
		return entry;
	}

	bool hasEntry() {
		return _hasEntry;
	}

	int getHeight();

	bool hit(int xx, int yy) {
		return xx >= x
				&& xx <= x + screenWidth
				&& yy >= y
				&& yy <= y + getHeight()
		;
	}

private:
	bool _hasEntry;
	Entry entry;

	int x, y;

	int screenWidth, screenHeight;

	ifont *titleFont;
	ifont *infosFont;
};


#endif /* GUI_GUI_LIST_ITEM_ENTRY_H_ */