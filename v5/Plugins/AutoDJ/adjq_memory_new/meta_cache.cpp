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


//META_CACHE *mFirst=NULL, *mLast=NULL;
//bool mChanged=false;

extern AD_PLUGIN_API * api;

typedef std::map<unsigned long, META_CACHE *> metaCacheList;

metaCacheList mList;

void FreeMetaCache() {
	if (api->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		//api->botapi->ib_printf("AutoDJ -> QueueMutex should be locked when calling FreeMetaCache!\n");
	}

	metaCacheList::iterator x = mList.begin();
	while (x != mList.end()) {
		api->free(x->second);
		mList.erase(x);
		x = mList.begin();
	}
	/*
	META_CACHE * Scan = mFirst;
	META_CACHE * toDel = NULL;
	while (Scan) {
		toDel = Scan;
		Scan = Scan->Next;
		zfree(toDel);
	}
	mFirst = NULL;
	mLast  = NULL;
	*/
}

/*
META_CACHE * AddMetaEntry(META_CACHE * mc) {
	META_CACHE * Scan = (META_CACHE *)zmalloc(sizeof(META_CACHE));
	memcpy(Scan, mc, sizeof(META_CACHE));
	Scan->Next = NULL;

	if (mFirst) {
		mLast->Next = Scan;
		Scan->Prev = mLast;
		mLast = Scan;
	} else {
		Scan->Prev = NULL;
		mFirst = Scan;
		mLast = Scan;
	}

	return Scan;
}
*/

int loadmc_callback(void * ptr, int ncols, char ** vals, char ** cols) {
	META_CACHE * Scan = (META_CACHE *)api->malloc(sizeof(META_CACHE));
	memset(Scan, 0, sizeof(META_CACHE));

	for (int i=0; i < ncols; i++) {
		if (!stricmp(cols[i], "FileID")) {
			Scan->FileID = strtoul(vals[i], NULL, 10);
		}
		if (!stricmp(cols[i], "mTime")) {
			Scan->mTime = vals[i] ? atoi64(vals[i]):0;
		}
		if (!stricmp(cols[i], "SongLen")) {
			Scan->songLen = vals[i] ? atoi(vals[i]):0;
		}
		if (!stricmp(cols[i], "Artist")) {
			strlcpy(Scan->meta.artist, vals[i], sizeof(Scan->meta.artist));
			DecodeString(Scan->meta.artist, sizeof(Scan->meta.artist));
		}
		if (!stricmp(cols[i], "Album")) {
			strlcpy(Scan->meta.album, vals[i], sizeof(Scan->meta.album));
			DecodeString(Scan->meta.album, sizeof(Scan->meta.album));
		}
		if (!stricmp(cols[i], "Title")) {
			strlcpy(Scan->meta.title, vals[i], sizeof(Scan->meta.title));
			DecodeString(Scan->meta.title, sizeof(Scan->meta.title));
		}
		if (!stricmp(cols[i], "Genre")) {
			strlcpy(Scan->meta.genre, vals[i], sizeof(Scan->meta.genre));
			DecodeString(Scan->meta.genre, sizeof(Scan->meta.genre));
		}
	}

	mList[Scan->FileID] = Scan;

	/*
	if (mFirst) {
		mLast->Next = Scan;
		Scan->Prev = mLast;
		mLast = Scan;
	} else {
		Scan->Prev = NULL;
		mFirst = Scan;
		mLast = Scan;
	}
	*/

	return 0;
}

