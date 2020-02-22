#include "wallabag_api.h"


void WallabagApi::setConfig(WallabagConfig conf)
{
	this->config = conf;
}


void WallabagApi::createOAuthToken(gui_update_progressbar progressbarUpdater)
{
	auto getUrl = [this] (CURL *curl) -> char * {
		char *url = (char *)calloc(2048, sizeof(char));
		snprintf(url, 2048, "%soauth/v2/token", config.url.c_str());
		return url;
	};

	auto getMethod = [this] (CURL *curl) -> char * {
		return (char *)strdup("POST");
	};

	auto getData = [this] (CURL *curl) -> char * {
		char *postdata = (char *)malloc(2048);

		char *encoded_client_id = curl_easy_escape(curl, this->config.client_id.c_str(), 0);
		char *encoded_secret_key = curl_easy_escape(curl, this->config.secret_key.c_str(), 0);
		char *encoded_login = curl_easy_escape(curl, this->config.login.c_str(), 0);
		char *encoded_password = curl_easy_escape(curl, this->config.password.c_str(), 0);

		snprintf(postdata, 2048, "grant_type=password&client_id=%s&client_secret=%s&username=%s&password=%s",
				encoded_client_id, encoded_secret_key, encoded_login, encoded_password);

		curl_free(encoded_client_id);
		curl_free(encoded_secret_key);
		curl_free(encoded_login);
		curl_free(encoded_password);

		return postdata;
	};

	auto beforeRequest = [progressbarUpdater] (void) -> void {
		progressbarUpdater(LBL_SYNC_OAUTH_CREATE_TOKEN, Gui::SYNC_PROGRESS_PERCENTAGE_OAUTH_START, NULL);
	};

	auto afterRequest = [progressbarUpdater] (void) -> void {
		progressbarUpdater(LBL_SYNC_OAUTH_CREATE_TOKEN, Gui::SYNC_PROGRESS_PERCENTAGE_OAUTH_END, NULL);
	};

	auto onSuccess = [this] (CURLcode res, char *json_string) -> void {
		json_tokener_error error;
		json_object *json_token = json_tokener_parse_verbose(json_string, &error);
		if (json_token == NULL) {
			ERROR("Could not create OAuth token: server returned an invalid JSON string: %s", json_tokener_error_desc(error));
			throw SyncOAuthException(std::string(LBL_SYNC_OAUTH_ERROR_CREATE_TOKEN_INVALID_JSON) + std::string(json_tokener_error_desc(error)));
		}

		const char *access_token = json_object_get_string(json_object_object_get(json_token, "access_token"));
		int expires_in = json_object_get_int(json_object_object_get(json_token, "expires_in"));
		const char *refresh_token = json_object_get_string(json_object_object_get(json_token, "refresh_token"));

		this->oauthToken.access_token = access_token;
		this->oauthToken.refresh_token = refresh_token;
		this->oauthToken.expires_at = time(NULL) + expires_in;
	};

	auto onFailure = [this] (CURLcode res, long response_code, CURL *curl) -> void {
		ERROR("API: createOAuthToken(): failure. HTTP response code = %ld", response_code);

		std::ostringstream ss;
		ss << LBL_SYNC_OAUTH_ERROR_CREATE_TOKEN_STATUS_CODE;
		ss << response_code;
		if (response_code == 401) {
			ss << "\n\n";
			ss << LBL_SYNC_ERROR_HINT_SHOULD_SET_HTTP_BASIC;
		}
		throw SyncOAuthException(ss.str());
	};

	DEBUG("API: createOAuthToken()");

	doHttpRequest(getUrl, getMethod, getData, beforeRequest, afterRequest, onSuccess, onFailure);

	DEBUG("API: createOAuthToken(): done");
}


