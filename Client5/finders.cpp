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

#include "client.h"

#if defined(DEBUG)
#pragma comment(lib, "sqlite3_d.lib")
#else
#pragma comment(lib, "sqlite3.lib")
#endif

void FreeFindResult(FIND_RESULTS * qRes) {
	for (int i=0; i < qRes->num_songs; i++) {
		zfree(qRes->songs[i].fn);
	}
	zfree(qRes);
}
Titus_Mutex dbMutex;

int Query(const char * query, sqlite3_callback cb=NULL, void * parm=NULL, char ** errmsg=NULL);
int Query(const char * query, sqlite3_callback cb, void * parm, char ** errmsg) {
	AutoMutex(dbMutex);
	if (config.music_db.dbHandle == NULL) { return SQLITE_NOTADB; }

	int ret = 0, tries = 0;
	char ** errmsg2 = errmsg;
	if (errmsg == NULL) {
		char * errmsg3 = NULL;
		errmsg2 = &errmsg3;
	}
	while ((ret = sqlite3_exec(config.music_db.dbHandle, query, cb, parm, errmsg2)) == SQLITE_BUSY && tries < 5) { tries++; }
	if (ret != SQLITE_OK && ret != SQLITE_ABORT) {
		char buf[1024];
		snprintf(buf, sizeof(buf), "SQL Query Error: %s -> %d\r\n\r\nEDesc: %s", query, ret, *errmsg2);
		MessageBox(config.mWnd, buf, "SQL Error", MB_ICONERROR);
	}
	if (errmsg == NULL && *errmsg2) {
		sqlite3_free(*errmsg2);
	}
	return ret;
}

int cb_status(void * uptr, int ncols,char ** vals,char ** cols) {
	*((uint32 *)uptr) = atoi(vals[0]);
	return 0;
}

void UpdateMusicDBStatus(HWND hWnd) {
	if (hWnd == NULL) { hWnd = config.tWnd[TAB_FIND]; }
	if (hWnd) {
		AutoMutex(dbMutex);
		char buf[1024];
		if (config.music_db.dbHandle) {
			uint32 num = 0;
			Query("SELECT COUNT(*) AS Count FROM songs", cb_status, &num, NULL);
			snprintf(buf, sizeof(buf), "Files music database: %u", num);
			SetDlgItemText(hWnd, IDC_FIND_DB_STATUS, buf);
		} else {
			snprintf(buf, sizeof(buf), "Music database is not open!");
			SetDlgItemText(hWnd, IDC_FIND_DB_STATUS, buf);
		}
	}
}

void CreateDB() {
	AutoMutex(dbMutex);
	Query("CREATE TABLE IF NOT EXISTS songs (`ID` INT UNIQUE, `Path` VARCHAR(255), `FN` VARCHAR(255));");
}

bool MusicDB_Init() {
	AutoMutex(dbMutex);
	char * fn = GetUserConfigFile("ShoutIRC", "client5_music.db");
	if (sqlite3_open(fn, &config.music_db.dbHandle) == SQLITE_OK) {
		sqlite3_busy_timeout(config.music_db.dbHandle, 5000);
		CreateDB();
		zfree(fn);
		return true;
	}
	char buf[1024];
	snprintf(buf, sizeof(buf), "Error opening music database, @find support will be unavailable!\r\n\r\nError: %s", sqlite3_errmsg(config.music_db.dbHandle));
	MessageBox(NULL, buf, "Error", MB_ICONERROR);
	if (config.music_db.dbHandle) {
		sqlite3_close(config.music_db.dbHandle);
		config.music_db.dbHandle = NULL;
	}
	zfree(fn);
	return false;
}

void MusicDB_Quit() {
	AutoMutex(dbMutex);
	if (config.music_db.dbHandle) {
		sqlite3_close(config.music_db.dbHandle);
		config.music_db.dbHandle = NULL;
	}
}

