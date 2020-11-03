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

#include "scanner.h"

void ScanDirectory(const char * dir, int32 * num_files, int32 * totlen);

bool has_crc_init = false;
unsigned long QM_CRC32(unsigned char *block, unsigned int length);
void GenCRC32Table();

typedef std::vector<std::string> dirListType;
dirListType dirList;

int32 hit,miss;

int32 ScanFiles(int32 * num_files, int32 * totlen) {
	//char buf[1024];

	//LockMutexPtr(api->getQueueMutex());

	if (!has_crc_init) {
		GenCRC32Table();
		has_crc_init = true;
	}

	std::stringstream sstr;
	sstr << "CREATE TABLE IF NOT EXISTS Songs (`ID` INTEGER UNSIGNED PRIMARY KEY NOT NULL, `mTime` INTEGER NOT NULL, `Path` varchar(255) NOT NULL, `FN` varchar(255) NOT NULL, `Title` varchar(255) NOT NULL, `Artist` varchar(255) NOT NULL, `Album` varchar(255) NOT NULL, `Genre` varchar(255) NOT NULL, `SongLen` INTEGER NOT NULL, `Seen` INTEGER NOT NULL);";
	char * errmsg = NULL;
	if (Query(sstr.str().c_str(), NULL, NULL, &errmsg) != SQLITE_OK) {
		//wxMessageBox(_("You can not close the window while the scan is in progress!"), _("Error"));
		status_printf("SQL error while creating table: %s\n", errmsg);
		sqlite3_free((void *)errmsg);
		return 0;
	}
	if (errmsg) { sqlite3_free((void *)errmsg); }

	Query("UPDATE Songs SET Seen=0");

	*totlen = 0;
	*num_files = 0;
	//int32 nofiles=0;
	hit = miss = 0;

	dirList.clear();

	for (int i=0; i < NUM_FOLDERS; i++) {
		if (strlen(config.music_folder[i])) { dirList.push_back(config.music_folder[i]); }
	}

	status_printf("Beginning file scan...");

	time_t scan_time = time(NULL);
	while (dirList.size()) {
		dirListType::iterator x = dirList.begin();
		char * path = zstrdup((*x).c_str());
		dirList.erase(x);
		ScanDirectory(path, num_files, totlen);
		zfree(path);
	}
	scan_time = time(NULL) - scan_time;

	status_printf("%d files scanned in " I64FMT " seconds... (%d/%d)", *num_files, scan_time, hit, miss);

	//RelMutexPtr(api->getQueueMutex());
	return *num_files;
}

Titus_Mutex dbMutex;

int Query(const char * query, sqlite3_callback cb, void * parm, char ** errmsg) {
	int ret = 0, tries = 0;
	dbMutex.Lock();
	char ** errmsg2 = errmsg;
	if (errmsg == NULL) {
		static char * errmsg3 = NULL;
		errmsg2 = &errmsg3;
	}
	while ((ret = sqlite3_exec(config.dbHandle, query, cb, parm, errmsg2)) == SQLITE_BUSY && tries < 5) { tries++; }
	if (ret != SQLITE_OK) {
		debug_printf("Query: %s -> %d (%s)\n", query, ret, *errmsg2);
	}
	if (errmsg == NULL && *errmsg2) {
		sqlite3_free(*errmsg2);
	}
	dbMutex.Release();
	return ret;
}

int loadFileRec(void * ptr, int nCols, char ** vals, char ** fields) {
	FILEREC * ret = (FILEREC *)ptr;

	int num = 0;
	char ** f = fields;
	char ** v = vals;

	while (num < nCols) {
		if (!stricmp(*f, "mTime")) {
			ret->mTime = atoi64(*v);
		} else if (!stricmp(*f, "Path")) {
			strcpy(ret->path, *v);
		} else if (!stricmp(*f, "FN")) {
			strcpy(ret->fn, *v);
		} else if (!stricmp(*f, "Artist")) {
			strcpy(ret->artist, *v);
		} else if (!stricmp(*f, "Album")) {
			strcpy(ret->album, *v);
		} else if (!stricmp(*f, "Title")) {
			strcpy(ret->title, *v);
		} else if (!stricmp(*f, "Genre")) {
			strcpy(ret->genre, *v);
		} else if (!stricmp(*f, "SongLen")) {
			ret->songlen = atoi(*v);
		}
		f++;
		v++;
		num++;
	}
	if (ret->mTime == 0) {
		ret->mTime = ret->mTime;
	}
	return 0;
}

FILEREC * GetFileFromDB(uint32 id) {
	FILEREC * ret = znew(FILEREC);
	memset(ret, 0, sizeof(FILEREC));
	std::stringstream str;
	str << "SELECT * FROM Songs WHERE ID=" << id << " LIMIT 1";
	int n;
	if ((n = Query(str.str().c_str(), loadFileRec, ret, NULL)) == SQLITE_OK && ret->mTime) {
		hit++;
		return ret;
	}
	miss++;
	zfree(ret);
	return NULL;
}

uint32 FileID(char * fn) {
#if defined(WIN32)
	char * tmp = strdup(fn);
	strlwr(tmp);
	uint32 ret = QM_CRC32((unsigned char *)tmp,strlen(tmp));
	free(tmp);
	return ret;
#else
	return QM_CRC32((unsigned char *)fn,strlen(fn));
#endif
}