void WallabagApi::refreshOAuthToken(gui_update_progressbar progressbarUpdater)
{
	if (this->oauthToken.access_token.empty()) {
		// We do not have an oauth token yet => request one!
		createOAuthToken(progressbarUpdater);
		return;
	}

	if (this->oauthToken.expires_at > time(NULL) - 60) {
		// The OAuth token expires in more than 1 minute => no need to refresh it now
		return;
	}

	auto getUrl = [this] (CURL *curl) -> char * {
		char *url = (char *)calloc(2048, sizeof(char));
		snprintf(url, 2048, "%soauth/v2/token", config.url.c_str());
		return url;
	};

	auto getMethod = [this] (CURL *curl) -> char * {
		return (char *)strdup("POST");
	};

	auto getData = [this] (CURL *curl) -> char * {
		char *postdata = (char *)malloc(2048);

		char *encoded_client_id = curl_easy_escape(curl, this->config.client_id.c_str(), 0);
		char *encoded_secret_key = curl_easy_escape(curl, this->config.secret_key.c_str(), 0);
		char *encoded_refresh_token = curl_easy_escape(curl, this->oauthToken.refresh_token.c_str(), 0);

		snprintf(postdata, 2048, "grant_type=refresh_token&client_id=%s&client_secret=%s&refresh_token=%s",
				encoded_client_id, encoded_secret_key, encoded_refresh_token);

		curl_free(encoded_client_id);
		curl_free(encoded_secret_key);
		curl_free(encoded_refresh_token);

		return postdata;
	};

	auto beforeRequest = [progressbarUpdater] (void) -> void {
		progressbarUpdater(LBL_SYNC_OUATH_REFRESH_TOKEN, Gui::SYNC_PROGRESS_PERCENTAGE_OAUTH_START, NULL);
	};

	auto afterRequest = [progressbarUpdater] (void) -> void {
		progressbarUpdater(LBL_SYNC_OUATH_REFRESH_TOKEN, Gui::SYNC_PROGRESS_PERCENTAGE_OAUTH_END, NULL);
	};

	auto onSuccess = [this] (CURLcode res, char *json_string) -> void {
		json_tokener_error error;
		json_object *json_token = json_tokener_parse_verbose(json_string, &error);
		if (json_token == NULL) {
			ERROR("Could not refresh OAuth token: server returned an invalid JSON string: %s", json_tokener_error_desc(error));
			throw SyncOAuthException(std::string(LBL_SYNC_OAUTH_ERROR_REFRESH_TOKEN_INVALID_JSON) + std::string(json_tokener_error_desc(error)));
		}

		const char *access_token = json_object_get_string(json_object_object_get(json_token, "access_token"));
		int expires_in = json_object_get_int(json_object_object_get(json_token, "expires_in"));
		const char *refresh_token = json_object_get_string(json_object_object_get(json_token, "refresh_token"));

		this->oauthToken.access_token = access_token;
		this->oauthToken.refresh_token = refresh_token;
		this->oauthToken.expires_at = time(NULL) + expires_in;
	};

	auto onFailure = [this] (CURLcode res, long response_code, CURL *curl) -> void {
		ERROR("API: refreshOAuthToken(): failure. HTTP response code = %ld", response_code);

		std::ostringstream ss;
		ss << LBL_SYNC_OAUTH_ERROR_REFRESH_TOKEN_STATUS_CODE;
		ss << response_code;
		if (response_code == 401) {
			ss << "\n\n";
			ss << LBL_SYNC_ERROR_HINT_SHOULD_SET_HTTP_BASIC;
		}
		throw SyncOAuthException(ss.str());
	};

	DEBUG("API: refreshOAuthToken()");

	doHttpRequest(getUrl, getMethod, getData, beforeRequest, afterRequest, onSuccess, onFailure);

	DEBUG("API: refreshOAuthToken(): done");
}