int cb_find(void * uptr, int ncols,char ** vals,char ** cols) {
	FIND_RESULTS * ret = (FIND_RESULTS *)uptr;
	if (ret->num_songs >= MAX_FIND_RESULTS) {
		ret->have_more = true;
		return 1;
	}
	ret->songs[ret->num_songs].id = atoul(vals[0]);
	ret->songs[ret->num_songs].fn = zstrdup(vals[1]);
	ret->num_songs++;
	return 0;
}

FIND_RESULTS * MusicDB_Find(const char * query, uint32 search_id) {
	AutoMutex(dbMutex);
	FIND_RESULTS * ret = znew(FIND_RESULTS);
	memset(ret, 0, sizeof(FIND_RESULTS));
	ret->free = FreeFindResult;
	char * tmp = zstrdup(query);
	int len = strlen(tmp)+1;
	str_replace(tmp, len, " ", "%");
	str_replace(tmp, len, "*", "%");
	char * sql = sqlite3_mprintf("SELECT ID,FN FROM songs WHERE FN LIKE '%%%q%%'", tmp);
	Query(sql, cb_find, ret);
	sqlite3_free(sql);
	zfree(tmp);
	return ret;
}

#define poly 0xEDB88320
/* On entry, addr=>start of data
             num = length of data
             crc = incoming CRC     */
uint32 crc32(char *addr, uint32 num, uint32 crc)
{
uint32 i;

for (; num>0; num--)              /* Step through bytes in memory */
  {
  crc = crc ^ *addr++;            /* Fetch byte from memory, XOR into CRC */
  for (i=0; i<8; i++)             /* Prepare to rotate 8 bits */
  {
    if (crc & 1)                  /* b0 is set... */
      crc = (crc >> 1) ^ poly;    /* rotate and XOR with ZIP polynomic */
    else                          /* b0 is clear... */
      crc >>= 1;                  /* just rotate */
  /* Some compilers need:
    crc &= 0xFFFFFFFF;
   */
    }                             /* Loop for 8 bits */
  }                               /* Loop until num=0 */
  return(crc);                    /* Return updated CRC */
}

uint32 GenFileID(const char * fn) {
	char * tmp = strdup(fn);
	strlwr(tmp);
	uint32 ret = crc32(tmp, strlen(tmp), 0);
	free(tmp);
	return ret;
}

const char * exts[] = {
	".mp3",
	".mp4",
	".m4a",
	".aac",
	".flac",
	".wav",
	NULL
};
bool want_extension(const char * ext) {
	if (ext == NULL) { return false; }
	for (int i=0; exts[i] != NULL; i++) {
		if (!stricmp(exts[i], ext)) { return true; }
	}
	return false;
}

typedef std::vector<string> dirList;
void ProcDir(const char * sDir, dirList * dirs, uint64 * num) {
	char buf[1024];
	Directory dir(sDir);
	bool is_dir;
	while (!config.shutdown_now && dir.Read(buf, sizeof(buf), &is_dir)) {
		if (buf[0] == '.') { continue; }
		string ffn = sDir;
		ffn += buf;
		if (is_dir) {
			ffn += PATH_SEPS;
			dirs->push_back(ffn);
		} else {
			const char * ext = strrchr(buf, '.');
			if (want_extension(ext)) {
				char * sql = sqlite3_mprintf("INSERT OR IGNORE INTO songs (`ID`,`Path`,`FN`) VALUES (%u, '%q', '%q');", GenFileID(ffn.c_str()), sDir, buf);
				Query(sql);
				sqlite3_free(sql);
				*num = *num + 1;
				if ((*num % 100) == 0) {
					sprintf(buf, "Processed " U64FMT " songs...", *num);
					SetDlgItemText(config.tWnd[TAB_FIND], IDC_FIND_DB_STATUS, buf);
				}
			}
		}
	}
}

