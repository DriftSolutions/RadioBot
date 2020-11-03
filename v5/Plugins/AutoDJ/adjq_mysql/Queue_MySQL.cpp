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

#include "Queue_MySQL.h"

AD_PLUGIN_API * adapi=NULL;
QM_CONFIG qm_config;
DB_MySQL conx;

void FreePlayList() {
	LockMutexPtr(adapi->getQueueMutex());
	if (adapi->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		adapi->botapi->ib_printf(_("AutoDJ -> QueueMutex should be locked when calling FreePlayList!\n"));
	}
	RelMutexPtr(adapi->getQueueMutex());
}

QUEUE * GetRowToQueue(MYSQL_RES * res) {
	if (!res) { return NULL; }
	QUEUE * ret = NULL;
	SC_Row row;
	if (conx.FetchRow(res, row)) {
		ret = (QUEUE *)adapi->malloc(sizeof(QUEUE));
		memset(ret, 0, sizeof(QUEUE));
		ret->fn = adapi->strdup(row.Get("FN").c_str());
		ret->path = adapi->strdup(row.Get("Path").c_str());
		ret->ID = atoul(row.Get("ID").c_str());
		ret->mTime = atoi64(row.Get("mTime").c_str());
		ret->lastPlayed = atoi64(row.Get("LastPlayed").c_str());
		ret->req_count = atoi(row.Get("ReqCount").c_str());
		ret->songlen = atoul(row.Get("SongLen").c_str());
		string p = row.Get("Title");
		if (p.length()) {
			ret->meta = (TITLE_DATA *)adapi->malloc(sizeof(TITLE_DATA));
			memset(ret->meta, 0, sizeof(TITLE_DATA));
			strlcpy(ret->meta->title, p.c_str(), sizeof(ret->meta->title));
			strlcpy(ret->meta->artist, row.Get("Artist").c_str(), sizeof(ret->meta->artist));
			strlcpy(ret->meta->album, row.Get("Album").c_str(), sizeof(ret->meta->album));
			strlcpy(ret->meta->album_artist, row.Get("AlbumArtist").c_str(), sizeof(ret->meta->album_artist));
			strlcpy(ret->meta->genre, row.Get("Genre").c_str(), sizeof(ret->meta->genre));
			strlcpy(ret->meta->url, row.Get("URL").c_str(), sizeof(ret->meta->url));
			strlcpy(ret->meta->comment, row.Get("Comment").c_str(), sizeof(ret->meta->comment));
			ret->meta->track_no = atoul(row.Get("TrackNo").c_str());
			ret->meta->year = atoul(row.Get("Year").c_str());
		}
		/*
		ret->fn = adapi->strdup(row.Get("FN"));
		ret->path = adapi->strdup(row.Get("Path"));
		ret->ID = atoul(row.Get("ID"));
		ret->mTime = atoi64(row.Get("mTime"));
		ret->lastPlayed = atoi64(row.Get("LastPlayed"));
		ret->req_count = atoi(row.Get("ReqCount"));
		ret->songlen = atoul(row.Get("SongLen"));
		char * p = row.Get("Title");
		if (p && strlen(p)) {
			ret->meta = (TITLE_DATA *)adapi->malloc(sizeof(TITLE_DATA));
			memset(ret->meta,0,sizeof(TITLE_DATA));
			strlcpy(ret->meta->title, p, sizeof(ret->meta->title));
			strlcpy(ret->meta->artist, row.Get("Artist"), sizeof(ret->meta->artist));
			strlcpy(ret->meta->album, row.Get("Album"), sizeof(ret->meta->album));
			strlcpy(ret->meta->album_artist, row.Get("AlbumArtist"), sizeof(ret->meta->album_artist));
			strlcpy(ret->meta->genre, row.Get("Genre"), sizeof(ret->meta->genre));
			strlcpy(ret->meta->url, row.Get("URL"), sizeof(ret->meta->url));
			strlcpy(ret->meta->comment, row.Get("Comment"), sizeof(ret->meta->comment));
			ret->meta->track_no = atoul(row.Get("TrackNo"));
			ret->meta->year = atoul(row.Get("Year"));
		}
		*/
	}
	return ret;
}

QUEUE * GetQueueFromID(uint32 id) {
	std::stringstream sstr;
	sstr << "SELECT * FROM " << qm_config.mysql_table << " WHERE ID=" << id;
	QUEUE * ret = NULL;
	MYSQL_RES * res = conx.Query(sstr.str().c_str());
	if (res && conx.NumRows(res) > 0) {
		ret = GetRowToQueue(res);
	}
	conx.FreeResult(res);
	return ret;
}

QUEUE_FIND_RESULT * FindWild(const char * pat) {
	LockMutexPtr(adapi->getQueueMutex());

	QUEUE_FIND_RESULT * ret = (QUEUE_FIND_RESULT *)adapi->malloc(sizeof(QUEUE_FIND_RESULT));
	memset(ret,0,sizeof(QUEUE_FIND_RESULT));

	std::stringstream sstr;
	char * pat2 = strdup(conx.EscapeString(pat).c_str());
	str_replace(pat2, strlen(pat2)+1, "*", "%");
	sstr << "SELECT * FROM " << qm_config.mysql_table << " WHERE (FN LIKE '" << pat2 << "' OR Artist LIKE '" << pat2 << "' OR Title LIKE '" << pat2 << "' OR Album LIKE '" << pat2 << "') AND Seen=1";
	free(pat2);
	switch (qm_config.find_sort) {
		case QMFS_RANDOM:
			sstr << " ORDER BY RAND()";
			break;
		case QMFS_FILENAME:
			sstr << " ORDER BY FN ASC";
			break;
		case QMFS_TITLE:
			sstr << " ORDER BY Title ASC";
			break;
		case QMFS_NONE:
			break;
	}
	conx.Ping();
	MYSQL_RES * res = conx.Query(sstr.str().c_str());
	if (res && conx.NumRows(res) > 0) {
		QUEUE * Scan=NULL;
		while ((Scan = GetRowToQueue(res)) != NULL) {
			if (ret->num == MAX_RESULTS) {
				ret->not_enough = true;
				adapi->FreeQueue(Scan);
				break;
			}
			ret->results[ret->num] = Scan;
			ret->num++;
		}
	}
	conx.FreeResult(res);
	RelMutexPtr(adapi->getQueueMutex());
	return ret;
}