void WallabagApi::loadRecentArticles(EntryRepository repository, EpubDownloadQueueRepository epubDownloadQueueRepository, time_t lastSyncTimestamp, gui_update_progressbar progressbarUpdater)
{
	this->refreshOAuthToken(progressbarUpdater);

	// TODO supprimer ça ;-)
	//startBackgroundDownloads(repository, epubDownloadQueueRepository, progressbarUpdater);
	//return;

	bool canDownloadEpub = false;
	if (serverVersion.empty()) {
		fetchServerVersion(progressbarUpdater);

		if (strverscmp(serverVersion.c_str(), "2.2") < 0) {
			DEBUG("Server version (%s) is older than 2.2 => we will not attempt to download EPUB version of entries", serverVersion.c_str());
			canDownloadEpub = false;

			if (config.force_download_epub) {
				DEBUG("WARNING: 'force_download_epub' is set in configuration => WE WILL ATTEMPT TO DOWNLOAD EPUB version of entries anyway!");
				canDownloadEpub = true;
			}
		}
		else {
			DEBUG("Server version (%s) is greater than 2.2 => we will attempt to download EPUB version of entries", serverVersion.c_str());
			canDownloadEpub = true;
		}
	}

	auto getUrl = [this] (CURL *curl) -> char * {
		char *entries_url = (char *)calloc(2048, sizeof(char));

		char *encoded_access_token = curl_easy_escape(curl, this->oauthToken.access_token.c_str(), 0);
		char *encoded_sort = curl_easy_escape(curl, "updated", 0);
		char *encoded_order = curl_easy_escape(curl, "desc", 0);

		snprintf(entries_url, 2048, "%sapi/entries.json?access_token=%s&sort=%s&order=%s&page=%d&perPage=%d&archive=%d",
				config.url.c_str(), encoded_access_token, encoded_sort, encoded_order, 1, PLOP_MAX_NUMBER_OF_ENTRIES_TO_FETCH_FROM_SERVER, 0);

		curl_free(encoded_access_token);
		curl_free(encoded_sort);
		curl_free(encoded_order);

		return entries_url;
	};

	auto getMethod = [this] (CURL *curl) -> char * {
		return (char *)strdup("GET");
	};

	auto getData = [this] (CURL *curl) -> char * {
		return NULL;
	};

	auto beforeRequest = [progressbarUpdater] (void) -> void {
		progressbarUpdater(LBL_SYNC_FETCH_ENTRIES_HTTP_REQUEST, Gui::SYNC_PROGRESS_PERCENTAGE_DOWN_HTTP_START, NULL);
	};

	auto afterRequest = [progressbarUpdater] (void) -> void {
		progressbarUpdater(LBL_SYNC_FETCH_ENTRIES_HTTP_REQUEST, Gui::SYNC_PROGRESS_PERCENTAGE_DOWN_HTTP_END, NULL);
	};

	auto onSuccess = [&] (CURLcode res, char *json_string) -> void {
		DEBUG("API: loadRecentArticles(): response fetched from server");
		progressbarUpdater(LBL_SYNC_SAVE_ENTRIES_TO_LOCAL_DB, Gui::SYNC_PROGRESS_PERCENTAGE_DOWN_SAVE_START, NULL);

		json_tokener_error error;
		json_object *obj = json_tokener_parse_verbose(json_string, &error);
		if (obj == NULL) {
			ERROR("Could not decode entries: server returned an invalid JSON string: %s", json_tokener_error_desc(error));
			throw SyncInvalidJsonException(std::string(LBL_SYNC_ERROR_DECODE_ENTRIES_INVALID_JSON) + std::string(json_tokener_error_desc(error)));
		}

		array_list *items = json_object_get_array(json_object_object_get(json_object_object_get(obj, "_embedded"), "items"));
		int numberOfEntries = items->length;
		DEBUG("API: loadRecentArticles(): number of entries fetched from server: %d", numberOfEntries);

		float percentage = (float)Gui::SYNC_PROGRESS_PERCENTAGE_DOWN_SAVE_START;
		float incrementPercentageEvery = (float)numberOfEntries / (float)(Gui::SYNC_PROGRESS_PERCENTAGE_DOWN_SAVE_END - Gui::SYNC_PROGRESS_PERCENTAGE_DOWN_SAVE_START);
		float nextIncrement = incrementPercentageEvery;

		DEBUG("API: loadRecentArticles(): saving entries to local DB");
		for (int i=0 ; i<numberOfEntries ; i++) {
			json_object *item = (json_object *)array_list_get_idx(items, i);

			Entry remoteEntry = this->entitiesFactory.createEntryFromJson(item);

			if (lastSyncTimestamp != 0) {
				time_t updated_at_ts = remoteEntry.remote_updated_at;
				if (lastSyncTimestamp > updated_at_ts) {
					// Remote updated_at if older than last sync => the entry has not been modified on the server
					// since we last fetched it => no need to re-save it locally
					continue;
				}
			}

			int remoteId = json_object_get_int(json_object_object_get(item, "id"));
			Entry localEntry = repository.findByRemoteId(remoteId);
			if (localEntry.id > 0) {
				// Entry already exists in local DB => we must merge the remote data with the local data
				// and save an updated version of the entry in local DB
				Entry entry = this->entitiesFactory.mergeLocalAndRemoteEntries(localEntry, remoteEntry);
				if (entry._isChanged) {
					DEBUG("API: loadRecentArticles(): updating entry local_id=%d remote_id=%s", entry.id, entry.remote_id.c_str());
					repository.persist(entry);
				}
			}
			else {
				// Entry does not already exist in local DB => just create it
				Entry entry = remoteEntry;
				DEBUG("API: loadRecentArticles(): creating entry for remote_id=%s", entry.remote_id.c_str());
				repository.persist(entry);

				// Download the EPUB for this entry -- as a first step, we can start by only downloading it when creating the local entry (and not when updating it)
				if (
					canDownloadEpub
					&& (!entry.local_is_archived || entry.local_is_starred)
					&& !entry.is_empty
				) {
					entry = repository.findByRemoteId(atoi(entry.remote_id.c_str()));
					enqueueEpubDownload(repository, entry, epubDownloadQueueRepository, progressbarUpdater, percentage);
				}

				// For now, thumbnail is only downloaded when the entry is fetched from the server for the first time
				downloadImage(repository, entry);
			}

			if (i >= nextIncrement) {
				nextIncrement += incrementPercentageEvery;
				percentage += 1;
				progressbarUpdater(LBL_SYNC_SAVE_ENTRIES_TO_LOCAL_DB, percentage, NULL);
			}
		}

		progressbarUpdater(LBL_SYNC_SAVE_ENTRIES_TO_LOCAL_DB, Gui::SYNC_PROGRESS_PERCENTAGE_DOWN_SAVE_END, NULL);
	};

	auto onFailure = [this] (CURLcode res, long response_code, CURL *curl) -> void {
		ERROR("API: loadRecentArticles(): failure. HTTP response code = %ld", response_code);

		std::ostringstream ss;
		ss << LBL_SYNC_FETCH_ENTRIES_ERROR_STATUS_CODE;
		ss << response_code;
		if (response_code == 401) {
			ss << "\n\n";
			ss << LBL_SYNC_ERROR_HINT_SHOULD_SET_HTTP_BASIC;
		}
		throw SyncHttpException(ss.str());
	};

	DEBUG("API: loadRecentArticles()");

	doHttpRequest(getUrl, getMethod, getData, beforeRequest, afterRequest, onSuccess, onFailure);

	DEBUG("API: loadRecentArticles(): done");

	startBackgroundDownloads(repository, epubDownloadQueueRepository, progressbarUpdater);
}


