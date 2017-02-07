#ifndef DEFINES_H_
#define DEFINES_H_


#define PLOP_APPLICATION_FULLNAME "Plop! reader"
#define PLOP_APPLICATION_SHORTNAME "Plop!R"

#define PLOP_VERSION_STR "v0.3.1"
#define PLOP_VERSION_NUM 5
#define PLOP_WEBSITE_URL "http://plop-reader.pascal-martin.fr"
#define PLOP_OPENSOURCE_URL "https://github.com/pmartin/plop-reader"

// On a TL3: /mnt/ext1/system/usr/share/plop-reader
#define PLOP_BASE_DIRECTORY USERDATA "/share/plop-reader"
#define PLOP_ENTRIES_CONTENT_DIRECTORY PLOP_BASE_DIRECTORY "/entries"
#define PLOP_ENTRIES_EPUB_DIRECTORY PLOP_BASE_DIRECTORY "/entries-epub"

#define PLOP_MAX_NUMBER_OF_ENTRIES_TO_FETCH_FROM_SERVER 200

#endif /* DEFINES_H_ */