TT_DEFINE_THREAD(Folder_Import) {
	TT_THREAD_START
	if (config.music_db.folder[0]) {
		SetDlgItemText(config.tWnd[TAB_FIND], IDC_FIND_DB_STATUS, "Beginning file scan...");
		Query("BEGIN;");
		dirList dirs;
		dirs.push_back(config.music_db.folder);
		uint64 num = 0;
		while (!config.shutdown_now && dirs.size()) {
			dirList::iterator x = dirs.begin();
			string dir = *x;
			dirs.erase(x);
			ProcDir(dir.c_str(), &dirs, &num);
		}
		SetDlgItemText(config.tWnd[TAB_FIND], IDC_FIND_DB_STATUS, "Commiting changes...");
		Query("COMMIT;");
		if ((rand()%100) == 78) {
			SetDlgItemText(config.tWnd[TAB_FIND], IDC_FIND_DB_STATUS, "Operation complete, by your command.");
		} else {
			SetDlgItemText(config.tWnd[TAB_FIND], IDC_FIND_DB_STATUS, "Operation complete!");
		}
	} else {
		SetDlgItemText(config.tWnd[TAB_FIND], IDC_FIND_DB_STATUS, "You do not have a music folder set!");
	}
	TT_THREAD_END
}


void MusicDB_Clear() {
	AutoMutex(dbMutex);
	Query("DROP TABLE songs");
	CreateDB();
}


DB_MySQL conx;
bool sam_init(char * errmsg, size_t errSize) {
	if (conx.Connect(config.sam.mysql_host, config.sam.mysql_user, config.sam.mysql_pass, config.sam.mysql_db, config.sam.mysql_port)) {
		MYSQL_RES * res = conx.Query("SELECT COUNT(*) FROM songlist");
		if (res && conx.NumRows(res) > 0) {
			return true;
		} else {
			strlcpy(errmsg, "Connected to MySQL successfully, but I don't see any songs in your SAM tables.\r\n\r\nDouble-check your database name and try again...", errSize);
		}
		conx.FreeResult(res);
	} else {
		ostringstream str;
		str << "Error connecting to MySQL: " << conx.GetErrorString();
		strlcpy(errmsg, str.str().c_str(), errSize);
	}
	conx.Disconnect();
	return false;
}
void sam_quit() {
	conx.Disconnect();
}

void sam_sync() {
	conx.Ping();
	MYSQL_RES * res = conx.Query("SELECT filename FROM songlist");
	if (res && conx.NumRows(res) > 0) {
		SC_Row row;
		uint32 num=0;
		char buf[64];
		Query("BEGIN;");
		while (!config.shutdown_now && conx.FetchRow(res, row)) {
			char * ffn = dsl_strdup(row.Get("filename").c_str());
			uint32 id = GenFileID(ffn);
			char * p = strrchr(ffn, PATH_SEP);
			if (p) {
				*p = 0;
				p++;
				char * sql = sqlite3_mprintf("INSERT OR IGNORE INTO songs (`ID`,`Path`,`FN`) VALUES (%u, '%q', '%q');", id, ffn, p);
				Query(sql);
				sqlite3_free(sql);
			} else {
				char * sql = sqlite3_mprintf("INSERT OR IGNORE INTO songs (`ID`,`Path`,`FN`) VALUES (%u, '', '%q');", id, ffn);
				Query(sql);
				sqlite3_free(sql);
			}
			dsl_free(ffn);
			num++;
			if ((num%100) == 0) {
				sprintf(buf, "Processed %u songs...", num);
				SetDlgItemText(config.tWnd[TAB_FIND], IDC_FIND_DB_STATUS, buf);
			}
		}
		SetDlgItemText(config.tWnd[TAB_FIND], IDC_FIND_DB_STATUS, "Commiting changes...");
		Query("COMMIT;");
	}
	conx.FreeResult(res);
}

TT_DEFINE_THREAD(SAM_Import) {
	TT_THREAD_START
	char buf[1024];
	if (sam_init(buf, sizeof(buf))) {
		SetDlgItemText(config.tWnd[TAB_FIND], IDC_FIND_DB_STATUS, "Importing/syncing songs from SAM...");
		sam_sync();
		SetDlgItemText(config.tWnd[TAB_FIND], IDC_FIND_DB_STATUS, "Disconnecting from SAM...");
		sam_quit();
		SetDlgItemText(config.tWnd[TAB_FIND], IDC_FIND_DB_STATUS, "Operation complete!");
	} else {
		MessageBox(config.mWnd, buf, "Error", MB_ICONERROR);
	}
	TT_THREAD_END
}