void WallabagApi::enqueueEpubDownload(EntryRepository &repository, Entry &entry, EpubDownloadQueueRepository &epubDownloadQueueRepository, gui_update_progressbar progressbarUpdater, int percent)
{
	DEBUG("API: enqueueEpubDownload(): Enqueuing EPUB download for entry %d / %s", entry.id, entry.remote_id.c_str());

	epubDownloadQueueRepository.enqueueDownloadForEntry(entry);

	DEBUG("API: enqueueEpubDownload(): Enqueuing EPUB download for entry %d / %s: done", entry.id, entry.remote_id.c_str());
}



static EntryRepository *entryRepository;
static EpubDownloadQueueRepository *_epubDownloadQueueRepository;
static WallabagApi *api;

static gui_update_progressbar *_progressbarUpdater;

static pthread_mutex_t mutex_download_progress;
static int count_total_downloads;
static int count_remaining_downloads;

static void do_download_epub_from_queue(void *data)
{
	int entry_id = *((int *)data);
	free(data);

	DEBUG("[background] -> Downloading entry %d on thread %u", entry_id, (int)pthread_self());


	Entry entry = entryRepository->get(entry_id);

	// TODO use a specific method for background download, without progressbar and all!
	api->downloadEpub(*entryRepository, entry, NULL, -1);


	DEBUG("[background] -> Marking entry %d as downloaded", entry_id);
	_epubDownloadQueueRepository->markEntryAsDownloaded(entry_id);


	// TODO if the entry is displayed on the screen, it should be re-drawn (a file is now there and the corresponding flag should be updated)


	float incrementPerDownload = (float)(Gui::SYNC_PROGRESS_PERCENTAGE_DOWN_FILES_END - Gui::SYNC_PROGRESS_PERCENTAGE_DOWN_FILES_START) / (float)count_total_downloads;

	pthread_mutex_lock(&mutex_download_progress);
	count_remaining_downloads -= 1;

	float currentIncrement = incrementPerDownload * ((float)count_total_downloads - (float)count_remaining_downloads);
	float percentage = (float)Gui::SYNC_PROGRESS_PERCENTAGE_DOWN_FILES_START + currentIncrement;

	(*_progressbarUpdater)(LBL_SYNC_DOWNLOAD_EPUB_FILES, percentage, NULL);
	pthread_mutex_unlock(&mutex_download_progress);

	DEBUG("[background] <- Done downloading entry %d on thread %u", entry_id, (int)pthread_self());
}


void WallabagApi::startBackgroundDownloads(EntryRepository &repository, EpubDownloadQueueRepository &epubDownloadQueueRepository, gui_update_progressbar progressbarUpdater)
{
	DEBUG("-> Starting downloading EPUB files in the background");

	int percent = Gui::SYNC_PROGRESS_PERCENTAGE_DOWN_FILES_START;
	progressbarUpdater(LBL_SYNC_DOWNLOAD_EPUB_FILES, percent, NULL);

	// TODO actually download EPUB files in the background + save them + update the corresponding entries ;-)

	DEBUG("Creating thread pool");
	threadpool thpool = thpool_init(4);

	// TODO do not do this... this is so ugly.
	entryRepository = &repository;
	_epubDownloadQueueRepository = &epubDownloadQueueRepository;
	api = this;
	_progressbarUpdater = &progressbarUpdater;

	int *data;

	// List the entry_id of all fetches that must be done
	// and add one job to the thread pool for each of those

	std::vector<int> ids;
	epubDownloadQueueRepository.listEntryIdsToDownload(ids, 100, 0);

	DEBUG("Number of EPUB files to download: %d", ids.size());

	mutex_download_progress = PTHREAD_MUTEX_INITIALIZER;
	count_total_downloads = 0;
	count_remaining_downloads = 0;

	for (unsigned int i=0 ; i<ids.size() ; i++) {
		DEBUG("Adding work to thread pool -> %d", ids.at(i));
		data = (int *)malloc(sizeof(int));
		int entry_id = ids.at(i);
		*data = entry_id;

		DEBUG("Marking entry %d as downloading", entry_id);
		epubDownloadQueueRepository.markEntryAsDownloading(entry_id);

		pthread_mutex_lock(&mutex_download_progress);
		count_total_downloads += 1;
		count_remaining_downloads += 1;
		pthread_mutex_unlock(&mutex_download_progress);

		thpool_add_work(thpool, do_download_epub_from_queue, data);
	}

	DEBUG("Waiting for thread pool to finish working");
	thpool_wait(thpool);

	DEBUG("Destroying thread pool");
	thpool_destroy(thpool);

	percent = Gui::SYNC_PROGRESS_PERCENTAGE_DOWN_FILES_END;
	progressbarUpdater(LBL_SYNC_DOWNLOAD_EPUB_FILES_DONE, percent, NULL);

	pthread_mutex_destroy(&mutex_download_progress);

	DEBUG("<- Done downloading EPUB files in the background");
}


