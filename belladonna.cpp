#include "belladonna.h"


ifont *font;
const int kFontSize = 16;


WallabagApi wallabag_api;
Database db;


static int main_handler(int event_type, int param_one, int param_two)
{
	int result = 0;

	static int step = 0;

	switch (event_type) {
	case EVT_INIT:
		font = OpenFont("LiberationSans", kFontSize, 1);
		SetFont(font, BLACK);
		log_reset();
		ClearScreen();
		FullUpdate();

		// TODO instead of dropping + creating the DB, we should detect if its structure is up-to-date
		// and run migrations only if it is not ;-)
		//db.drop();
		db.open();
		db.runMigrations();

		break;
	case EVT_SHOW:

		break;
	case EVT_KEYPRESS:
		if (param_one == KEY_PREV) {
			CloseApp();
			return 1;
		}
		else if (param_one == KEY_NEXT) {


			WallabagConfigLoader configLoader;
			WallabagConfig config = configLoader.load();

			wallabag_api.setConfig(config);
			//wallabag_api.createOAuthToken();

			EntryRepository repository(db);
			//wallabag_api.loadRecentArticles(repository);

			database_display_entries(repository);

			/*
			if (step == 0) {

			}
			else {
				CloseApp();
			}
			//*/

			step++;
			return 1;
		}

		break;
	case EVT_EXIT:
		CloseFont(font);
		break;
	default:
		break;
	}

    return result;
}


int main (int argc, char* argv[])
{
    InkViewMain(main_handler);

    return 0;
}