QUEUE_FIND_RESULT * FindByMeta(const char * pat, META_TYPES type) {
	LockMutexPtr(adapi->getQueueMutex());
	if (adapi->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		adapi->botapi->ib_printf(_("AutoDJ -> QueueMutex should be locked when calling FindByMeta!\n"));
	}

	QUEUE_FIND_RESULT * ret = (QUEUE_FIND_RESULT *)adapi->malloc(sizeof(QUEUE_FIND_RESULT));
	memset(ret,0,sizeof(QUEUE_FIND_RESULT));

	std::stringstream sstr;
	sstr << "SELECT * FROM " << qm_config.mysql_table << " WHERE (";
	char * field=NULL;
	switch(type) {
		case META_FILENAME:
			field = "FN";
			break;
		case META_DIRECTORY:
			field = "Path";
			break;
		case META_ARTIST:
			field = "Artist";
			break;
		case META_ALBUM:
			field = "Album";
			break;
		case META_GENRE:
			field = "Genre";
			break;
		case META_YEAR:
			field = "Year";
			break;
		case META_REQ_COUNT:
			field = "ReqCount";
			break;
		case META_TITLE:
		default:
			field = "Title";
			break;
	}

	char * pat2 = strdup(pat);
	str_replace(pat2, strlen(pat2)+1, "*", "%");
	int cnt=0;
	char * p2 = NULL;
	char * p = strtok_r(pat2, "|,", &p2);
	while (p) {
		if (cnt) {
			sstr << " OR";
		}
		sstr << " " << field << " LIKE '" << conx.EscapeString(p) << "'";
		p = strtok_r(NULL, "|,", &p2);
		cnt++;
	}
	free(pat2);
	sstr << ") AND Seen=1";
	switch (qm_config.find_sort) {
		case QMFS_RANDOM:
			sstr << " ORDER BY RAND()";
			break;
		case QMFS_FILENAME:
			sstr << " ORDER BY FN ASC";
			break;
		case QMFS_TITLE:
			sstr << " ORDER BY Title ASC";
			break;
		case QMFS_NONE:
			break;
	}
	conx.Ping();
	MYSQL_RES * res = conx.Query(sstr.str().c_str());
	if (res && conx.NumRows(res) > 0) {
		QUEUE * Scan=NULL;
		while ((Scan = GetRowToQueue(res)) != NULL) {
			if (ret->num == MAX_RESULTS) {
				ret->not_enough = true;
				adapi->FreeQueue(Scan);
				break;
			}
			ret->results[ret->num] = Scan;
			ret->num++;
		}
	}
	conx.FreeResult(res);

	RelMutexPtr(adapi->getQueueMutex());
	return ret;
}

void FreeFindResult(QUEUE_FIND_RESULT * qRes) {
	LockMutexPtr(adapi->getQueueMutex());
	if (adapi->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		adapi->botapi->ib_printf(_("AutoDJ -> QueueMutex should be locked when calling FreeFindResult!\n"));
	}
	for (int i=0; i < qRes->num; i++) {
		adapi->FreeQueue(qRes->results[i]);
	}
	adapi->free(qRes);
	RelMutexPtr(adapi->getQueueMutex());
}

QUEUE * FindFile(const char * fn) {
	LockMutexPtr(adapi->getQueueMutex());
	if (adapi->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		adapi->botapi->ib_printf(_("AutoDJ -> QueueMutex should be locked when calling FindFile!\n"));
	}

	QUEUE * ret = NULL;

	std::stringstream sstr;
	char * pat2 = strdup(conx.EscapeString(fn).c_str());
	sstr << "SELECT * FROM " << qm_config.mysql_table << " WHERE FN LIKE '" << pat2 << "' AND Seen=1";
	free(pat2);
	conx.Ping();
	MYSQL_RES * res = conx.Query(sstr.str().c_str());
	if (res && conx.NumRows(res) > 0) {
		ret = GetRowToQueue(res);
	}
	conx.FreeResult(res);
	RelMutexPtr(adapi->getQueueMutex());
	return ret;
}

QUEUE * FindFileByID(uint32 id) {
	LockMutexPtr(adapi->getQueueMutex());

	QUEUE * ret = NULL;

	std::stringstream sstr;
	sstr << "SELECT * FROM " << qm_config.mysql_table << " WHERE ID=" << id << " AND Seen=1";
	conx.Ping();
	MYSQL_RES * res = conx.Query(sstr.str().c_str());
	if (res && conx.NumRows(res) > 0) {
		ret = GetRowToQueue(res);
	}
	conx.FreeResult(res);
	RelMutexPtr(adapi->getQueueMutex());
	return ret;
}

MYSQL_RES * LoadMetaCacheQuery() {
	AutoMutexPtr(adapi->getQueueMutex());
	std::stringstream sstr;
	sstr << "SELECT * FROM " << qm_config.mysql_table;
	conx.Ping();
	return conx.Query(sstr.str().c_str());
}

void ResetChecked() {
	/*
	std::stringstream sstr;
	sstr << "UPDATE " << qm_config.mysql_table << " SET `Checked`=0";
	MYSQL_RES * res = conx.Query(sstr.str().c_str());
	conx.FreeResult(res);
	*/
}

QUEUE * GetRandomFileExec(const char * query, time_t tme, const char * pat) {
	QUEUE * Scan = NULL;
	MYSQL_RES * res = conx.Query(query);
	if (res && conx.NumRows(res) > 0) {
		while ((Scan = GetRowToQueue(res)) != NULL) {
			if (adapi->AllowSongPlayback(Scan, tme, pat)) {
				break;
			} else {
				adapi->FreeQueue(Scan);
			}
		}
	}
	conx.FreeResult(res);
	return Scan;
}