void WallabagApi::downloadEpub(EntryRepository &repository, Entry &entry, gui_update_progressbar progressbarUpdater, int percent)
{
	this->refreshOAuthToken(progressbarUpdater);

	DEBUG("API: downloadEpub(): Downloading EPUB for entry %d / %s", entry.id, entry.remote_id.c_str());

	// In case the storage directory didn't already exist
	iv_mkdir(PLOP_ENTRIES_EPUB_DIRECTORY, 0777);

	char tmp_filepath[1024];
	snprintf(tmp_filepath, sizeof(tmp_filepath), PLOP_ENTRIES_EPUB_DIRECTORY "/tmp-%d.epub", entry.id);
	FILE *tmpDestinationFile = iv_fopen(tmp_filepath, "wb");

	if (percent > -1) {
		char buffer[1024];
		snprintf(buffer, sizeof(buffer), LBL_SYNC_DOWNLOAD_EPUB_FILE_FOR_ENTRY, entry.id, entry.remote_id.c_str());
		progressbarUpdater(buffer, percent, NULL);
	}

	auto getUrl = [this, &entry] (CURL *curl) -> char * {
		char *url = (char *)calloc(2048, sizeof(char));

		char *encoded_access_token = curl_easy_escape(curl, this->oauthToken.access_token.c_str(), 0);
		char *encoded_entry_id = curl_easy_escape(curl, entry.remote_id.c_str(), 0);

		snprintf(url, 2048, "%sapi/entries/%s/export.epub?access_token=%s",
				config.url.c_str(), encoded_entry_id, encoded_access_token);

		curl_free(encoded_access_token);
		curl_free(encoded_entry_id);

		return url;
	};

	auto getMethod = [this] (CURL *curl) -> char * {
		return (char *)strdup("GET");
	};

	auto getData = [this] (CURL *curl) -> char * {
		return NULL;
	};

	auto beforeRequest = [progressbarUpdater] (void) -> void {

	};

	auto afterRequest = [progressbarUpdater] (void) -> void {

	};

	auto onSuccess = [&] (CURLcode res, char *data) -> void {
		DEBUG("API: downloadEpub(): response fetched from server");

		// The temporary file should be complete => close it, se we can work with it.
		iv_fclose(tmpDestinationFile);

		// Let's check if the temporary file seems OK, before actually using it ;-)
		struct stat st;
		int statResult = iv_stat(tmp_filepath, &st);
		if (statResult != 0 || st.st_size == 0) {
			DEBUG("Temporary EPUB file %s for entry %d / %s doesn't seem OK: stat=%d and size=%ld => we cannot use it",
					tmp_filepath, entry.id, entry.remote_id.c_str(), statResult, st.st_size);

			iv_unlink(tmp_filepath);
			return;
		}

		char filepath[1024];
		snprintf(filepath, sizeof(filepath), PLOP_ENTRIES_EPUB_DIRECTORY "/%d.epub", entry.id);

		// move tmp_filepath to filepath
		iv_rename(tmp_filepath, filepath);

		// Update the entry so it references the EPUB file
		DEBUG("Updating entry %d; setting epub path to %s", entry.id, filepath);
		entry.local_content_file_epub = filepath;
		repository.persist(entry);
	};

	auto onFailure = [this, &tmpDestinationFile, tmp_filepath] (CURLcode res, long response_code, CURL *curl) -> void {
		ERROR("API: loadRecentArticles(): failure. HTTP response code = %ld", response_code);

		iv_fclose(tmpDestinationFile);

		// We remove the temporary file, as it's useless and we don't want to pollute the device with temporary files ;-)
		iv_unlink(tmp_filepath);

		// This doesn't abort sync !
	};

	doHttpRequest(getUrl, getMethod, getData, beforeRequest, afterRequest, onSuccess, onFailure, tmpDestinationFile);

	DEBUG("API: downloadEpub(): Downloading EPUB for entry %d / %s: done", entry.id, entry.remote_id.c_str());
}


void WallabagApi::syncEntriesToServer(EntryRepository repository, gui_update_progressbar progressbarUpdater)
{
	this->refreshOAuthToken(progressbarUpdater);

	// Basic idea :
	// For each entry that's been updated more recently on the device than on the server,
	// send updates (archived / starred statuses) to the server

	progressbarUpdater(LBL_SYNC_SEND_UPDATES_TO_SERVER, Gui::SYNC_PROGRESS_PERCENTAGE_UP_START, NULL);


	std::vector<Entry> changedEntries;
	repository.findUpdatedLocallyMoreRecentlyThanRemotely(changedEntries);

	int numberOfEntries = changedEntries.size();
	float percentage = (float)Gui::SYNC_PROGRESS_PERCENTAGE_UP_START;
	float incrementPercentageEvery = (float)numberOfEntries / (float)(Gui::SYNC_PROGRESS_PERCENTAGE_UP_END - Gui::SYNC_PROGRESS_PERCENTAGE_UP_START);
	float nextIncrement = incrementPercentageEvery;

	DEBUG("API: syncEntriesToServer()");

	for (unsigned int i=0 ; i<changedEntries.size() ; i++) {
		Entry entry = changedEntries.at(i);

		DEBUG("Must sync entry l#%d r#%s ; local_updated_at=%d remote_updated_at=%d ; local_is_archived=%d remote_is_archived=%d ; local_is_starred=%d remote_is_starred=%d",
			entry.id, entry.remote_id.c_str(),
			entry.local_updated_at, entry.remote_updated_at,
			entry.local_is_archived, entry.remote_is_archived,
			entry.local_is_starred, entry.remote_is_starred
		);

		syncOneEntryToServer(repository, entry);

		if (i >= nextIncrement) {
			nextIncrement += incrementPercentageEvery;
			percentage += 1;
			progressbarUpdater(LBL_SYNC_SEND_UPDATES_TO_SERVER, percentage, NULL);
		}
	}

	progressbarUpdater(LBL_SYNC_SEND_UPDATES_TO_SERVER, Gui::SYNC_PROGRESS_PERCENTAGE_UP_END, NULL);

	DEBUG("API: syncEntriesToServer(): done");
}


