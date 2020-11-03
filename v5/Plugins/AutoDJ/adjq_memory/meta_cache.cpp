//@AUTOHEADER@BEGIN@
/**********************************************************************\
|                          ShoutIRC RadioBot                           |
|           Copyright 2004-2020 Drift Solutions / Indy Sams            |
|        More information available at https://www.shoutirc.com        |
|                                                                      |
|                    This file is part of RadioBot.                    |
|                                                                      |
|   RadioBot is free software: you can redistribute it and/or modify   |
| it under the terms of the GNU General Public License as published by |
|  the Free Software Foundation, either version 3 of the License, or   |
|                 (at your option) any later version.                  |
|                                                                      |
|     RadioBot is distributed in the hope that it will be useful,      |
|    but WITHOUT ANY WARRANTY; without even the implied warranty of    |
|     MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the     |
|             GNU General Public License for more details.             |
|                                                                      |
|  You should have received a copy of the GNU General Public License   |
|  along with RadioBot. If not, see <https://www.gnu.org/licenses/>.   |
\**********************************************************************/
//@AUTOHEADER@END@

#include "../adj_plugin.h"
#include "meta_cache.h"

#undef zmalloc
#undef zstrdup
#undef zrealloc
#undef znew
#undef zfreenn
#undef zfree

extern AD_PLUGIN_API * adapi;

//
#if defined(WIN32)
#if defined(DEBUG)
#pragma comment(lib, "sqlite3_d.lib")
#else
#pragma comment(lib, "sqlite3.lib")
#endif
#endif

int loadmc_callback(void * ptr, int ncols, char ** vals, char ** cols) {
	MetaCache * mc = (MetaCache *)ptr;

	META_CACHE Scan;// = (META_CACHE *)adapi->malloc(sizeof(META_CACHE));
	memset(&Scan, 0, sizeof(META_CACHE));

	for (int i = 0; i < ncols; i++) {
		if (!stricmp(cols[i], "FileID") && vals[i]) {
			Scan.FileID = strtoul(vals[i], NULL, 10);
		}
		if (!stricmp(cols[i], "mTime") && vals[i]) {
			Scan.mTime = vals[i] ? atoi64(vals[i]) : 0;
		}
		if (!stricmp(cols[i], "SongLen") && vals[i]) {
			Scan.songLen = vals[i] ? atoi(vals[i]) : 0;
		}
		if (!stricmp(cols[i], "Artist") && vals[i]) {
			strlcpy(Scan.meta.artist, vals[i], sizeof(Scan.meta.artist));
			//DecodeString(Scan.meta.artist, sizeof(Scan.meta.artist));
		}
		if (!stricmp(cols[i], "Album") && vals[i]) {
			strlcpy(Scan.meta.album, vals[i], sizeof(Scan.meta.album));
			//DecodeString(Scan.meta.album, sizeof(Scan.meta.album));
		}
		if (!stricmp(cols[i], "Title") && vals[i]) {
			strlcpy(Scan.meta.title, vals[i], sizeof(Scan.meta.title));
			//DecodeString(Scan.meta.title, sizeof(Scan.meta.title));
		}
		if (!stricmp(cols[i], "Genre") && vals[i]) {
			strlcpy(Scan.meta.genre, vals[i], sizeof(Scan.meta.genre));
			//DecodeString(Scan.meta.genre, sizeof(Scan.meta.genre));
		}
		if (!stricmp(cols[i], "URL") && vals[i]) {
			strlcpy(Scan.meta.url, vals[i], sizeof(Scan.meta.url));
			//DecodeString(Scan.meta.url, sizeof(Scan.meta.url));
		}
		if (!stricmp(cols[i], "Comment") && vals[i]) {
			strlcpy(Scan.meta.comment, vals[i], sizeof(Scan.meta.comment));
			//DecodeString(Scan.meta.comment, sizeof(Scan.meta.comment));
		}
		if (!stricmp(cols[i], "TrackNo") && vals[i]) {
			Scan.meta.track_no = atoi(vals[i]);
		}
		if (!stricmp(cols[i], "Year") && vals[i]) {
			Scan.meta.year = atoi(vals[i]);
		}
	}

	mc->mList[Scan.FileID] = Scan;

	return 0;
}


 MetaCache::MetaCache() {
	 AutoMutex(hMutex);

	 hMetaCache = NULL;
	 if (sqlite3_open("autodj.cache", &hMetaCache) != SQLITE_OK) {
		 adapi->botapi->ib_printf(_("AutoDJ -> Error opening AutoDJ Meta Cache!\n"));
		 if (hMetaCache != NULL) {
			 sqlite3_close(hMetaCache);
			 hMetaCache = NULL;
		 }
		 return;
	 }
	 sqlite3_busy_timeout(hMetaCache, 30000);

	 char buf[4096];//,tmp[4096];
	 sstrcpy(buf, "CREATE TABLE IF NOT EXISTS autodj_cache(FileID INTEGER PRIMARY KEY, mTime INTEGER, SongLen INTEGER, Title VARCHAR(255), Artist VARCHAR(255), Album VARCHAR(255), Genre VARCHAR(255), URL VARCHAR(255), Year INTEGER);");
	 sqlite3_exec(hMetaCache, buf, NULL, NULL, NULL);

	 sstrcpy(buf, "ALTER TABLE autodj_cache ADD COLUMN URL VARCHAR(255);");
	 sqlite3_exec(hMetaCache, buf, NULL, NULL, NULL);

	 sstrcpy(buf, "ALTER TABLE autodj_cache ADD COLUMN Comment VARCHAR(255);");
	 sqlite3_exec(hMetaCache, buf, NULL, NULL, NULL);

	 sstrcpy(buf, "ALTER TABLE autodj_cache ADD COLUMN TrackNo INT DEFAULT 0;");
	 sqlite3_exec(hMetaCache, buf, NULL, NULL, NULL);

	 sstrcpy(buf, "ALTER TABLE autodj_cache ADD COLUMN Year INT DEFAULT 0;");
	 sqlite3_exec(hMetaCache, buf, NULL, NULL, NULL);

	 sprintf(buf, "SELECT * FROM autodj_cache");
	 int64 ticks = adapi->botapi->get_cycles();
	 sqlite3_exec(hMetaCache, buf, loadmc_callback, this, NULL);
	 ticks = adapi->botapi->get_cycles() - ticks;
	 adapi->botapi->ib_printf(_("AutoDJ -> Loaded meta DB in " I64FMT " ms\n"), ticks);
 }

 MetaCache::~MetaCache() {
	AutoMutex(hMutex);

	commitMeta();

	//	char buf[512];
	uint32 deleted = 0;

	/*
	for (metaCacheList::const_iterator x = mList.begin(); x != mList.end(); x++) {
	if (!x->second->used) {
	sprintf(buf, "DELETE FROM autodj_cache WHERE FileID=%u", x->second->FileID);
	adapi->botapi->db->Query(buf, NULL, NULL, NULL);
	deleted++;
	if (deleted == 1) {
	adapi->botapi->ib_printf("AutoDJ -> Deleting unused meta entries...\n", deleted);
	}
	}
	}
	*/

	if (deleted) {
		adapi->botapi->ib_printf(_("AutoDJ -> Deleted %u unused meta entries\n"), deleted);
	}

	/*
	META_CACHE * Scan = mFirst;
	while (Scan) {
	if (!Scan->used) {
	sprintf(buf, "DELETE FROM autodj_cache WHERE FileID=%u", Scan->FileID);
	adapi->botapi->db->Query(buf, NULL, NULL, NULL);
	deleted++;
	}
	Scan = Scan->Next;
	}
	*/

	freeMetaCache();
 }

 void MetaCache::freeMetaCache() {
	AutoMutex(hMutex);
	mList.clear();
	queries.clear();
	if (hMetaCache != NULL) {
		sqlite3_close(hMetaCache);
		hMetaCache = NULL;
	}
}