QUEUE * GetRandomFile(int playlist) {
	LockMutexPtr(adapi->getQueueMutex());

	char pat[1024]="";
	time_t tme = time(NULL);
	TIMER * tmr = adapi->getActiveFilter();
	if (tmr && tme < tmr->extra) {
		sstrcpy(pat, tmr->pattern);
		adapi->ProcText(pat, tme, sizeof(pat));
	}

	conx.Ping();
	ResetChecked();

	std::stringstream sstr2;
	sstr2 << "SELECT COUNT(*) AS Count FROM `" << qm_config.mysql_table << "_Playlists` WHERE Playlist=" << playlist << " AND Seen=1";
	MYSQL_RES * res = conx.Query(sstr2.str().c_str());
	uint32 count = 0;
	if (res && conx.NumRows(res) > 0) {
		SC_Row row;
		if (conx.FetchRow(res, row)) {
			count = strtoul(row.Get("Count").c_str(), NULL, 10);
		}
	}
	conx.FreeResult(res);

	if (count == 0) {
		adapi->botapi->ib_printf(_("AutoDJ -> ERROR: No (supported) files in playlist!\n"));
		return NULL;
	}

	QUEUE *Scan = NULL;
	while (!Scan) {
		int tries = 0;
		while (Scan == NULL && tries++ < 100) {
			std::stringstream sstr;
			uint32 rnd = adapi->botapi->genrand_range(0, count - 1);
			sstr << "SELECT t1.*,t2.Playlist FROM `" << qm_config.mysql_table << "_Playlists` AS t2 RIGHT JOIN `" << qm_config.mysql_table << "` AS t1 ON t1.ID=t2.FileID WHERE t2.Playlist=" << playlist << " AND t2.Seen=1 AND t1.Seen=1 LIMIT " << rnd << ", 3";//18446744073709551615";
			Scan = GetRandomFileExec(sstr.str().c_str(), tme, pat);
		}

		if (Scan == NULL) {
			std::stringstream sstr;
			uint32 seed = adapi->botapi->genrand_int32() & 0x7FFFFFFF;
			sstr << "SELECT t1.*,t2.Playlist FROM `" << qm_config.mysql_table << "_Playlists` AS t2 RIGHT JOIN `" << qm_config.mysql_table << "` AS t1 ON t1.ID=t2.FileID WHERE t2.Playlist=" << playlist << " AND t2.Seen=1 AND t1.Seen=1 ORDER BY RAND(" << seed << ")";
			Scan = GetRandomFileExec(sstr.str().c_str(), tme, pat);
		}

		if (Scan == NULL) {
			if (adapi->getNoRepeatArtist() != 0) {
				adapi->botapi->ib_printf(_("AutoDJ -> Unsetting NoRepeatArtist, hopefully that will solve your problem...\n"));
				adapi->setNoRepeatArtist(0);
			} else if (adapi->getNoRepeat() != 0) {
				adapi->botapi->ib_printf(_("AutoDJ -> Unsetting NoRepeat, hopefully that will solve your problem...\n"));
				adapi->setNoRepeat(0);
			} else if (adapi->getMaxSongDuration() != 0) {
				adapi->botapi->ib_printf(_("AutoDJ -> Unsetting MaxSongDuration, hopefully that will solve your problem...\n"));
				adapi->setMaxSongDuration(0);
			} else {
				//we tried everything, but failed
				break;
			}
		}
	}

	if (!Scan) {
		adapi->botapi->ib_printf(_("AutoDJ -> FATAL ERROR: Can not find any files to play, but GetRandomFile() was called!!!\n"));
		RelMutexPtr(adapi->getQueueMutex());
		return NULL;
	}

	Scan->lastPlayed = tme;
	RelMutexPtr(adapi->getQueueMutex());
	return Scan;
}

void AddFieldIfDoesntExist(const char * field, const char * def) {
	std::stringstream sstr;
	sstr << "SHOW COLUMNS FROM `" << qm_config.mysql_table << "` LIKE '" << field << "'";
	MYSQL_RES * res = conx.Query(sstr.str().c_str());
	if (conx.NumRows(res) == 0) {
		std::stringstream sstr2;
		sstr2 << "ALTER TABLE `" << qm_config.mysql_table << "` ADD `" << field << "` " << def << ";";
		MYSQL_RES * res2 = conx.Query(sstr2.str().c_str());
		conx.FreeResult(res2);
	}
	conx.FreeResult(res);
}

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wnarrowing"
#endif

bool PostProc(READER_HANDLE * fOutList) {
	if (fOutList) {
		char buf[1024];
		char last_char = 0;

		std::stringstream sstr;
		sstr << "SELECT FN FROM `" << qm_config.mysql_table << "` ORDER BY FN ASC";
		MYSQL_RES * res = conx.Query(sstr.str().c_str());
		SC_Row row;
		while (conx.FetchRow(res, row)) {
			char * fn = strdup(row.Get("FN").c_str());
			if (toupper(fn[0]) != last_char && strlen(adapi->FileLister->NewChar)) {
				sprintf(buf,"%s\r\n",adapi->FileLister->NewChar);
				char buf2[2] = { toupper(fn[0]), 0 };
				last_char = toupper(fn[0]);
				str_replace(buf,sizeof(buf),"%char%", buf2);
				fOutList->write(buf,strlen(buf),fOutList);
			}
			sprintf(buf,"%s\r\n",adapi->FileLister->Line);
			str_replace(buf,sizeof(buf),"%file%",fn);
			fOutList->write(buf,strlen(buf),fOutList);
			free(fn);
		}
		conx.FreeResult(res);
	}

	return true;
}

#if __GNUC__ > 4 || (__GNUC__ == 4 && __GNUC_MINOR__ >= 4)
#pragma GCC diagnostic pop
#endif