void WallabagApi::syncOneEntryToServer(EntryRepository repository, Entry &entry)
{
	auto getUrl = [this, &entry] (CURL *curl) -> char * {
		char *url = (char *)calloc(2048, sizeof(char));
		char *encoded_access_token = curl_easy_escape(curl, this->oauthToken.access_token.c_str(), 0);
		snprintf(url, 2048, "%sapi/entries/%s.json?access_token=%s",
				config.url.c_str(), entry.remote_id.c_str(), encoded_access_token);
		return url;
	};

	auto getMethod = [this] (CURL *curl) -> char * {
		return (char *)strdup("PATCH");
	};

	auto getData = [this, &entry] (CURL *curl) -> char * {
		char *postdata = (char *)malloc(2048);
		snprintf(postdata, 2048, "archive=%d&starred=%d", entry.local_is_archived, entry.local_is_starred);
		return postdata;
	};

	auto beforeRequest = [] (void) -> void {};

	auto afterRequest = [] (void) -> void {};

	auto onSuccess = [this, &repository, &entry] (CURLcode res, char *json_string) -> void {
		json_tokener_error error;
		json_object *item = json_tokener_parse_verbose(json_string, &error);
		if (item == NULL) {
			ERROR("Could not decode synced entry: server returned an invalid JSON string: %s", json_tokener_error_desc(error));
			throw SyncInvalidJsonException(std::string("Could not decode synced entry: server returned an invalid JSON string: ") + std::string(json_tokener_error_desc(error)));
		}

		Entry remoteEntry = this->entitiesFactory.createEntryFromJson(item);

		DEBUG("Entry l#%d r#%s -> %s", entry.id, entry.remote_id.c_str(), entry.title.c_str());
		DEBUG(" * la=%d->%d ; ra=%d->%d", entry.local_is_archived, remoteEntry.local_is_archived, entry.remote_is_archived, remoteEntry.remote_is_archived);
		DEBUG(" * l*=%d->%d ; r*=%d->%d", entry.local_is_starred, remoteEntry.local_is_starred, entry.remote_is_starred, remoteEntry.remote_is_starred);

		entry = this->entitiesFactory.mergeLocalAndRemoteEntries(entry, remoteEntry);
		repository.persist(entry);
	};

	auto onFailure = [this] (CURLcode res, long response_code, CURL *curl) -> void {
		if (response_code == 404) {
			// Sync TO server occurs after we created an OAuth token and fetched recent entries,
			// which means a 404 error is not likely caused by a wrong URL.
			// Chances are pretty high a 404 here is caused by an entry that's been deleted on the server,
			// but updated locally.
			// => We do not consider this as an error and do not abort sync
			INFO("API: syncOneEntryToServer(): we got a 404 error, but we consider this is OK (probably caused by an error that's been deleted on the server)");

			// TODO flag local entry in some sort of way? At least so we don't try syncing it to the server again and again (and fail each time)

			return;
		}

		ERROR("API: syncOneEntryToServer(): failure. HTTP response code = %ld", response_code);

		std::ostringstream ss;
		ss << LBL_SYNC_SEND_UPDATES_TO_SERVER_ERROR_STATUS_CODE;
		ss << response_code;
		if (response_code == 401) {
			ss << "\n\n";
			ss << LBL_SYNC_ERROR_HINT_SHOULD_SET_HTTP_BASIC;
		}
		throw SyncHttpException(ss.str());
	};

	DEBUG("API: syncOneEntryToServer(%d)", entry.id);

	doHttpRequest(getUrl, getMethod, getData, beforeRequest, afterRequest, onSuccess, onFailure);

	DEBUG("API: syncOneEntryToServer(%d): done", entry.id);
}


