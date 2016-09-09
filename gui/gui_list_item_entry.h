#ifndef GUI_GUI_LIST_ITEM_ENTRY_H_
#define GUI_GUI_LIST_ITEM_ENTRY_H_


#include "inkview.h"


#include "../entities/entry.h"


class GuiListItemEntry
{
public:
	int x;
	int y;

	bool hasEntry;
	Entry entry;

	void draw(bool clearBeforeDraw, bool updateScreen);

	void setEntry(Entry &e) {
		entry = e;
	}

	int getHeight();

	bool hit(int xx, int yy) {
		return xx >= x
				&& xx <= x + screenWidth
				&& yy >= y
				&& yy <= y + getHeight()
		;
	}

	ifont *titleFont;
	ifont *infosFont;

	int screenWidth, screenHeight;

private:

};


#endif /* GUI_GUI_LIST_ITEM_ENTRY_H_ */