void MetaCache::commitMeta() {
	AutoMutex(hMutex);
	if (queries.size() > 0) {
		if (hMetaCache != NULL) {
			adapi->botapi->ib_printf("AutoDJ -> Committing %u meta cache changes...\n", queries.size());
			sqlite3_exec(hMetaCache, "BEGIN TRANSACTION", NULL, NULL, NULL);
			for (auto x = queries.begin(); x != queries.end(); x++) {
				sqlite3_exec(hMetaCache, x->c_str(), NULL, NULL, NULL);
			}
			sqlite3_exec(hMetaCache, "COMMIT", NULL, NULL, NULL);
			adapi->botapi->ib_printf("AutoDJ -> Done!\n");
		}
		queries.clear();
	}
}

META_CACHE * MetaCache::FindFileMeta(unsigned long FileID) {
	AutoMutex(hMutex);
	metaCacheList::iterator x = mList.find(FileID);
	if (x != mList.end()) {
		return &x->second;
	}
	return NULL;
}

void MetaCache::SetFileMeta(unsigned long FileID, int64 mTime, int32 songLen, TITLE_DATA * meta) {
	AutoMutex(hMutex);
	META_CACHE * ret = FindFileMeta(FileID);
	if (ret) {
		ret->songLen = songLen;
		ret->mTime = mTime;
		ret->used = 1;
		memcpy(&ret->meta, meta, sizeof(TITLE_DATA));
	} else {
		META_CACHE Scan;// = ret = (META_CACHE *)adapi->malloc(sizeof(META_CACHE));
		memset(&Scan, 0, sizeof(META_CACHE));
		Scan.FileID = FileID;
		Scan.songLen = songLen;
		Scan.mTime = mTime;
		Scan.used = 1;
		memcpy(&Scan.meta, meta, sizeof(TITLE_DATA));
		mList[FileID] = Scan;
		//ret = FindFileMeta(FileID);
	}

	//sprintf(buf, "DELETE FROM autodj_cache WHERE FileID=%u", FileID);
	//adapi->botapi->db->Query(buf, NULL, NULL, NULL);
	TITLE_DATA tmp;
	memcpy(&tmp, meta, sizeof(TITLE_DATA));

	char * stmp = sqlite3_mprintf("REPLACE INTO autodj_cache (FileID, mTime, SongLen, Artist, Album, Title, Genre, URL, Comment, TrackNo, Year) VALUES (%u, %lld, '%d', '%q', '%q', '%q', '%q', '%q', '%q', %u, %u)", FileID, mTime, songLen, tmp.artist, tmp.album, tmp.title, tmp.genre, tmp.url, tmp.comment, tmp.track_no, tmp.year);
	queries.push_back(stmp);
	sqlite3_free(stmp);
	if (queries.size() >= 500) {
		commitMeta();
	}
}