void LoadMetaCache() {
	FreeMetaCache();

	if (api->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		api->botapi->ib_printf(_("AutoDJ -> QueueMutex should be locked when calling LoadMetaCache!\n"));
	}

	//META_CACHE mc;

	char buf[4096],tmp[4096];
	sstrcpy(buf, "CREATE TABLE IF NOT EXISTS autodj_cache(FileID INTEGER PRIMARY KEY, mTime INTEGER, SongLen INTEGER, Title VARCHAR(255), Artist VARCHAR(255), Album VARCHAR(255), Genre VARCHAR(255));");
	api->botapi->db->Query(buf, NULL, NULL, NULL);

	sprintf(buf, "SELECT * FROM autodj_cache");
#if defined(WIN32) && defined(DEBUG)
	uint32 ticks = GetTickCount();
#endif
	api->botapi->db->Query(buf, loadmc_callback, NULL, NULL);
#if defined(WIN32) && defined(DEBUG)
	ticks = GetTickCount() - ticks;
	api->botapi->ib_printf(_("AutoDJ -> Loaded meta DB in %u ticks\n"), ticks);
#endif
}


void EncodeString(char * str, unsigned long bufLen) {
	int maxlen = (strlen(str)*7)+1;
	char * out = (char *)api->malloc(maxlen);
	memset(out, 0, maxlen);

	str_replace(str, bufLen, "&",  "&amp;");
	str_replace(str, bufLen, "\"", "&qout;");
	str_replace(str, bufLen, "<",  "&lt;");
	str_replace(str, bufLen, ">",  "&gt;");
	str_replace(str, bufLen, "'",  "&apos;");

	int len = strlen(str);
//	unsigned char tmp[2];
	char rep[32]={0};
	for (int i=0; i < len; i++) {
		if ((unsigned char)str[i] >= 0x7F || (unsigned char)str[i] < 32) {
			//printf("Str[i]: %d / %u / %c\n", str[i] & 0xff, str[i] & 0xff, str[i] & 0xff);
			sprintf(rep, "&#x%03u;", (unsigned) ( str[i] & 0xff ) );
			//sprintf(rep, "&#x%.2X;", *ptr);
			strcat(out, rep);
			//str_replace(str, bufLen, (char *)&tmp, rep);
		} else {
			strncat(out, &str[i], 1);
		}
	}

	strlcpy(str, out, bufLen);
	api->free(out);
}

void DecodeString(char * str, unsigned long bufLen) {
	char tmp[4];
	char * p;
	memset(tmp, 0, sizeof(tmp));
	while ((p = strstr(str, "&#x"))) {
		memcpy(tmp, p+3, 3);
		*p = atoi(tmp);
		memmove(p+1, p+7, strlen(p+7)+1);
	}

	str_replace(str, bufLen, "&amp;",  "&");
	str_replace(str, bufLen, "&qout;", "\"");
	str_replace(str, bufLen, "&lt;",  "<");
	str_replace(str, bufLen, "&gt;",  ">");
	str_replace(str, bufLen, "&apos;",  "'");
}