void WallabagApi::fetchServerVersion(gui_update_progressbar progressbarUpdater)
{
	auto getUrl = [this] (CURL *curl) -> char * {
		char *url = (char *)calloc(2048, sizeof(char));

		char *encoded_access_token = curl_easy_escape(curl, this->oauthToken.access_token.c_str(), 0);

		snprintf(url, 2048, "%sapi/version.json?access_token=%s", config.url.c_str(), encoded_access_token);

		curl_free(encoded_access_token);

		return url;
	};

	auto getMethod = [this] (CURL *curl) -> char * {
		return (char *)strdup("GET");
	};

	auto getData = [this] (CURL *curl) -> char * {
		return NULL;
	};

	auto beforeRequest = [progressbarUpdater] (void) -> void {
		progressbarUpdater(LBL_SYNC_FETCH_SERVER_VERSION, Gui::SYNC_PROGRESS_PERCENTAGE_FETCH_SERVER_VERSION_START, NULL);
	};

	auto afterRequest = [progressbarUpdater] (void) -> void {
		progressbarUpdater(LBL_SYNC_FETCH_SERVER_VERSION, Gui::SYNC_PROGRESS_PERCENTAGE_FETCH_SERVER_VERSION_END, NULL);
	};

	auto onSuccess = [&] (CURLcode res, char *json_string) -> void {
		json_tokener_error error;
		json_object *obj = json_tokener_parse_verbose(json_string, &error);
		if (obj == NULL) {
			ERROR("Could not decode response: server returned an invalid JSON string: %s", json_tokener_error_desc(error));
			throw SyncInvalidJsonException(std::string("Could not decode response: server returned an invalid JSON string: ") + std::string(json_tokener_error_desc(error)));
		}

		const char *version_string = json_object_get_string(obj);

		DEBUG("API: fetchServerVersion(): response fetched from server -> %s", version_string);
		serverVersion = version_string;
	};

	auto onFailure = [this] (CURLcode res, long response_code, CURL *curl) -> void {
		ERROR("API: fetchServerVersion(): failure. HTTP response code = %ld", response_code);

		std::ostringstream ss;
		ss << LBL_SYNC_FETCH_SERVER_VERSION_ERROR_STATUS_CODE;
		ss << response_code;
		if (response_code == 401) {
			ss << "\n\n";
			ss << LBL_SYNC_ERROR_HINT_SHOULD_SET_HTTP_BASIC;
		}
		throw SyncHttpException(ss.str());
	};


	DEBUG("API: fetchServerVersion()");

	doHttpRequest(getUrl, getMethod, getData, beforeRequest, afterRequest, onSuccess, onFailure);

	DEBUG("API: fetchServerVersion(): done");
}



void WallabagApi::downloadImage(EntryRepository &repository, Entry &entry)
{
	if (entry.preview_picture_url == "") {
		return;
	}
	const char *url = entry.preview_picture_url.c_str();

	// In case the storage directory didn't already exist
	iv_mkdir(PLOP_ENTRIES_IMAGES_DIRECTORY, 0777);

	char tmp_filepath[1024];
	snprintf(tmp_filepath, sizeof(tmp_filepath), PLOP_ENTRIES_IMAGES_DIRECTORY "/tmp-preview-image-%d.img", entry.id);
	FILE *tmpDestinationFile = iv_fopen(tmp_filepath, "wb");

	auto getUrl = [this, &entry] (CURL *curl) -> char * {
		return (char *)strdup(entry.preview_picture_url.c_str());
	};

	auto getMethod = [this] (CURL *curl) -> char * {
		return (char *)strdup("GET");
	};

	auto getData = [this] (CURL *curl) -> char * {
		return NULL;
	};

	auto beforeRequest = [] (void) -> void {};

	auto afterRequest = [] (void) -> void {};

	auto onSuccess = [&] (CURLcode res, char *data) -> void {
		DEBUG("API: downloadImage(): response fetched from server");

		// The temporary file should be complete => close it, se we can work with it.
		iv_fclose(tmpDestinationFile);

		// Let's check if the temporary file seems OK, before actually using it ;-)
		struct stat st;
		int statResult = iv_stat(tmp_filepath, &st);
		if (statResult != 0 || st.st_size <= 0) {
			DEBUG("Temporary IMG file %s for entry %d / %s doesn't seem OK: stat=%d and size=%ld => we cannot use it",
					tmp_filepath, entry.id, entry.remote_id.c_str(), statResult, st.st_size);
			iv_unlink(tmp_filepath);
			return;
		}

		char filepath[1024];

		int thumbnailWidth = PLOP_THUMBNAIL_MAX_WIDTH;
		int thumbnailHeight = GuiListItemEntry::getHeight() - 4;
		ibitmap *img = NULL;
		if (entry.preview_picture_url.compare(entry.preview_picture_url.size() - 4, 4, ".png") == 0) {
			DEBUG("Loading PNG from %s", tmp_filepath);
			int proportional = 1;
			int dither = 1;
			img = LoadPNGStretch(tmp_filepath, thumbnailWidth, thumbnailHeight, proportional, dither);
		}
		else if (
			entry.preview_picture_url.compare(entry.preview_picture_url.size() - 5, 5, ".jpeg") == 0
			|| entry.preview_picture_url.compare(entry.preview_picture_url.size() - 4, 4, ".jpg") == 0
		) {
			DEBUG("Loading JPG from %s", tmp_filepath);
			int br = 100; // background color ? 100 seems to be white
			int co = 100; // compression qualitu ? 100 seems to be high quality, while 1 is low quality
			int proportional = 1;
			img = LoadJPEG(tmp_filepath, thumbnailWidth, thumbnailHeight, br, co, proportional);
		} else {
			// We can only deal with JPEG/PNG images... so nothing we can do, here
		}

		if (img != NULL) {
			snprintf(filepath, sizeof(filepath), PLOP_ENTRIES_IMAGES_DIRECTORY "/preview-image-%d.png", entry.id);

			DEBUG("Saving thumbnail to %s ; width=%d height=%d depth=%d scanline=%d", filepath, img->width, img->height, img->depth, img->scanline);
			int saveResult = SavePNG(filepath, img);
			free(img);

			// Update the entry so it references the image file
			DEBUG("Updating entry %d; setting preview image path to %s from %s", entry.id, filepath, entry.preview_picture_url.c_str());
			entry.preview_picture_path = filepath;
			entry.preview_picture_type = 1;
			repository.persist(entry);
		}

		// Delete the full-size downloaded image: we only need the thumbnail we just generated
		iv_unlink(tmp_filepath);

	};

	auto onFailure = [this, &tmpDestinationFile, tmp_filepath] (CURLcode res, long response_code, CURL *curl) -> void {
		ERROR("API: downloadImage(): failure. HTTP response code = %ld", response_code);

		iv_fclose(tmpDestinationFile);

		// We remove the temporary file, as it's useless and we don't want to pollute the device with temporary files ;-)
		iv_unlink(tmp_filepath);

		// This doesn't abort sync !
	};

	doHttpRequest(getUrl, getMethod, getData, beforeRequest, afterRequest, onSuccess, onFailure, tmpDestinationFile);

	DEBUG("API: downloadImage(): Downloading IMG for entry %d / %s: done", entry.id, entry.remote_id.c_str());
}