bool ProcFile(Titus_Mutex * scanMutex, const char * path, const char * fn, const char * ffn, int playlist, void * uptr);//, struct stat st
int32 QueueContentFiles() {
//	char buf[1024];

	LockMutexPtr(adapi->getQueueMutex());

	conx.Ping();

	{
		std::stringstream sstr;
		sstr << "CREATE TABLE IF NOT EXISTS `" << qm_config.mysql_table << "` (`ID` int(10) unsigned NOT NULL, `Seen` TINYINT NOT NULL DEFAULT '0', `Checked` TINYINT NOT NULL DEFAULT '0', `Path` varchar(255) CHARACTER SET utf8 COLLATE utf8_unicode_ci NOT NULL, `FN` varchar(255) CHARACTER SET utf8 COLLATE utf8_unicode_ci NOT NULL, `mTime` int(11) NOT NULL, `IsPlaying` TINYINT NOT NULL DEFAULT '0', `LastPlayed` int(11) NOT NULL DEFAULT '0', `LastListeners` int(11) NOT NULL DEFAULT '0', `PlayCount` int(11) NOT NULL DEFAULT '0', `LastReq` int(11) NOT NULL DEFAULT '0', `ReqCount` int(11) NOT NULL DEFAULT '0', `Rating` TINYINT NOT NULL DEFAULT '0', `RatingVotes` INT NOT NULL DEFAULT '0', `Title` varchar(255) CHARACTER SET utf8 COLLATE utf8_unicode_ci NOT NULL DEFAULT '', `Artist` varchar(255) CHARACTER SET utf8 COLLATE utf8_unicode_ci NOT NULL DEFAULT '', `Album` varchar(255) CHARACTER SET utf8 COLLATE utf8_unicode_ci NOT NULL DEFAULT '', `AlbumArtist` varchar(255) CHARACTER SET utf8 COLLATE utf8_unicode_ci NOT NULL DEFAULT '', `Genre` varchar(255) CHARACTER SET utf8 COLLATE utf8_unicode_ci NOT NULL DEFAULT '', `URL` varchar(255) CHARACTER SET utf8 COLLATE utf8_unicode_ci NOT NULL DEFAULT '', `Comment` varchar(255) CHARACTER SET utf8 COLLATE utf8_unicode_ci NOT NULL DEFAULT '', `TrackNo` int(11) NOT NULL DEFAULT '0', `SongLen` int(11) NOT NULL DEFAULT '0', `Year` int(11) NOT NULL DEFAULT '0', PRIMARY KEY  (`ID`));";
		MYSQL_RES * res = conx.Query(sstr.str().c_str());
		conx.FreeResult(res);
	}
	{
		std::stringstream sstr,sstr2;
		sstr << "SHOW COLUMNS IN `" << qm_config.mysql_table << "` LIKE \"Seen\"";
		MYSQL_RES * res = conx.Query(sstr.str().c_str());
		if (conx.NumRows(res) == 0) {
			conx.FreeResult(res);
			sstr2 << "ALTER TABLE `" << qm_config.mysql_table << "` ADD `Seen` TINYINT NOT NULL DEFAULT '0'";
			res = conx.Query(sstr2.str().c_str());
		}
		conx.FreeResult(res);
	}
	{
		std::stringstream sstr,sstr2;
		sstr << "SHOW COLUMNS IN `" << qm_config.mysql_table << "` LIKE \"Checked\"";
		MYSQL_RES * res = conx.Query(sstr.str().c_str());
		if (conx.NumRows(res) == 0) {
			conx.FreeResult(res);
			sstr2 << "ALTER TABLE `" << qm_config.mysql_table << "` ADD `Checked` TINYINT NOT NULL DEFAULT '0'";
			res = conx.Query(sstr2.str().c_str());
		}
		conx.FreeResult(res);
	}
	{
		std::stringstream sstr,sstr2;
		sstr << "SHOW COLUMNS IN `" << qm_config.mysql_table << "` LIKE \"URL\"";
		MYSQL_RES * res = conx.Query(sstr.str().c_str());
		if (conx.NumRows(res) == 0) {
			conx.FreeResult(res);
			sstr2 << "ALTER TABLE `" << qm_config.mysql_table << "` ADD `URL` varchar(255) CHARACTER SET utf8 COLLATE utf8_unicode_ci NOT NULL DEFAULT ''";
			res = conx.Query(sstr2.str().c_str());
		}
		conx.FreeResult(res);
	}
	{
		std::stringstream sstr,sstr2;
		sstr << "SHOW COLUMNS IN `" << qm_config.mysql_table << "` LIKE \"Comment\"";
		MYSQL_RES * res = conx.Query(sstr.str().c_str());
		if (conx.NumRows(res) == 0) {
			conx.FreeResult(res);
			sstr2 << "ALTER TABLE `" << qm_config.mysql_table << "` ADD `Comment` varchar(255) CHARACTER SET utf8 COLLATE utf8_unicode_ci NOT NULL DEFAULT ''";
			res = conx.Query(sstr2.str().c_str());
		}
		conx.FreeResult(res);
	}
	{
		std::stringstream sstr,sstr2;
		sstr << "SHOW COLUMNS IN `" << qm_config.mysql_table << "` LIKE \"AlbumArtist\"";
		MYSQL_RES * res = conx.Query(sstr.str().c_str());
		if (conx.NumRows(res) == 0) {
			conx.FreeResult(res);
			sstr2 << "ALTER TABLE `" << qm_config.mysql_table << "` ADD `AlbumArtist` varchar(255) CHARACTER SET utf8 COLLATE utf8_unicode_ci NOT NULL DEFAULT ''";
			res = conx.Query(sstr2.str().c_str());
		}
		conx.FreeResult(res);
	}
	{
		std::stringstream sstr,sstr2;
		sstr << "SHOW COLUMNS IN `" << qm_config.mysql_table << "` LIKE \"TrackNo\"";
		MYSQL_RES * res = conx.Query(sstr.str().c_str());
		if (conx.NumRows(res) == 0) {
			conx.FreeResult(res);
			sstr2 << "ALTER TABLE `" << qm_config.mysql_table << "` ADD `TrackNo` INT NOT NULL DEFAULT '0'";
			res = conx.Query(sstr2.str().c_str());
		}
		conx.FreeResult(res);
	}
	{
		std::stringstream sstr,sstr2;
		sstr << "SHOW COLUMNS IN `" << qm_config.mysql_table << "` LIKE \"Year\"";
		MYSQL_RES * res = conx.Query(sstr.str().c_str());
		if (conx.NumRows(res) == 0) {
			conx.FreeResult(res);
			sstr2 << "ALTER TABLE `" << qm_config.mysql_table << "` ADD `Year` INT NOT NULL DEFAULT '0'";
			res = conx.Query(sstr2.str().c_str());
		}
		conx.FreeResult(res);
	}
	{
		std::stringstream sstr,sstr2;
		sstr << "SHOW COLUMNS IN `" << qm_config.mysql_table << "` LIKE \"LastListeners\"";
		MYSQL_RES * res = conx.Query(sstr.str().c_str());
		if (conx.NumRows(res) == 0) {
			conx.FreeResult(res);
			sstr2 << "ALTER TABLE `" << qm_config.mysql_table << "` ADD `LastListeners` INT NOT NULL DEFAULT '0'";
			res = conx.Query(sstr2.str().c_str());
		}
		conx.FreeResult(res);
	}
	/*
	{
		std::stringstream sstr,sstr2;
		sstr << "SHOW COLUMNS IN `" << qm_config.mysql_table << "` LIKE \"ReqID\"";
		MYSQL_RES * res = conx.Query(sstr.str().c_str());
		if (conx.NumRows(res) == 0) {
			conx.FreeResult(res);
			sstr2 << "ALTER TABLE `" << qm_config.mysql_table << "` ADD `ReqID` INT UNSIGNED NOT NULL AUTO_INCREMENT AFTER `ReqCount`, ADD UNIQUE ( `ReqID` )";
			res = conx.Query(sstr2.str().c_str());
		}
		conx.FreeResult(res);
	}
	*/
	{
		std::stringstream sstr;
		sstr << "CREATE TABLE IF NOT EXISTS `" << qm_config.mysql_table << "_RequestHistory` (`ID` INT NOT NULL AUTO_INCREMENT PRIMARY KEY, `SongID` INT UNSIGNED NOT NULL, `TimeStamp` INT NOT NULL, `Nick` VARCHAR(255) NOT NULL);";
		MYSQL_RES * res = conx.Query(sstr.str().c_str());
		conx.FreeResult(res);
	}
	{
		std::stringstream sstr;
		sstr << "CREATE TABLE IF NOT EXISTS `" << qm_config.mysql_table << "_Playlists` (`FileID` int unsigned NOT NULL, `Playlist` int NOT NULL, `Seen` TINYINT NOT NULL DEFAULT '0', PRIMARY KEY  (`FileID`,`Playlist`));";
		MYSQL_RES * res = conx.Query(sstr.str().c_str());
		conx.FreeResult(res);
	}


	{
		std::stringstream sstr;
		sstr << "UPDATE `" << qm_config.mysql_table << "` SET Seen=0, IsPlaying=0";
		MYSQL_RES * res = conx.Query(sstr.str().c_str());
		conx.FreeResult(res);
	}
	{
		std::stringstream sstr;
		sstr << "UPDATE `" << qm_config.mysql_table << "_Playlists` SET Seen=0";
		MYSQL_RES * res = conx.Query(sstr.str().c_str());
		conx.FreeResult(res);
	}

	FreePlayList();
	qm_config.nofiles=0;

	LoadMetaCache();

	qm_config.nofiles = adapi->DoFileScan(ProcFile, PostProc, NULL);

	SaveMetaCache();
	FreeMetaCache();

	{
		std::stringstream sstr;
		sstr << "DELETE FROM `" << qm_config.mysql_table << "_Playlists` WHERE Seen=0";
		MYSQL_RES * res = conx.Query(sstr.str().c_str());
		conx.FreeResult(res);
	}

	RelMutexPtr(adapi->getQueueMutex());
	return qm_config.nofiles;
}