void ASCII_Remap(unsigned char * str) {
	for (uint32 i=0; str[i] != 0; i++) {
		if (str[i] == 0x8B) {
			str[i] = '<';
		} else if (str[i] == 0x8E) {
			str[i] = 'Z';
		} else if (str[i] == 0x91 || str[i] == 0x92) {
			str[i] = '\'';
		} else if (str[i] == 0x93 || str[i] == 0x94) {
			str[i] = '\"';
		} else if (str[i] == 0x96 || str[i] == 0x97) {
			str[i] = '-';
		} else if (str[i] == 0x9A) {
			str[i] = 's';
		} else if (str[i] == 0x9B) {
			str[i] = '>';
		} else if (str[i] == 0x9E) {
			str[i] = 'z';
		} else if (str[i] == 0x9F) {
			str[i] = 'y';
		} else if (str[i] == 0xA6) {
			str[i] = '|';
		} else if (str[i] == 0xAA) {
			str[i] = 'a';
		} else if (str[i] == 0xB2) {
			str[i] = '2';
		} else if (str[i] == 0xB3) {
			str[i] = '3';
		} else if (str[i] == 0xB5) {
			str[i] = 'u';
		} else if (str[i] == 0xBF) {
			str[i] = '?';
		} else if (str[i] >= 0xC0 && str[i] <= 0xC6) {
			str[i] = 'A';
		} else if (str[i] == 0xC7) {
			str[i] = 'C';
		} else if (str[i] >= 0xC8 && str[i] <= 0xCB) {
			str[i] = 'E';
		} else if (str[i] >= 0xCC && str[i] <= 0xCF) {
			str[i] = 'I';
		} else if (str[i] == 0xD0) {
			str[i] = 'D';
		} else if (str[i] == 0xD1) {
			str[i] = 'N';
		} else if (str[i] >= 0xD2 && str[i] <= 0xD6) {
			str[i] = 'O';
		} else if (str[i] == 0xD7) {
			str[i] = 'X';
		} else if (str[i] == 0xD8) {
			str[i] = 'O';
		} else if (str[i] >= 0xD9 && str[i] <= 0xDC) {
			str[i] = 'U';
		} else if (str[i] == 0xDD) {
			str[i] = 'Y';
		} else if (str[i] == 0xDE) {
			str[i] = 'P';
		} else if (str[i] == 0xDF) {
			str[i] = 'B';
		} else if (str[i] >= 0xE0 && str[i] <= 0xE6) {
			str[i] = 'a';
		} else if (str[i] == 0xE7) {
			str[i] = 'c';
		} else if (str[i] >= 0xE8 && str[i] <= 0xEB) {
			str[i] = 'e';
		} else if (str[i] >= 0xEC && str[i] <= 0xEF) {
			str[i] = 'i';
		} else if (str[i] == 0xF0) {
			str[i] = 'o';
		} else if (str[i] == 0xF1) {
			str[i] = 'n';
		} else if (str[i] >= 0xF2 && str[i] <= 0xF6) {
			str[i] = 'o';
		} else if (str[i] == 0xF8) {
			str[i] = 'o';
		} else if (str[i] >= 0xF9 && str[i] <= 0xFC) {
			str[i] = 'u';
		} else if (str[i] == 0xFD) {
			str[i] = 'y';
		} else if (str[i] == 0xFE) {
			str[i] = 'p';
		} else if (str[i] > 0x7F) {
			memmove(&str[i], &str[i]+1, strlen((char *)&str[i]));
		}
	}
}