CURLcode WallabagApi::doHttpRequest(
	std::function<char * (CURL *curl)> getUrl,
	std::function<char * (CURL *curl)> getMethod,
	std::function<char * (CURL *curl)> getData,
	std::function<void (void)> beforeRequest,
	std::function<void (void)> afterRequest,
	std::function<void (CURLcode res, char *json_string)> onSuccess,
	std::function<void (CURLcode res, long response_code, CURL *curl)> onFailure,
	FILE *destinationFile
)
{
	CURL *curl;
	CURLcode res = CURLE_OK;

	curl = curl_easy_init();
	if (curl) {
		if (destinationFile == NULL) {
			this->json_string_len = 0;
			this->json_string = (char *)calloc(1, 1);
		}

		char *url = getUrl(curl);
		char *method = getMethod(curl);
		char *data = getData(curl);

		curl_easy_setopt(curl, CURLOPT_CUSTOMREQUEST, method);

		curl_easy_setopt(curl, CURLOPT_URL, url);

		curl_easy_setopt(curl, CURLOPT_USERAGENT, "libcurl-agent/1.0");

		curl_easy_setopt(curl, CURLOPT_FOLLOWLOCATION, 1L);
		curl_easy_setopt(curl, CURLOPT_NOPROGRESS, 1L);

		if (data != NULL) {
			curl_easy_setopt(curl, CURLOPT_POSTFIELDS, data);
			curl_easy_setopt(curl, CURLOPT_POSTFIELDSIZE, (long)strlen(data));
		}

		if (!this->config.http_login.empty() && !this->config.http_password.empty()) {
			curl_easy_setopt(curl, CURLOPT_HTTPAUTH, (long)CURLAUTH_BASIC);
			curl_easy_setopt(curl, CURLOPT_USERNAME, this->config.http_login.c_str());
			curl_easy_setopt(curl, CURLOPT_PASSWORD, this->config.http_password.c_str());
		}

		if (destinationFile == NULL) {
			curl_easy_setopt(curl, CURLOPT_WRITEFUNCTION, WallabagApi::_curlWriteCallback);
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, this);
		}
		else {
			curl_easy_setopt(curl, CURLOPT_WRITEDATA, destinationFile);
		}

		beforeRequest();

		res = curl_easy_perform(curl);

		afterRequest();

		free(url);
		free(method);
		if (data != NULL) {
			free(data);
		}

		long response_code;
		curl_easy_getinfo(curl, CURLINFO_RESPONSE_CODE, &response_code);
		try {
			if (res != CURLE_OK || response_code != 200) {
				onFailure(res, response_code, curl);
			}
			else {
				onSuccess(res, json_string);
			}

			if (destinationFile == NULL) {
				free(this->json_string);
			}
			curl_easy_cleanup(curl);
		}
		catch (std::exception &e) {
			if (destinationFile == NULL) {
				free(this->json_string);
			}
			curl_easy_cleanup(curl);

			throw;
		}
	}

	return res;
}


size_t WallabagApi::_curlWriteCallback(char *ptr, size_t size, size_t nmemb, void *userdata)
{
	WallabagApi *that = (WallabagApi *)userdata;

	size_t data_size = size * nmemb;

	that->json_string = (char *)realloc(that->json_string, that->json_string_len + data_size + 1);
	if (that->json_string == NULL) {
		// TODO error-handling
	}

	memcpy(that->json_string + that->json_string_len, ptr, data_size);
	that->json_string_len += data_size;
	that->json_string[that->json_string_len] = '\0';

	return data_size;
}