bool ProcFile(Titus_Mutex * scanMutex, const char * path, const char * fn, const char * ffn, int playlist, void * uptr) {
	bool ret = false;
	DECODER * dec = adapi->GetFileDecoder(ffn);
	if (dec) {
		uint32 id = adapi->FileID(ffn);
		QUEUE * Scan = FindFileMeta(id);
		bool doSave=false;
		bool isNew=false;
		if (!Scan) {
			Scan = (QUEUE *)adapi->malloc(sizeof(QUEUE));
			memset(Scan,0,sizeof(QUEUE));

			Scan->fn = (char *)adapi->strdup(fn);
			Scan->path = (char *)adapi->strdup(path);
			Scan->ID = id;
			doSave = true;
			isNew = true;
		}

		TITLE_DATA * td = Scan->meta;
		if (!td) {
			Scan->meta = td = (TITLE_DATA *)adapi->malloc(sizeof(TITLE_DATA));
			memset(td, 0, sizeof(TITLE_DATA));
		}

		if (!adapi->getOnlyScanNewFiles() || isNew) {
			struct stat64 st;
			if (stat64(ffn, &st) == 0) {
				if ((td->title[0] == 0 && Scan->mTime == 0) || Scan->mTime != st.st_mtime) {
					int ret2 = adapi->ReadMetaDataEx(ffn, td, &Scan->songlen, dec);
					if (ret2 == 1) {
						Scan->mTime = st.st_mtime;
						doSave = true;
					} else {
						memset(td, 0, sizeof(TITLE_DATA));
						//Scan->songlen = 0;
						if (Scan->mTime != st.st_mtime) {
							Scan->mTime = st.st_mtime;
							doSave = true;
						}
					}
				}
			} else {
				adapi->botapi->ib_printf(_("AutoDJ -> Error stat()ing file: %s (%d / %s)\n"), ffn, errno, strerror(errno));
			}
		}

		//uint32 songlen = td->songlen;

		if (td->title[0] == 0) {
			Scan->meta = NULL;
			adapi->free(td);
		}

		ret = true;
		if (doSave) {
			std::stringstream sstr;
			if (isNew) {
				sstr << "REPLACE INTO ";
			} else {
				sstr << "UPDATE ";
			}
			sstr << qm_config.mysql_table << " SET ID=" << Scan->ID << ", mTime=" << Scan->mTime << ", Path=CAST('" << conx.EscapeString(Scan->path) << "' AS CHAR(255) CHARACTER SET utf8), FN=CAST('" << conx.EscapeString(Scan->fn) << "' AS CHAR(255) CHARACTER SET utf8), Seen=1";
			if (Scan->meta) {
				sstr << ", Title=CAST('" << conx.EscapeString(Scan->meta->title) << "' AS CHAR(255) CHARACTER SET utf8)";
				sstr << ", Artist=CAST('" << conx.EscapeString(Scan->meta->artist) << "' AS CHAR(255) CHARACTER SET utf8)";
				sstr << ", Album=CAST('" << conx.EscapeString(Scan->meta->album) << "' AS CHAR(255) CHARACTER SET utf8)";
				sstr << ", AlbumArtist=CAST('" << conx.EscapeString(Scan->meta->album_artist) << "' AS CHAR(255) CHARACTER SET utf8)";
				sstr << ", Genre=CAST('" << conx.EscapeString(Scan->meta->genre) << "' AS CHAR(255) CHARACTER SET utf8)";
				sstr << ", URL=CAST('" << conx.EscapeString(Scan->meta->url) << "' AS CHAR(255) CHARACTER SET utf8)";
				sstr << ", `Comment`=CAST('" << conx.EscapeString(Scan->meta->comment) << "' AS CHAR(255) CHARACTER SET utf8)";
				sstr << ", TrackNo='" << Scan->meta->track_no << "'";
				sstr << ", Year='" << Scan->meta->year << "'";
			}
			sstr << ", SongLen=" << Scan->songlen;
			SONG_RATING r = {0,0};
			adapi->botapi->ss->GetSongRatingByID(Scan->ID, &r);
			sstr << ", Rating=" << r.Rating << ", RatingVotes=" << r.Votes;
			if (!isNew) {
				sstr << " WHERE ID=" << Scan->ID;
			}
			if (IsValidUTF8(Scan->path) && IsValidUTF8(Scan->fn)) {
				MYSQL_RES * res = conx.Query(sstr.str().c_str());
				conx.FreeResult(res);
			} else {
				adapi->botapi->ib_printf(_("AutoDJ (queue_mysql) -> Skipping addition of %s%s, invalid UTF-8 in path and/or filename...\n"), Scan->path, Scan->fn);
			}
		} else {
			std::stringstream sstr;
			sstr << "UPDATE ";
			sstr << qm_config.mysql_table << " SET ID=" << Scan->ID << ", Seen=1";
			SONG_RATING r = {0,0};
			adapi->botapi->ss->GetSongRatingByID(Scan->ID, &r);
			sstr << ", Rating=" << r.Rating << ", RatingVotes=" << r.Votes;
			sstr << " WHERE ID=" << Scan->ID;
			MYSQL_RES * res = conx.Query(sstr.str().c_str());
			conx.FreeResult(res);
		}
		std::stringstream sstr;
		sstr << "REPLACE INTO `" << qm_config.mysql_table << "_Playlists` (`FileID`,`Playlist`,`Seen`) VALUES (" << Scan->ID << ", " << playlist << ", 1)";
		MYSQL_RES * res = conx.Query(sstr.str().c_str());
		conx.FreeResult(res);
		if (isNew) {
			adapi->FreeQueue(Scan);
		}
	}
	return ret;
}

