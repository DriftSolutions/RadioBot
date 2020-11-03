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

#include "Queue_SQLite.h"

AD_PLUGIN_API * adapi=NULL;
QM_CONFIG config;

sql_rows GetTable(const char * query, char **errmsg=NULL) {
	sql_rows rows;
	int ret = 0, tries = 0;
	char ** errmsg2 = errmsg;
	if (errmsg == NULL) {
		static char * errmsg3 = NULL;
		errmsg2 = &errmsg3;
	}
	int nrow=0, ncol=0;
	char ** resultp;
	while ((ret = adapi->botapi->db->GetTable(query, &resultp, &nrow, &ncol, errmsg2)) == SQLITE_BUSY && tries < 5) { tries++; }
	if (ret != SQLITE_OK) {
		adapi->botapi->ib_printf(_("GetTable: %s -> %d (%s)\n"), query, ret, *errmsg2);
	} else {
		if (ncol && nrow) {
			for (int i=0; i < nrow; i++) {
				sql_row row;
				for (int s=0; s < ncol; s++) {
					char * p = resultp[(i*ncol)+ncol+s];
					if (!p) { p=""; }
					row[resultp[s]] = p;
				}
				rows[i] = row;
			}
		}
		adapi->botapi->db->FreeTable(resultp);
	}
	if (errmsg == NULL && *errmsg2) {
		adapi->botapi->db->Free(*errmsg2);
	}
	return rows;
}

void FreePlayList() {
}

QUEUE * GetRowToQueue(const char * query) {
	sql_rows res = GetTable(query);
	if (res.size() == 0) { return NULL; }
	sql_row x = res.begin()->second;
	QUEUE * ret = (QUEUE *)adapi->malloc(sizeof(QUEUE));
	memset(ret, 0, sizeof(QUEUE));
	ret->fn = adapi->strdup(x["FN"].c_str());
	ret->path = adapi->strdup(x["Path"].c_str());
	ret->ID = atoul(x["ID"].c_str());
	ret->mTime = atoi64(x["mTime"].c_str());
	ret->lastPlayed = atoi64(x["LastPlayed"].c_str());
	ret->songlen = atoul(x["SongLen"].c_str());
	const char * p = x["Title"].c_str();
	if (p && strlen(p)) {
		ret->meta = (TITLE_DATA *)adapi->malloc(sizeof(TITLE_DATA));
		memset(ret->meta,0,sizeof(TITLE_DATA));
		strncpy(ret->meta->title, p, sizeof(ret->meta->title)-1);
		strncpy(ret->meta->artist, x["Artist"].c_str(), sizeof(ret->meta->artist)-1);
		strncpy(ret->meta->album, x["Album"].c_str(), sizeof(ret->meta->album)-1);
		strncpy(ret->meta->genre, x["Genre"].c_str(), sizeof(ret->meta->genre)-1);
		strncpy(ret->meta->url, x["URL"].c_str(), sizeof(ret->meta->url)-1);
	}
	return ret;
}

QUEUE * GetQueueFromID(uint32 id) {
	std::stringstream sstr;
	sstr << "SELECT * FROM AutoDJ_Queue WHERE ID=" << id;
	return GetRowToQueue(sstr.str().c_str());
}

QUEUE_FIND_RESULT * FindWild(const char * pat) {
	LockMutexPtr(adapi->getQueueMutex());
	if (adapi->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		adapi->botapi->ib_printf(_("AutoDJ -> QueueMutex should be locked when calling FindWild!\n"));
	}

	QUEUE_FIND_RESULT * ret = (QUEUE_FIND_RESULT *)adapi->malloc(sizeof(QUEUE_FIND_RESULT));
	memset(ret,0,sizeof(QUEUE_FIND_RESULT));

	/*
	std::stringstream sstr;
	char * pat2 = strdup(conx.EscapeString(pat).c_str());
	str_replace(pat2, strlen(pat2)+1, "*", "%");
	sstr << "SELECT * FROM " << qm_config.mysql_table << " WHERE (FN LIKE '" << pat2 << "' OR Artist LIKE '" << pat2 << "' OR Title LIKE '" << pat2 << "' OR Album LIKE '" << pat2 << "') AND Seen=1";
	free(pat2);
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
	*/
	RelMutexPtr(adapi->getQueueMutex());
	return ret;
}

QUEUE_FIND_RESULT * FindByMeta(const char * pat, META_TYPES type) {
	LockMutexPtr(adapi->getQueueMutex());

	QUEUE_FIND_RESULT * ret = (QUEUE_FIND_RESULT *)adapi->malloc(sizeof(QUEUE_FIND_RESULT));
	memset(ret,0,sizeof(QUEUE_FIND_RESULT));

	/*
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
	*/

	RelMutexPtr(adapi->getQueueMutex());
	return ret;
}