const char * good_exts[] = {
	".mp3",
	".ogg",
	".mpc",
	".flac",
	".cm3",
	".speex",
	NULL
};

bool IsGoodExt(const char * fn) {
	const char * ext = strrchr(fn, '.');
	if (ext) {
		for (int i=0; good_exts[i]; i++) {
			if (!stricmp(ext, good_exts[i])) { return true; }
		}
	}
	return false;
}

void ScanDirectory(const char * path, int32 * num_files, int32 * totlen) {
	char buf[MAX_PATH],buf2[MAX_PATH];

	status_printf("ScanDir: %s", path);

	Directory * dir = new Directory(path);
	//DECODER * dec = NULL;
	while (dir->Read(buf, sizeof(buf), NULL)) {
		sprintf(buf2, "%s%s", path, buf);
		struct stat st;
		if (stat(buf2, &st)) {// error completing stat
			continue;
		}
		if (!S_ISDIR(st.st_mode)) {
			if (IsGoodExt(buf)) {
				uint32 id = FileID(buf2);
				FILEREC * pRec = GetFileFromDB(id);
				if (!pRec || pRec->mTime != st.st_mtime) {
					FILEREC rc;
					memset(&rc, 0, sizeof(rc));
					rc.mTime = st.st_mtime;

					TagLib::FileRef file(buf2);
					if (!file.isNull()) {
						TagLib::AudioProperties * ap = file.audioProperties();
						if (ap) {
							rc.songlen = ap->length();
						}
						TagLib::Tag * tag = file.tag();
						if (tag && !tag->isEmpty()) {
							strcpy(rc.title, tag->title().toCString(true));
							strcpy(rc.album, tag->album().toCString(true));
							strcpy(rc.artist, tag->artist().toCString(true));
							strcpy(rc.genre, tag->genre().toCString(true));
							strtrim(rc.title);
							strtrim(rc.album);
							strtrim(rc.artist);
							strtrim(rc.genre);
						}
					}

					*totlen = *totlen + rc.songlen;

					//save file
					std::stringstream sstr;
					sstr << "INSERT OR REPLACE INTO Songs (ID,Seen,mTime,SongLen,Path,FN,Artist,Album,Title,Genre) VALUES (" << id << ", 1, " << rc.mTime << ", " << rc.songlen;

					strlcpy(rc.path, path, sizeof(rc.path));
					strlcpy(rc.fn, buf, sizeof(rc.fn));
					str_replaceA(rc.path, sizeof(rc.path), "'", "`");
					str_replaceA(rc.fn, sizeof(rc.fn), "'", "`");
					sstr << ", '" << rc.path << "'";
					sstr << ", '" << rc.fn << "'";

					str_replaceA(rc.artist, sizeof(rc.artist), "'", "`");
					str_replaceA(rc.album, sizeof(rc.artist), "'", "`");
					str_replaceA(rc.title, sizeof(rc.artist), "'", "`");
					str_replaceA(rc.genre, sizeof(rc.artist), "'", "`");
					sstr << ", '" << rc.artist << "'";
					sstr << ", '" << rc.album << "'";
					sstr << ", '" << rc.title << "'";
					sstr << ", '" << rc.genre << "');";
					char * errmsg = NULL;
					int n = 0;
					if ((n = Query(sstr.str().c_str(), NULL, NULL, &errmsg)) != SQLITE_OK) {
						status_printf("SQL Error: %d\n", n);
					}
					if (errmsg) {
						status_printf("SQL ErrorStr: %s\n", errmsg);
						sqlite3_free(errmsg);
					}
				} else {
					std::stringstream sstr;
					sstr << "UPDATE Songs SET Seen=1 WHERE ID=" << id;
					Query(sstr.str().c_str());
					*totlen = *totlen + pRec->songlen;
				}
				if (pRec) { zfree(pRec); }
				*num_files = *num_files + 1;
			}
		} else if (buf[0] != '.') {
#ifdef WIN32
			sprintf(buf2,"%s%s\\",path,buf);
#else
			sprintf(buf2,"%s%s/",path,buf);
#endif
			dirList.push_back(buf2);
		}
	}
	delete dir;
}

unsigned long crc_table[256];

/* chksum_crc() -- to a given block, this one calculates the
 *				crc32-checksum until the length is
 *				reached. the crc32-checksum will be
 *				the result.
 */
unsigned long QM_CRC32(unsigned char *block, unsigned int length) {
   register unsigned long crc;
   unsigned long i;

   crc = 0xFFFFFFFF;
   for (i = 0; i < length; i++)
   {
      crc = ((crc >> 8) & 0x00FFFFFF) ^ crc_table[(crc ^ *block++) & 0xFF];
   }
   return (crc ^ 0xFFFFFFFF);
}

void GenCRC32Table() {
   unsigned long crc, poly;
   int i, j;

   poly = 0xEDB88320L;
   for (i = 0; i < 256; i++)
   {
      crc = i;
      for (j = 8; j > 0; j--)
      {
	 if (crc & 1)
	 {
	    crc = (crc >> 1) ^ poly;
	 }
	 else
	 {
	    crc >>= 1;
	 }
      }
      crc_table[i] = crc;
   }
}