int QM_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
	if (!stricmp(command, "songstop10")) {
		char buf[1024];
		std::ostringstream sstr;
		sstr << "SELECT `SongID`,COUNT(*) AS Count FROM  `" << qm_config.mysql_table << "_RequestHistory` GROUP BY `SongID` ORDER BY Count DESC LIMIT 10";
		MYSQL_RES * res = conx.Query(sstr.str().c_str());
		if (conx.NumRows(res) > 0) {
			sprintf(buf, _("Listing top %u most requested song(s):"), uint32(conx.NumRows(res)));
			ut->std_reply(ut, buf);
			SC_Row row;
			int i=1;
			while (conx.FetchRow(res, row)) {
				row.NumFields = row.NumFields;
				QUEUE * song = GetQueueFromID(strtoul(row.Get("SongID").c_str(), NULL, 10));
				sprintf(buf, _("%d. %s with %s requests"), i++, song ? song->fn:"Unknown Song", row.Get("Count").c_str());
				if (song) { adapi->FreeQueue(song); }
				ut->std_reply(ut, buf);
			}
			ut->std_reply(ut, _("End of list."));
		} else {
			sprintf(buf, _("Nobody has requested any songs in the last %d day(s)!"), qm_config.keep_history);
			ut->std_reply(ut, buf);
		}
		conx.FreeResult(res);
	}
	if (!stricmp(command, "requesterstop10")) {
		char buf[1024];
		std::ostringstream sstr;
		sstr << "SELECT `Nick`,COUNT(*) AS Count FROM  `" << qm_config.mysql_table << "_RequestHistory` GROUP BY `Nick` ORDER BY Count DESC LIMIT 10";
		MYSQL_RES * res = conx.Query(sstr.str().c_str());
		if (conx.NumRows(res) > 0) {
			sprintf(buf, _("Listing top %u biggest requester(s):"), uint32(conx.NumRows(res)));
			ut->std_reply(ut, buf);
			SC_Row row;
			int i=1;
			while (conx.FetchRow(res, row)) {
				row.NumFields = row.NumFields;
				sprintf(buf, _("%d. %s with %s requests"), i++, row.Get("Nick").c_str(), row.Get("Count").c_str());
				ut->std_reply(ut, buf);
			}
			ut->std_reply(ut, _("End of list."));
		} else {
			sprintf(buf, _("Nobody has requested any songs in the last %d day(s)!"), qm_config.keep_history);
			ut->std_reply(ut, buf);
		}
		conx.FreeResult(res);
	}

	if (!stricmp(command, "autodj-clearhistory")) {
		ut->std_reply(ut, _("Clearing AutoDJ Request History..."));
		{
			std::stringstream sstr;
			sstr << "TRUNCATE TABLE `" << qm_config.mysql_table << "_RequestHistory`";
			MYSQL_RES * res = conx.Query(sstr.str().c_str());
			conx.FreeResult(res);
		}
		ut->std_reply(ut, _("Operation completed, your history is cleared."));
		return 1;
	}

	if (!stricmp(command, "autodj-clear")) {
		ut->std_reply(ut, _("Clearing AutoDJ Meta Cache..."));
		{
			std::stringstream sstr;
			sstr << "TRUNCATE TABLE `" << qm_config.mysql_table << "`";
			MYSQL_RES * res = conx.Query(sstr.str().c_str());
			conx.FreeResult(res);
		}
		{
			std::stringstream sstr;
			sstr << "DROP TABLE `" << qm_config.mysql_table << "`";
			MYSQL_RES * res = conx.Query(sstr.str().c_str());
			conx.FreeResult(res);
		}
		ut->std_reply(ut, _("Operation completed, your playlist is cleared. Make sure you do an !autodj-reload if you want me to keep playing songs..."));
		return 1;
	}

	if (!stricmp(command, "autodj-qstat")) {
		ut->std_reply(ut, _("=== MySQL Queue Status ==="));
		uint32 seen=0, unseen=0;
		uint32 len=0;
		{
			std::stringstream sstr;
			sstr << "SELECT COUNT(*) AS Count FROM `" << qm_config.mysql_table << "` WHERE Seen=0";
			MYSQL_RES * res = conx.Query(sstr.str().c_str());
			if (res && conx.NumRows(res)) {
				SC_Row row;
				if (conx.FetchRow(res, row)) {
					unseen = atoul(row.Get("Count").c_str());
				}
			}
			conx.FreeResult(res);
		}
		{
			std::stringstream sstr;
			sstr << "SELECT COUNT(*) AS Count FROM `" << qm_config.mysql_table << "` WHERE Seen=1";
			MYSQL_RES * res = conx.Query(sstr.str().c_str());
			if (res && conx.NumRows(res)) {
				SC_Row row;
				if (conx.FetchRow(res, row)) {
					seen = atoul(row.Get("Count").c_str());
				}
			}
			conx.FreeResult(res);
		}
		//tot = seen + unseen;
		{
			std::stringstream sstr;
			sstr << "SELECT SUM(SongLen) AS sLen FROM `" << qm_config.mysql_table << "` WHERE Seen=1";
			MYSQL_RES * res = conx.Query(sstr.str().c_str());
			if (res && conx.NumRows(res)) {
				SC_Row row;
				if (conx.FetchRow(res, row)) {
					len = atoul(row.Get("sLen").c_str());
				}
			}
			conx.FreeResult(res);
		}
		char buf[1024], buf2[64];
		adapi->botapi->textfunc->duration(len, buf2);
		sprintf(buf, _("Songs in DB: %u seen, %u old/missing"), seen, unseen);
		ut->std_reply(ut, buf);
		sprintf(buf, _("Length of Songs in DB: %s (est., not all file types can accurately be determined)"), buf2);
		ut->std_reply(ut, buf);
		if (unseen > 10) {
			ut->std_reply(ut, _("* Note: if you use !autodj-chroot often it is normal to have a lot of old/missing songs. You can also use !autodj-clear to manually clean the database."));
		}
		return 1;
	}
	return 0;
}