void SaveMetaCache() {
	if (api->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		//api->botapi->ib_printf("AutoDJ -> QueueMutex should be locked when calling SaveMetaCache!\n");
	}

	char buf[512];
	uint32 deleted=0;

	/*
	for (metaCacheList::const_iterator x = mList.begin(); x != mList.end(); x++) {
		if (!x->second->used) {
			sprintf(buf, "DELETE FROM autodj_cache WHERE FileID=%u", x->second->FileID);
			api->botapi->db->Query(buf, NULL, NULL, NULL);
			deleted++;
			if (deleted == 1) {
				api->botapi->ib_printf("AutoDJ -> Deleting unused meta entries...\n", deleted);
			}
		}
	}
	*/

	if (deleted) {
		api->botapi->ib_printf(_("AutoDJ -> Deleted %u unused meta entries\n"), deleted);
	}

	/*
	META_CACHE * Scan = mFirst;
	while (Scan) {
		if (!Scan->used) {
			sprintf(buf, "DELETE FROM autodj_cache WHERE FileID=%u", Scan->FileID);
			api->botapi->db->Query(buf, NULL, NULL, NULL);
			deleted++;
		}
		Scan = Scan->Next;
	}
	*/

/*
	if (!mChanged) {
		api->botapi->ib_printf("AutoDJ -> Skipping meta cache save (no changes)\n");
		return;
	}
	//char buf[1024];
	FILE * fp = fopen("autodj.cache", "wb");
#ifdef DEBUG
	FILE * csv = fopen("autodj.csv", "wb");
#endif
	if (fp) {
		fprintf(fp, "<?xml version=\"1.0\" encoding=\"US-ASCII\" ?>\r\n");
		fprintf(fp, "<meta_cache version=\"1.0\">\r\n");
		META_CACHE * Scan = mFirst;
		while (Scan) {
			if (Scan->used) {
				TITLE_DATA tmp;
				memcpy(&tmp, &Scan->meta, sizeof(TITLE_DATA));
				EncodeString(tmp.title, sizeof(tmp.title));
				EncodeString(tmp.artist, sizeof(tmp.artist));
				EncodeString(tmp.album, sizeof(tmp.album));
				EncodeString(tmp.genre, sizeof(tmp.genre));
#ifdef WIN32
#ifdef DEBUG
				if (csv) {
					fprintf(csv, "%u,%I64d,\"%s\",\"%s\",\"%s\",\"%s\"\r\n", Scan->FileID, Scan->mTime, tmp.title, tmp.artist, tmp.album, tmp.genre);
				}
#endif
				fprintf(fp, "\t<file ID=\"%u\" mTime=\"%I64d\" Title=\"%s\" Artist=\"%s\" Album=\"%s\" Genre=\"%s\" />\r\n", Scan->FileID, Scan->mTime, tmp.title, tmp.artist, tmp.album, tmp.genre);
#else
				fprintf(fp, "\t<file ID=\"%u\" mTime=\"%lld\" Title=\"%s\" Artist=\"%s\" Album=\"%s\" Genre=\"%s\" />\r\n", Scan->FileID, Scan->mTime, tmp.title, tmp.artist, tmp.album, tmp.genre);
#endif
			}
			Scan = Scan->Next;
		}
		fprintf(fp, "</meta_cache>\r\n");
		fclose(fp);
	}
#ifdef DEBUG
	if (csv) {
		fclose(csv);
	}
#endif
	*/
}

META_CACHE * FindFileMeta(unsigned long FileID) {
	metaCacheList::const_iterator x = mList.find(FileID);
	if (x != mList.end()) {
		return x->second;
	}
	return NULL;
}

META_CACHE * SetFileMeta(unsigned long FileID, int64 mTime, int32 songLen, TITLE_DATA * meta) {
	//mChanged = true;

	META_CACHE * ret = FindFileMeta(FileID);
	if (ret) {
		ret->songLen = songLen;
		ret->mTime = mTime;
		ret->used = 1;
		memcpy(&ret->meta, meta, sizeof(TITLE_DATA));
	} else {
		META_CACHE * Scan = ret = (META_CACHE *)api->malloc(sizeof(META_CACHE));
		memset(Scan, 0, sizeof(META_CACHE));
		Scan->FileID = FileID;
		Scan->songLen = songLen;
		Scan->mTime = mTime;
		Scan->used = 1;
		memcpy(&Scan->meta, meta, sizeof(TITLE_DATA));
		mList[FileID] = Scan;
	}

	char buf[8192];
	//sprintf(buf, "DELETE FROM autodj_cache WHERE FileID=%u", FileID);
	//api->botapi->db->Query(buf, NULL, NULL, NULL);
	TITLE_DATA tmp;
	memcpy(&tmp, meta, sizeof(TITLE_DATA));
	EncodeString(tmp.title, sizeof(tmp.title));
	EncodeString(tmp.artist, sizeof(tmp.artist));
	EncodeString(tmp.album, sizeof(tmp.album));
	EncodeString(tmp.genre, sizeof(tmp.genre));
	sprintf(buf, "REPLACE INTO autodj_cache (FileID, mTime, SongLen, Artist, Album, Title, Genre) VALUES (%u, "I64FMT", %d, '%s', '%s', '%s', '%s')", FileID, mTime, songLen, tmp.artist, tmp.album, tmp.title, tmp.genre);
	api->botapi->db->Query(buf, NULL, NULL, NULL);

	return ret;
}