void FreeFindResult(QUEUE_FIND_RESULT * qRes) {
	LockMutexPtr(adapi->getQueueMutex());
	for (int i=0; i < qRes->num; i++) {
		adapi->FreeQueue(qRes->results[i]);
	}
	adapi->free(qRes);
	RelMutexPtr(adapi->getQueueMutex());
}

QUEUE * FindFile(const char * fn) {
	LockMutexPtr(adapi->getQueueMutex());
	std::stringstream sstr;
	char * pat2 = adapi->botapi->db->MPrintf("%q", fn);
	sstr << "SELECT * FROM AutoDJ_Queue WHERE FN LIKE '" << pat2 << "' AND Seen=1 LIMIT 1";
	adapi->botapi->db->Free(pat2);
	QUEUE * ret = GetRowToQueue(sstr.str().c_str());
	RelMutexPtr(adapi->getQueueMutex());
	return ret;
}

QUEUE * FindFileByID(uint32 id) {
	LockMutexPtr(adapi->getQueueMutex());
	std::stringstream sstr;
	sstr << "SELECT * FROM AutoDJ_Queue WHERE ID=" << id << " AND Seen=1 LIMIT 1";
	QUEUE * ret = GetRowToQueue(sstr.str().c_str());
	RelMutexPtr(adapi->getQueueMutex());
	return ret;
}