bool init(AD_PLUGIN_API * pApi) {
	adapi = pApi;
	memset(&qm_config, 0, sizeof(qm_config));

	DS_CONFIG_SECTION * sec = adapi->botapi->config->GetConfigSection(NULL, "AutoDJ");
	if (sec == NULL) {
		adapi->botapi->ib_printf(_("AutoDJ (queue_mysql) -> No 'AutoDJ' section found in your config!\n"));
		return false;
	}

	qm_config.find_sort = QMFS_NONE;
	strcpy(qm_config.mysql_host, "localhost");
	strcpy(qm_config.mysql_user, "root");
	strcpy(qm_config.mysql_db, "IRCBot");
	strcpy(qm_config.mysql_table, "AutoDJ");

	sec = adapi->botapi->config->GetConfigSection(sec, "Queue_MySQL");
	if (sec) {
		adapi->botapi->config->GetConfigSectionValueBuf(sec, "Host", qm_config.mysql_host, sizeof(qm_config.mysql_host));
		adapi->botapi->config->GetConfigSectionValueBuf(sec, "User", qm_config.mysql_user, sizeof(qm_config.mysql_user));
		adapi->botapi->config->GetConfigSectionValueBuf(sec, "Pass", qm_config.mysql_pass, sizeof(qm_config.mysql_pass));
		adapi->botapi->config->GetConfigSectionValueBuf(sec, "DBName", qm_config.mysql_db, sizeof(qm_config.mysql_db));
		adapi->botapi->config->GetConfigSectionValueBuf(sec, "DBTable", qm_config.mysql_table, sizeof(qm_config.mysql_table));
		if (adapi->botapi->config->GetConfigSectionLong(sec, "Port") > 0) {
			qm_config.mysql_port = (uint16)adapi->botapi->config->GetConfigSectionLong(sec, "Port");
		}
		if (adapi->botapi->config->GetConfigSectionLong(sec, "KeepHistory") > 0) {
			qm_config.keep_history = adapi->botapi->config->GetConfigSectionLong(sec, "KeepHistory");
		}
		char *p = adapi->botapi->config->GetConfigSectionValue(sec, "FindSort");
		if (p) {
			if (!stricmp(p, "rand") || !stricmp(p, "random")) {
				qm_config.find_sort = QMFS_RANDOM;
			} else if (!stricmp(p, "title")) {
				qm_config.find_sort = QMFS_TITLE;
			} else if (!stricmp(p, "filename")) {
				qm_config.find_sort = QMFS_FILENAME;
			} else {
				adapi->botapi->ib_printf(_("AutoDJ (queue_mysql) -> Unknown FindSort type: '%s'\n"), p);
			}
		}
	} else {
		adapi->botapi->ib_printf(_("AutoDJ (queue_mysql) -> No 'Queue_MySQL' section found in your AutoDJ section, using pure defaults\n"));
	}

	bool connected = false;
	for (int tries=0; tries < 5 && !connected; tries++) {
		adapi->botapi->ib_printf(_("AutoDJ (queue_mysql) -> Connecting to MySQL server...\n"));
		if (conx.Connect(qm_config.mysql_host, qm_config.mysql_user, qm_config.mysql_pass, qm_config.mysql_db, qm_config.mysql_port)) {
			connected = true;
		} else {
			adapi->botapi->ib_printf(_("AutoDJ (queue_mysql) -> Waiting 2 second for another attempt...\n"));
			adapi->botapi->safe_sleep_seconds(2);
		}
	}
	if (!connected) { return false; }

	std::stringstream sstr;
	sstr << "UPDATE " << qm_config.mysql_table << " SET LastReq=LastPlayed WHERE LastReq>LastPlayed";
	MYSQL_RES * res = conx.Query(sstr.str().c_str());
	conx.FreeResult(res);

	COMMAND_ACL acl;
	adapi->botapi->commands->FillACL(&acl, 0, UFLAG_MASTER|UFLAG_OP, 0);
	adapi->botapi->commands->RegisterCommand_Proc(adapi->GetPluginNum(), "autodj-clear", &acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE, QM_Commands, _("This command will empty and reset AutoDJ's Meta Cache and Playlist"));
	adapi->botapi->commands->FillACL(&acl, 0, UFLAG_ADVANCED_SOURCE, 0);
	adapi->botapi->commands->RegisterCommand_Proc(adapi->GetPluginNum(), "autodj-qstat", &acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE, QM_Commands, _("This command display some information about your song queue"));
	if (qm_config.keep_history) {
		adapi->botapi->commands->RegisterCommand_Proc(adapi->GetPluginNum(), "autodj-clearhistory", &acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE, QM_Commands, _("This command will empty and reset AutoDJ's request history"));
		adapi->botapi->commands->FillACL(&acl, 0, 0, 0);
		adapi->botapi->commands->RegisterCommand_Proc(adapi->GetPluginNum(), "songstop10", &acl, CM_ALLOW_ALL, QM_Commands, _("This command will show AutoDJ's top 10 most requested songs"));
		adapi->botapi->commands->RegisterCommand_Proc(adapi->GetPluginNum(), "requesterstop10", &acl, CM_ALLOW_ALL, QM_Commands, _("This command will show AutoDJ's top 10 biggest requesters"));
	}
	return true;
}

void quit() {
	adapi->botapi->commands->UnregisterCommandByName( "autodj-clear" );
	adapi->botapi->commands->UnregisterCommandByName( "autodj-qstat" );
	adapi->botapi->commands->UnregisterCommandByName( "autodj-clearhistory" );
	adapi->botapi->commands->UnregisterCommandByName( "songstop10" );
	adapi->botapi->commands->UnregisterCommandByName( "requesterstop10" );
}

void on_song_play(QUEUE * Scan) {
	std::stringstream sstr;
	int64 tme = time(NULL);
	sstr << "UPDATE " << qm_config.mysql_table << " SET LastPlayed=" << tme << ", PlayCount=PlayCount+1, IsPlaying=1 WHERE ID=" << Scan->ID;
	MYSQL_RES * res = conx.Query(sstr.str().c_str());
	conx.FreeResult(res);
}
void on_song_play_over(QUEUE * Scan) {
	std::stringstream sstr;
	STATS * s = adapi->botapi->ss->GetStreamInfo();
	sstr << "UPDATE " << qm_config.mysql_table << " SET IsPlaying=0, LastListeners=" << s->listeners << " WHERE ID=" << Scan->ID;
	MYSQL_RES * res = conx.Query(sstr.str().c_str());
	conx.FreeResult(res);
}
void on_song_request(QUEUE * Scan, const char * nick) {
	std::stringstream sstr;
	int64 tme = time(NULL);
	sstr << "UPDATE " << qm_config.mysql_table << " SET LastReq=" << tme << ", ReqCount=ReqCount+1 WHERE ID=" << Scan->ID;
	MYSQL_RES * res = conx.Query(sstr.str().c_str());
	conx.FreeResult(res);

	if (qm_config.keep_history) {
		std::stringstream sstr;

		sstr << "INSERT INTO `" << qm_config.mysql_table << "_RequestHistory` SET `SongID`=" << Scan->ID << ", `TimeStamp`=" << time(NULL) << ", `Nick`='" << conx.EscapeString(nick ? nick:"") << "'";
		res = conx.Query(sstr.str().c_str());
		conx.FreeResult(res);
		std::stringstream sstr2;
		sstr2 << "DELETE FROM `" << qm_config.mysql_table << "_RequestHistory` WHERE `TimeStamp`<" << (time(NULL) - (qm_config.keep_history * 86400));
		res = conx.Query(sstr2.str().c_str());
		conx.FreeResult(res);
	}
}
void on_song_request_delete(QUEUE * Scan) {
	std::stringstream sstr;
	sstr << "UPDATE " << qm_config.mysql_table << " SET LastReq=LastPlayed WHERE ID=" << Scan->ID;
	MYSQL_RES * res = conx.Query(sstr.str().c_str());
	conx.FreeResult(res);
}
void on_song_rating(uint32 song, uint32 rating, uint32 votes) {
	std::stringstream sstr;
	sstr << "UPDATE " << qm_config.mysql_table << " SET Rating=" << rating << ", RatingVotes=" << votes << " WHERE ID=" << song;
	MYSQL_RES * res = conx.Query(sstr.str().c_str());
	conx.FreeResult(res);
}

AD_QUEUE_PLUGIN plugin = {
	AD_PLUGIN_VERSION,
	"MySQL Queue",

	init,
	quit,
	on_song_play,
	on_song_play_over,
	on_song_request,
	on_song_request_delete,
	on_song_rating,

	QueueContentFiles,
	FreePlayList,

	FindWild,
	FindByMeta,
	FreeFindResult,

	FindFile,
	FindFileByID,
	GetRandomFile,

	NULL
};

PLUGIN_EXPORT AD_QUEUE_PLUGIN * GetADQPlugin() { return &plugin; }