QUEUE * GetRandomFile() {
	LockMutexPtr(adapi->getQueueMutex());
	if (adapi->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		adapi->botapi->ib_printf(_("AutoDJ -> QueueMutex should be locked when calling GetRandomFile!\n"));
	}

	char pat[1024]="";
	time_t tme = time(NULL);
	if (adapi->getActiveFilter() && tme < adapi->getActiveFilter()->extra) {
		strcpy(pat, adapi->getActiveFilter()->pattern);
		adapi->ProcText(pat, tme, sizeof(pat));
	}

	std::stringstream sstr;
	sstr << "UPDATE AutoDJ_Queue SET `Checked`=0";
	adapi->botapi->db->Query(sstr.str().c_str(), NULL, NULL, NULL);

	QUEUE *Scan = NULL;
	while (!Scan) {

		std::stringstream sstr;
		sstr << "SELECT * FROM AutoDJ_Queue WHERE Seen=1 AND `Checked`=0 ORDER BY RAND() LIMIT 1";

		Scan = GetRowToQueue(sstr.str().c_str());
		if (Scan == NULL) {
			//no more results
			if (adapi->getNoRepeat() != 0) {
				adapi->botapi->ib_printf(_("AutoDJ -> Unsetting NoRepeat, hopefully that will solve your problem...\n"));
				adapi->setNoRepeat(0);
			} else {
				break;
			}
		}

		if (Scan) {
			std::stringstream sstr;
			sstr << "UPDATE AutoDJ_Queue SET `Checked`=1 WHERE ID=" << Scan->ID;
			adapi->botapi->db->Query(sstr.str().c_str(), NULL, NULL, NULL);

			if (!adapi->AllowSongPlayback(Scan, tme, pat)) {
				adapi->FreeQueue(Scan);
				Scan = NULL;
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

bool ProcFile(Titus_Mutex * scanMutex, const char * path, const char * fn, const char * ffn, int playlist, void * uptr);

void AddFieldIfDoesntExist(const char * field, const char * def) {
	std::stringstream sstr;
	sstr << "SHOW COLUMNS FROM `AutoDJ_Queue` LIKE '" << field << "'";
	sql_rows res = GetTable(sstr.str().c_str());
	if (res.size() == 0) {
		std::stringstream sstr2;
		sstr2 << "ALTER TABLE `AutoDJ_Queue` ADD `" << field << "` " << def << ";";
		adapi->botapi->db->Query(sstr2.str().c_str(), NULL, NULL, NULL);
	}
}

bool PostProc(READER_HANDLE * fOutList) {
	if (fOutList) {
//		char buf[1024];
		char last_char = 0;

		std::stringstream sstr;
		sstr << "SELECT FN FROM `AutoDJ_Queue` ORDER BY FN ASC";
		/*
		MYSQL_RES * res = conx.Query(sstr.str().c_str());
		SC_Row row;
		while (conx.FetchRow(res, &row)) {
			char * fn = conx.GetValue(&row, "FN");
			if (toupper(fn[0]) != last_char && strlen(adapi->FileLister->NewChar)) {
				sprintf(buf,"%s\r\n",adapi->FileLister->NewChar);
				char buf2[2] = { toupper(fn[0]), 0 };
				last_char = toupper(fn[0]);
				str_replace(buf,sizeof(buf),"%char%", buf2);
				fOutList->write(buf,strlen(buf),1,fOutList);
			}
			sprintf(buf,"%s\r\n",adapi->FileLister->Line);
			str_replace(buf,sizeof(buf),"%file%",fn);
			fOutList->write(buf,strlen(buf),1,fOutList);
			conx.FreeRow(&row);
		}
		conx.FreeResult(res);
		*/
	}

	return true;
}


int32 QueueContentFiles() {
//	char buf[1024];

	LockMutexPtr(adapi->getQueueMutex());

	{
		std::stringstream sstr;
		sstr << "CREATE TABLE IF NOT EXISTS `AutoDJ_Queue` (`ID` UNSIGNED INTEGER NOT NULL, `Path` varchar(255) NOT NULL, `FN` varchar(255) NOT NULL, `mTime` int NOT NULL, `LastPlayed` int NOT NULL DEFAULT '0', `PlayCount` int NOT NULL DEFAULT '0', `LastReq` int NOT NULL DEFAULT '0', `ReqCount` int NOT NULL DEFAULT '0', `Title` varchar(255) NOT NULL DEFAULT '', `Artist` varchar(255) NOT NULL DEFAULT '', `Album` varchar(255) NOT NULL DEFAULT '', `Genre` varchar(255) NOT NULL DEFAULT '', `SongLen` int(11) NOT NULL DEFAULT '0', `Seen` TINYINT NOT NULL DEFAULT '0', `Checked` TINYINT NOT NULL DEFAULT '0', `Rating` TINYINT NOT NULL DEFAULT '0', `RatingVotes` INT NOT NULL DEFAULT '0', `IsPlaying` TINYINT NOT NULL DEFAULT '0', PRIMARY KEY  (`ID`));";
		adapi->botapi->db->Query(sstr.str().c_str(), NULL, NULL, NULL);
	}
	{
		std::stringstream sstr;
		sstr << "UPDATE `AutoDJ_Queue` SET Seen=0, IsPlaying=0";
		adapi->botapi->db->Query(sstr.str().c_str(), NULL, NULL, NULL);
	}
	{
		std::stringstream sstr;
		sstr << "CREATE TABLE IF NOT EXISTS `AutoDJ_RequestHistory` (`ID` INTEGER PRIMARY KEY AUTOINCREMENT , `SongID` INT NOT NULL, `TimeStamp` INT NOT NULL, `Nick` VARCHAR(255) NOT NULL);";
		adapi->botapi->db->Query(sstr.str().c_str(), NULL, NULL, NULL);
	}

	FreePlayList();
	config.nofiles=0;

	config.nofiles = adapi->DoFileScan(ProcFile, PostProc, NULL);

	RelMutexPtr(adapi->getQueueMutex());
	return config.nofiles;
}

bool ProcFile(Titus_Mutex * scanMutex, const char * path, const char * fn, const char * ffn, int playlist, void * uptr) {//, struct stat st
	bool ret = false;
	DECODER * dec = adapi->GetFileDecoder(fn);
	if (dec) {
		uint32 id = adapi->FileID(ffn);
		QUEUE * Scan = GetQueueFromID(id);
		bool doSave=false;
		bool isNew=false;
		if (!Scan) {
			Scan = (QUEUE *)adapi->malloc(sizeof(QUEUE));
			memset(Scan,0,sizeof(QUEUE));

			size_t len = strlen(fn)+1;
			Scan->fn = (char *)adapi->malloc(len);
			strcpy(Scan->fn, fn);
			len = strlen(path)+1;
			Scan->path = (char *)adapi->malloc(len);
			strcpy(Scan->path, path);
			Scan->ID = id;
			doSave = true;
			isNew = true;
		}

		TITLE_DATA * td = Scan->meta;
		if (!td) {
			Scan->meta = td = (TITLE_DATA *)adapi->malloc(sizeof(TITLE_DATA));
			memset(td, 0, sizeof(TITLE_DATA));
		}

		if ((!strlen(td->title) && Scan->mTime == 0) || Scan->mTime != st.st_mtime) {
			bool ret2 = false;
			try {
				ret2 = dec->get_title_data(ffn,td,&Scan->songlen);
			} catch(...) {
				adapi->botapi->ib_printf(_("AutoDJ -> Exception occured while trying to process %s\n"), fn);
				ret2 = false;
			}
			if (ret2) {
				strtrim(td->title);
				strtrim(td->album);
				strtrim(td->artist);
				strtrim(td->genre);
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

		//uint32 songlen = td->songlen;

		if (!strlen(td->title)) {
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
			char * tmp1 = adapi->botapi->db->MPrintf("%q", Scan->path);
			char * tmp2 = adapi->botapi->db->MPrintf("%q", Scan->fn);
			sstr << "AutoDJ_Queue SET ID=" << Scan->ID << ", mTime=" << Scan->mTime << ", Path='" << tmp1 << "', FN='" << tmp2 << "', Seen=1";
			adapi->botapi->db->Free(tmp2);
			adapi->botapi->db->Free(tmp1);
			if (Scan->meta) {
				char * tmp = adapi->botapi->db->MPrintf("%q", Scan->meta->title);
				sstr << ", Title='" << tmp << "'";
				adapi->botapi->db->Free(tmp);
				tmp = adapi->botapi->db->MPrintf("%q", Scan->meta->artist);
				sstr << ", Artist='" << tmp << "'";
				adapi->botapi->db->Free(tmp);
				tmp = adapi->botapi->db->MPrintf("%q", Scan->meta->album);
				sstr << ", Album='" << tmp << "'";
				adapi->botapi->db->Free(tmp);
				tmp = adapi->botapi->db->MPrintf("%q", Scan->meta->genre);
				sstr << ", Genre='" << tmp << "'";
				adapi->botapi->db->Free(tmp);
			}
			sstr << ", SongLen=" << Scan->songlen;
			SONG_RATING r = {0,0};
			adapi->botapi->GetSongRatingByID(Scan->ID, &r);
			sstr << ", Rating=" << r.Rating << ", RatingVotes=" << r.Votes;
			if (!isNew) {
				sstr << " WHERE ID=" << Scan->ID;
			}
			adapi->botapi->db->Query(sstr.str().c_str(), NULL, NULL, NULL);
		} else {
			std::stringstream sstr;
			if (isNew) {
				sstr << "REPLACE INTO ";
			} else {
				sstr << "UPDATE ";
			}
			sstr << "AutoDJ_Queue SET ID=" << Scan->ID << ", Seen=1";
			SONG_RATING r = {0,0};
			adapi->botapi->GetSongRatingByID(Scan->ID, &r);
			sstr << ", Rating=" << r.Rating << ", RatingVotes=" << r.Votes;
			if (!isNew) {
				sstr << " WHERE ID=" << Scan->ID;
			}
			adapi->botapi->db->Query(sstr.str().c_str(), NULL, NULL, NULL);
		}
		adapi->FreeQueue(Scan);
	}
	return ret;
}

int QM_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
	if (!stricmp(command, "autodj-clear")) {
		ut->std_reply(ut, _("Clearing AutoDJ Meta Cache..."));
		{
			std::stringstream sstr;
			sstr << "TRUNCATE TABLE `AutoDJ_Queue`";
			adapi->botapi->db->Query(sstr.str().c_str(), NULL, NULL, NULL);
		}
		{
			std::stringstream sstr;
			sstr << "DROP TABLE `AutoDJ_Queue`";
			adapi->botapi->db->Query(sstr.str().c_str(), NULL, NULL, NULL);
		}
		ut->std_reply(ut, _("Operation completed, your playlist is cleared. Make sure you do an !autodj-reload if you want me to keep playing songs..."));
		return 1;
	}
	if (!stricmp(command, "autodj-qstat")) {
		ut->std_reply(ut, _("=== SQLite Queue Status ==="));
		uint32 seen=0, unseen=0, tot=0;
		uint32 len=0;
		{
			std::stringstream sstr;
			sstr << "SELECT COUNT(*) AS Count FROM `AutoDJ_Queue` WHERE Seen=0";
			sql_rows res = GetTable(sstr.str().c_str());
			sql_row row = res.begin()->second;
			unseen = atoul(row["Count"].c_str());
		}
		{
			std::stringstream sstr;
			sstr << "SELECT COUNT(*) AS Count FROM `AutoDJ_Queue` WHERE Seen=1";
			sql_rows res = GetTable(sstr.str().c_str());
			sql_row row = res.begin()->second;
			seen = atoul(row["Count"].c_str());
		}
		tot = seen + unseen;
		{
			std::stringstream sstr;
			sstr << "SELECT SUM(SongLen) AS sLen FROM `AutoDJ_Queue` WHERE Seen=1";
			sql_rows res = GetTable(sstr.str().c_str());
			sql_row row = res.begin()->second;
			len = atoul(row["Count"].c_str());
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
	api = pApi;
	memset(&config, 0, sizeof(config));

	DS_CONFIG_SECTION * sec = adapi->botapi->config->GetConfigSection(NULL, "AutoDJ");
	if (sec == NULL) {
		adapi->botapi->ib_printf(_("AutoDJ -> No 'AutoDJ' section found in your config!\n"));
		return false;
	}

	sec = adapi->botapi->config->GetConfigSection(sec, "Queue_SQLite");
	if (sec) {
		if (adapi->botapi->config->GetConfigSectionLong(sec, "KeepHistory") > 0) {
			config.keep_history = adapi->botapi->config->GetConfigSectionLong(sec, "KeepHistory");
		}
	} else {
		adapi->botapi->ib_printf(_("AutoDJ -> No 'Queue_SQLite' section found in your AutoDJ section, using pure defaults\n"));
	}

	COMMAND_ACL acl;
	adapi->botapi->commands->FillACL(&acl, 0, UFLAG_MASTER|UFLAG_OP, 0);
	adapi->botapi->commands->RegisterCommand_Proc(adapi->GetPluginNum(), "autodj-clear", &acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE, QM_Commands, _("This command will empty and reset AutoDJ's Meta Cache and Playlist"));
	adapi->botapi->commands->RegisterCommand_Proc(adapi->GetPluginNum(), "autodj-qstat", &acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE, QM_Commands, _("This command display some information about your song queue"));
//	adapi->botapi->commands->RegisterCommand_Proc("songstop10", 5, CM_ALLOW_IRC_PUBLIC|CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE, QM_Commands, _("This command will show AutoDJ's top 10 most requested songs"));
//	adapi->botapi->commands->RegisterCommand_Proc("requesterstop10", 5, CM_ALLOW_IRC_PUBLIC|CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE, QM_Commands, _("This command will show AutoDJ's top 10 biggest requesters"));

	return true;
}

void quit() {
	adapi->botapi->commands->UnregisterCommandByName( "autodj-clear" );
	adapi->botapi->commands->UnregisterCommandByName( "autodj-qstat" );
//	adapi->botapi->commands->UnregisterCommandByName( "songstop10" );
//	adapi->botapi->commands->UnregisterCommandByName( "requesterstop10" );
}

void on_song_play(QUEUE * Scan) {
	std::stringstream sstr;
	sstr << "UPDATE AutoDJ_Queue SET LastPlayed=" << time(NULL) << ", PlayCount=PlayCount+1, IsPlaying=1 WHERE ID=" << Scan->ID;
	adapi->botapi->db->Query(sstr.str().c_str(), NULL, NULL, NULL);
}
void on_song_play_over(QUEUE * Scan) {
	std::stringstream sstr;
	sstr << "UPDATE AutoDJ_Queue SET IsPlaying=0 WHERE ID=" << Scan->ID;
	adapi->botapi->db->Query(sstr.str().c_str(), NULL, NULL, NULL);
}
void on_song_request(QUEUE * Scan, const char * nick) {
	std::stringstream sstr;
	sstr << "UPDATE AutoDJ_Queue SET LastReq=" << time(NULL) << ", ReqCount=ReqCount+1 WHERE ID=" << Scan->ID;
	adapi->botapi->db->Query(sstr.str().c_str(), NULL, NULL, NULL);

	if (config.keep_history) {
		std::stringstream sstr;

		char * tmp = "";
		if (nick) {
			tmp = adapi->botapi->db->MPrintf("%q", nick);
		}
		sstr << "INSERT INTO `AutoDJ_RequestHistory` SET `SongID`=" << Scan->ID << ", `TimeStamp`=" << time(NULL) << ", `Nick`='" << tmp << "'";
		adapi->botapi->db->Free(tmp);
		adapi->botapi->db->Query(sstr.str().c_str(), NULL, NULL, NULL);

		std::stringstream sstr2;
		sstr2 << "DELETE FROM `AutoDJ_RequestHistory` WHERE `TimeStamp`<" << (time(NULL) - (config.keep_history * 86400));
		adapi->botapi->db->Query(sstr2.str().c_str(), NULL, NULL, NULL);
	}
}
void on_song_rating(uint32 song, uint32 rating, uint32 votes) {
	std::stringstream sstr;
	sstr << "UPDATE AutoDJ_Queue SET Rating=" << rating << ", RatingVotes=" << votes << " WHERE ID=" << song;
	adapi->botapi->db->Query(sstr.str().c_str(), NULL, NULL, NULL);
}

AD_QUEUE_PLUGIN plugin = {
	AD_PLUGIN_VERSION,
	"SQLite Queue",

	init,
	quit,
	on_song_play,
	on_song_play_over,
	on_song_request,
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

