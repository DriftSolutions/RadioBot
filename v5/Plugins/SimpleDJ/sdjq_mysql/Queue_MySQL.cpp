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

SD_PLUGIN_API * api=NULL;
QM_CONFIG qm_config;
DB_MySQL conx;

void GenCRC32Table();
unsigned long QM_CRC32(unsigned char *block, unsigned int length);
bool has_crc_init=false;

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

void FreePlayList() {
	LockMutexPtr(api->getQueueMutex());
	if (api->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		api->botapi->ib_printf("SimpleDJ -> QueueMutex should be locked when calling FreePlayList!\n");
	}
	RelMutexPtr(api->getQueueMutex());
}

QUEUE * GetRowToQueue(MYSQL_RES * res) {
	if (!res) { return NULL; }
	QUEUE * ret = NULL;
	SC_Row row;
	if (conx.FetchRow(res, row)) {
		ret = (QUEUE *)api->malloc(sizeof(QUEUE));
		memset(ret, 0, sizeof(QUEUE));
		ret->fn = api->strdup(row.Get("FN").c_str());
		ret->path = api->strdup(row.Get("Path").c_str());
		ret->ID = atoul(row.Get("ID").c_str());
		ret->mTime = atoi64(row.Get("mTime").c_str());
		ret->LastPlayed = atoi64(row.Get("LastPlayed").c_str());
		ret->songlen = atoul(row.Get("SongLen").c_str());
		string p = row.Get("Title");
		if (p.length()) {
			ret->meta = (TITLE_DATA *)api->malloc(sizeof(TITLE_DATA));
			memset(ret->meta,0,sizeof(TITLE_DATA));
			strlcpy(ret->meta->title, p.c_str(), sizeof(ret->meta->title));
			strlcpy(ret->meta->artist, row.Get("Artist").c_str(), sizeof(ret->meta->artist));
			strlcpy(ret->meta->album, row.Get("Album").c_str(), sizeof(ret->meta->album));
			strlcpy(ret->meta->genre, row.Get("Genre").c_str(), sizeof(ret->meta->genre));
		}
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
	LockMutexPtr(api->getQueueMutex());
	if (api->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		api->botapi->ib_printf("SimpleDJ -> QueueMutex should be locked when calling FindWild!\n");
	}

	QUEUE_FIND_RESULT * ret = (QUEUE_FIND_RESULT *)api->malloc(sizeof(QUEUE_FIND_RESULT));
	memset(ret,0,sizeof(QUEUE_FIND_RESULT));

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
				api->FreeQueue(Scan);
				break;
			}
			ret->results[ret->num] = Scan;
			ret->num++;
		}
	}
	conx.FreeResult(res);
	RelMutexPtr(api->getQueueMutex());
	return ret;
}

QUEUE_FIND_RESULT * FindByMeta(const char * pat, META_TYPES type) {
	LockMutexPtr(api->getQueueMutex());
	if (api->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		api->botapi->ib_printf("SimpleDJ -> QueueMutex should be locked when calling FindByMeta!\n");
	}

	QUEUE_FIND_RESULT * ret = (QUEUE_FIND_RESULT *)api->malloc(sizeof(QUEUE_FIND_RESULT));
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

	conx.Ping();
	MYSQL_RES * res = conx.Query(sstr.str().c_str());
	if (res && conx.NumRows(res) > 0) {
		QUEUE * Scan=NULL;
		while ((Scan = GetRowToQueue(res)) != NULL) {
			if (ret->num == MAX_RESULTS) {
				ret->not_enough = true;
				api->FreeQueue(Scan);
				break;
			}
			ret->results[ret->num] = Scan;
			ret->num++;
		}
	}
	conx.FreeResult(res);

	RelMutexPtr(api->getQueueMutex());
	return ret;
}

void FreeFindResult(QUEUE_FIND_RESULT * qRes) {
	LockMutexPtr(api->getQueueMutex());
	if (api->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		api->botapi->ib_printf("SimpleDJ -> QueueMutex should be locked when calling FreeFindResult!\n");
	}
	for (int i=0; i < qRes->num; i++) {
		api->FreeQueue(qRes->results[i]);
	}
	api->free(qRes);
	RelMutexPtr(api->getQueueMutex());
}

QUEUE * FindFile(const char * fn) {
	LockMutexPtr(api->getQueueMutex());
	if (api->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		api->botapi->ib_printf("SimpleDJ -> QueueMutex should be locked when calling FindFile!\n");
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
	RelMutexPtr(api->getQueueMutex());
	return ret;
}

QUEUE * FindFileByID(uint32 id) {
	LockMutexPtr(api->getQueueMutex());
	if (api->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		api->botapi->ib_printf("SimpleDJ -> QueueMutex should be locked when calling FindFile!\n");
	}

	QUEUE * ret = NULL;

	std::stringstream sstr;
	sstr << "SELECT * FROM " << qm_config.mysql_table << " WHERE ID=" << id << " AND Seen=1";
	conx.Ping();
	MYSQL_RES * res = conx.Query(sstr.str().c_str());
	if (res && conx.NumRows(res) > 0) {
		ret = GetRowToQueue(res);
	}
	conx.FreeResult(res);
	RelMutexPtr(api->getQueueMutex());
	return ret;
}

QUEUE * GetRandomFile() {
	LockMutexPtr(api->getQueueMutex());
	if (api->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		api->botapi->ib_printf("SimpleDJ -> QueueMutex should be locked when calling GetRandomFile!\n");
	}

//	char buf[4096];

	int tries = 0;
	QUEUE *Scan = NULL;
	while (!Scan && tries < 40) {
		tries++;

		if (tries == 30) {
			if (api->getNoRepeat() != 0) {
				api->botapi->ib_printf("SimpleDJ -> Unsetting NoRepeat, hopefully that will solve your problem...\n");
				api->setNoRepeat(0);
			}
		}

		std::stringstream sstr;
		sstr << "SELECT * FROM " << qm_config.mysql_table << " WHERE Seen=1";

		if (api->getNoRepeat() > 0) {
			time_t lp = time(NULL) - api->getNoRepeat();
			sstr << " AND LastPlayed < " << lp;
		}

		if (api->getActiveFilter() && time(NULL) < api->getActiveFilter()->extra) {

			/*
			QUEUE * Start = Scan;
			strcpy(buf, api->getActiveFilter()->pattern);
			api->ProcText(buf, time(NULL), sizeof(buf));
			while (!api->MatchFilter(Scan, api->getActiveFilter(), buf)) {
				Scan=Scan->Next;
				if (Scan == NULL) { Scan=qFirst; } // loop about
				if (Scan == Start) {
					api->botapi->ib_printf("SimpleDJ -> ERROR: All files are blocked by your current filter, disabling...\n",Scan->path,Scan->fn);
					if (api->getActiveFilter()) {
						// the filter is expired if it exists here
						api->removeActiveFilter();
						//zfree(GetADConfig()->Filter);
						//GetADConfig()->Filter = NULL;
					}

					// there is not a a single file the filter will allow
					Scan=NULL;
					//tries=40;
					break;
				}
			}

			if (!Scan) { continue; }
			*/

		} else {

			if (api->getActiveFilter()) {
				// the filter is expired if it exists here
				api->removeActiveFilter();
			}

			//NoPlayFilters do not apply when a filter is active

			char * pat2 = strdup(conx.EscapeString(api->getNoPlayFilters()).c_str());
			str_replace(pat2, strlen(pat2)+1, "*", "%");
			StrTokenizer * st = new StrTokenizer(pat2, ',');
			int num = st->NumTok();
			for (int i=1; i <= num; i++) {
				char * pat = st->GetSingleTok(i);
				if (strlen(pat)) {
					sstr << " AND ";
					sstr << "FN NOT LIKE '" << pat << "' AND Path NOT LIKE '" << pat << "'";
					//cnt++;
				}
				st->FreeString(pat);
			}
			delete st;
			free(pat2);

			/*
			sprintf(buf, "%s%s", Scan->path, Scan->fn);
			if (api->MatchesNoPlayFilter(buf)) {
				api->botapi->ib_printf("SimpleDJ -> %s%s blocked by NoPlayFilters...\n",Scan->path,Scan->fn);
				Scan = NULL;
				continue;
			}
			*/
		}

		sstr << " ORDER BY RAND() LIMIT 1";

		conx.Ping();
		MYSQL_RES * res = conx.Query(sstr.str().c_str());
		if (res && conx.NumRows(res) > 0) {
			Scan = GetRowToQueue(res);
		}
		conx.FreeResult(res);
	}

	if (!Scan) {
		api->botapi->ib_printf("SimpleDJ -> FATAL ERROR: Can not find any files to play, but GetRandomFile() was called!!!\n");
		RelMutexPtr(api->getQueueMutex());
		return NULL;
	}

	Scan->LastPlayed = time(NULL);
	RelMutexPtr(api->getQueueMutex());
	return Scan;
}

typedef std::vector<std::string> dirListType;
dirListType dirList;
Titus_Mutex scanMutex;

struct SCANTHREAD {
	bool done;
	int32 * num_files;
};

THREADTYPE ScanDirectory(void * lpData);

int32 QueueContentFiles() {
//	char buf[1024];

	LockMutexPtr(api->getQueueMutex());

	if (!has_crc_init) {
		GenCRC32Table();
		has_crc_init = true;
	}

	conx.Ping();

	{
		std::stringstream sstr;
		sstr << "CREATE TABLE IF NOT EXISTS `" << qm_config.mysql_table << "` (`ID` int(10) unsigned NOT NULL, `Path` varchar(255) NOT NULL, `FN` varchar(255) NOT NULL, `mTime` int(11) NOT NULL, `LastPlayed` int(11) NOT NULL, `PlayCount` int(11) NOT NULL, `LastReq` int(11) NOT NULL, `ReqCount` int(11) NOT NULL, `Title` varchar(255) CHARACTER SET utf8 COLLATE utf8_unicode_ci NOT NULL, `Artist` varchar(255) CHARACTER SET utf8 COLLATE utf8_unicode_ci NOT NULL, `Album` varchar(255) CHARACTER SET utf8 COLLATE utf8_unicode_ci NOT NULL, `Genre` varchar(255) CHARACTER SET utf8 COLLATE utf8_unicode_ci NOT NULL, `SongLen` int(11) NOT NULL, `Seen` TINYINT NOT NULL, `Rating` TINYINT NOT NULL, `RatingVotes` INT NOT NULL, `IsPlaying` TINYINT NOT NULL, PRIMARY KEY  (`ID`));";
		MYSQL_RES * res = conx.Query(sstr.str().c_str());
		conx.FreeResult(res);
	}
	{
		std::stringstream sstr;
		sstr << "SHOW COLUMNS FROM  `" << qm_config.mysql_table << "` LIKE 'Seen'";
		MYSQL_RES * res = conx.Query(sstr.str().c_str());
		if (conx.NumRows(res) == 0) {
			std::stringstream sstr2;
			sstr2 << "ALTER TABLE `" << qm_config.mysql_table << "` ADD `Seen` TINYINT NOT NULL DEFAULT '1', ADD `Rating` TINYINT NOT NULL, ADD `RatingVotes` INT NOT NULL;";
			MYSQL_RES * res2 = conx.Query(sstr2.str().c_str());
			conx.FreeResult(res2);
		}
		conx.FreeResult(res);
	}
	{
		std::stringstream sstr;
		sstr << "SHOW COLUMNS FROM  `" << qm_config.mysql_table << "` LIKE 'IsPlaying'";
		MYSQL_RES * res = conx.Query(sstr.str().c_str());
		if (conx.NumRows(res) == 0) {
			std::stringstream sstr2;
			sstr2 << "ALTER TABLE `" << qm_config.mysql_table << "` ADD `IsPlaying` TINYINT NOT NULL;";
			MYSQL_RES * res2 = conx.Query(sstr2.str().c_str());
			conx.FreeResult(res2);
		}
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
		sstr << "CREATE TABLE IF NOT EXISTS `" << qm_config.mysql_table << "_RequestHistory` (`ID` INT NOT NULL AUTO_INCREMENT PRIMARY KEY, `SongID` INT UNSIGNED NOT NULL, `TimeStamp` INT NOT NULL);";
		MYSQL_RES * res = conx.Query(sstr.str().c_str());
		conx.FreeResult(res);
	}

	FreePlayList();
	qm_config.nofiles=0;

	FILE *fOutList=NULL;
	if (strlen(api->FileLister->List) > 0) {
		fOutList=fopen(api->FileLister->List,"wb");
		if (strlen(api->FileLister->Header) && fOutList != NULL) {
			FILE *fOutHeader=fopen(api->FileLister->Header,"rb");
			if (fOutHeader) {
				fseek(fOutHeader,0,SEEK_END);
				long len=ftell(fOutHeader);
				fseek(fOutHeader,0,SEEK_SET);
				char *outfl_buf = (char *)api->malloc(len);
				if (fread(outfl_buf,len,1,fOutHeader) == 1 && fwrite(outfl_buf,len,1,fOutList) == 1) {
					fflush(fOutList);
				} else {
					printf("Error reading/writing file lister header!\n");
				}
				api->free(outfl_buf);
				fclose(fOutHeader);
			} else {
				fprintf(fOutList,"Error opening '%s'\r\n",api->FileLister->Header);
				api->botapi->ib_printf("SimpleDJ -> Error opening file lister header file!\n");
			}
		}
	}

	dirList.clear();
	time_t scan_time = time(NULL);
	char * tmp = api->strdup(api->getContentDir());
	char * p2 = NULL;
	char * p = strtok_r(tmp, ";", &p2);
	while (p) {
		dirList.push_back(p);
		//ScanDirectory(p, &qm_config.nofiles);
		p = strtok_r(NULL, ";", &p2);
	}
	api->free(tmp);

	api->botapi->ib_printf("SimpleDJ -> Beginning file scan...\n");
	#define NUM_SCAN_THREADS 2
	SCANTHREAD st[NUM_SCAN_THREADS];
	memset(&st, 0, sizeof(st));
	for (int i=0; i < NUM_SCAN_THREADS; i++) {
		st[i].num_files = &qm_config.nofiles;
		TT_StartThread(ScanDirectory, &st[i], NULL, api->GetPluginNum());
	}

	bool done = false;
	while (!done) {
		safe_sleep(100, true);
		done = true;
		for (int i=0; i < NUM_SCAN_THREADS; i++) {
			if (!st[i].done) { done = false; break; }
		}
	}

	scan_time = time(NULL) - scan_time;
	api->botapi->ib_printf("SimpleDJ -> %d files scanned in %d seconds...\n", qm_config.nofiles, scan_time);

	if (fOutList) {
		char last_char = 0;
		/*
		QUEUE * Scan = qFirst;
		while (Scan) {
			if (toupper(Scan->fn[0]) != last_char && strlen(api->FileLister->NewChar)) {
				sprintf(buf,"%s\r\n",api->FileLister->NewChar);
				char buf2[2] = { toupper(Scan->fn[0]), 0 };
				last_char = toupper(Scan->fn[0]);
				api->botapi->textfunc->StrReplace(buf,sizeof(buf),"%char%", buf2);
				fwrite(buf,strlen(buf),1,fOutList);
			}
			sprintf(buf,"%s\r\n",api->FileLister->Line);
			api->botapi->textfunc->StrReplace(buf,sizeof(buf),"%file%",Scan->fn);
			fwrite(buf,strlen(buf),1,fOutList);
			Scan = Scan->Next;
		}
		*/

		if (strlen(api->FileLister->Footer) && fOutList != NULL) {
			FILE *fOutHeader=fopen(api->FileLister->Footer,"r");
			if (fOutHeader) {
				fseek(fOutHeader,0,SEEK_END);
				long len=ftell(fOutHeader);
				fseek(fOutHeader,0,SEEK_SET);
				char *outfl_buf = (char *)api->malloc(len);
				if (fread(outfl_buf,len,1,fOutHeader) == 1 && fwrite(outfl_buf,len,1,fOutList) == 1) {
					fflush(fOutList);
				} else {
					printf("Error reading/writing file lister footer!\n");
				}
				api->free(outfl_buf);
				fclose(fOutHeader);
			} else {
				fprintf(fOutList,"Error opening '%s'\r\n",api->FileLister->Footer);
				api->botapi->ib_printf("SimpleDJ -> Error opening file lister footer file!\n");
			}
		}
		fclose(fOutList);
	}
	RelMutexPtr(api->getQueueMutex());
	return qm_config.nofiles;
}

/*
void AddToQueue(QUEUE * q, QUEUE ** qqFirst, QUEUE ** qqLast) {
	if (api->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		api->botapi->ib_printf("SimpleDJ -> QueueMutex should be locked when calling AddToQueue!\n");
	}
	QUEUE *fQue = *qqFirst;
	QUEUE *lQue = *qqLast;
	q->Next = NULL;
	if (fQue) {
		lQue->Next = q;
		q->Prev = lQue;
		lQue = q;
	} else {
		q->Prev = NULL;
		fQue = lQue = q;
	}
	*qqFirst = fQue;
	*qqLast  = lQue;
}
*/

TT_DEFINE_THREAD(ScanDirectory) {
//void ScanDirectory(const char * path, int32 * num_files) {
	TT_THREAD_START
	SCANTHREAD * hst = (SCANTHREAD *)tt->parm;

	char buf[MAX_PATH*4],buf2[MAX_PATH*4];

	while (!hst->done) {
		scanMutex.Lock();
		if (dirList.size() > 0) {
			dirListType::iterator x = dirList.begin();
			char * path = api->strdup((*x).c_str());
			dirList.erase(x);
			scanMutex.Release();

			//api->botapi->ib_printf("ScanDir: %s\n", path);

			Directory * dir = new Directory(path);
			DECODER * dec = NULL;
			while (dir->Read(buf, sizeof(buf), NULL)) {
				snprintf(buf2, sizeof(buf2), "%s%s", path, buf);
				struct stat st;
				if (stat(buf2, &st)) {// error completing stat
					continue;
				}
				if (!S_ISDIR(st.st_mode)) {
					if ((dec = api->GetFileDecoder(buf))) {
						uint32 id = FileID(buf2);
						QUEUE * Scan = GetQueueFromID(id);
						bool doSave=false;
						bool isNew=false;
						if (!Scan) {
							Scan = (QUEUE *)api->malloc(sizeof(QUEUE));
							memset(Scan,0,sizeof(QUEUE));

							size_t len = strlen(buf)+1;
							Scan->fn = (char *)api->malloc(len);
							strcpy(Scan->fn, buf);
							len = strlen(path)+1;
							Scan->path = (char *)api->malloc(len);
							strcpy(Scan->path, path);
							Scan->ID = id;
							doSave = true;
							isNew = true;
						}

						TITLE_DATA * td = Scan->meta;
						if (!td) {
							Scan->meta = td = (TITLE_DATA *)api->malloc(sizeof(TITLE_DATA));
							memset(td, 0, sizeof(TITLE_DATA));
						}

						if ((!strlen(td->title) && Scan->mTime == 0) || Scan->mTime != st.st_mtime) {
							bool ret = false;
							try {
								//api->botapi->ib_printf("File: %s\n", buf);
								ret = dec->get_title_data(buf2,td,&Scan->songlen);
								//api->botapi->ib_printf("Scanned\n");
							} catch(...) {
								api->botapi->ib_printf("SimpleDJ -> Exception occured while trying to process %s\n", buf);
								ret = false;
							}
							if (!ret) {
								memset(td, 0, sizeof(TITLE_DATA));
							}
							strtrim(td->title);
							strtrim(td->album);
							strtrim(td->artist);
							strtrim(td->genre);
							Scan->mTime = st.st_mtime;
							doSave = true;
						}

						//uint32 songlen = td->songlen;

						if (!strlen(td->title)) {
							Scan->meta = NULL;
							api->free(td);
						}

						scanMutex.Lock();
						*hst->num_files = *hst->num_files + 1;
						scanMutex.Release();
						if (doSave) {
							std::stringstream sstr;
							if (isNew) {
								sstr << "REPLACE INTO ";
							} else {
								sstr << "UPDATE ";
							}
							sstr << qm_config.mysql_table << " SET ID=" << Scan->ID << ", mTime=" << Scan->mTime << ", Path='" << conx.EscapeString(Scan->path) << "', FN='" << conx.EscapeString(Scan->fn) << "', Seen=1";
							if (Scan->meta) {
								sstr << ", Title='" << conx.EscapeString(Scan->meta->title) << "'";
								sstr << ", Artist='" << conx.EscapeString(Scan->meta->artist) << "'";
								sstr << ", Album='" << conx.EscapeString(Scan->meta->album) << "'";
								sstr << ", Genre='" << conx.EscapeString(Scan->meta->genre) << "'";
							}
							sstr << ", SongLen=" << Scan->songlen;
							SONG_RATING r = {0,0};
							api->botapi->ss->GetSongRatingByID(Scan->ID, &r);
							sstr << ", Rating=" << r.Rating << ", RatingVotes=" << r.Votes;
							if (!isNew) {
								sstr << " WHERE ID=" << Scan->ID;
							}
							MYSQL_RES * res = conx.Query(sstr.str().c_str());
							conx.FreeResult(res);
						} else {
							std::stringstream sstr;
							if (isNew) {
								sstr << "REPLACE INTO ";
							} else {
								sstr << "UPDATE ";
							}
							sstr << qm_config.mysql_table << " SET ID=" << Scan->ID << ", Seen=1";
							SONG_RATING r = {0,0};
							api->botapi->ss->GetSongRatingByID(Scan->ID, &r);
							sstr << ", Rating=" << r.Rating << ", RatingVotes=" << r.Votes;
							if (!isNew) {
								sstr << " WHERE ID=" << Scan->ID;
							}
							MYSQL_RES * res = conx.Query(sstr.str().c_str());
							conx.FreeResult(res);
						}
						api->FreeQueue(Scan);
					}
				} else if (buf[0] != '.') {
		#ifdef WIN32
					snprintf(buf2, sizeof(buf2), "%s%s\\",path,buf);
		#else
					snprintf(buf2, sizeof(buf2), "%s%s/",path,buf);
		#endif
					scanMutex.Lock();
					dirList.push_back(buf2);
					scanMutex.Release();
					//ScanDirectory(buf2, num_files);
				}
			}
			delete dir;
			api->free(path);
		} else {
			scanMutex.Release();
			hst->done = true;
		}
	}

	//hst->done = true;
	TT_THREAD_END
	//DRIFT_DIGITAL_SIGNATURE();
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

int QM_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
	if (!stricmp(command, "autodj-clear")) {
		ut->std_reply(ut, "Clearing SimpleDJ Meta Cache...");
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
		ut->std_reply(ut, "Operation completed, your playlist is cleared. Make sure you do an !autodj-reload if you want me to keep playing songs...");
		return 1;
	}
	if (!stricmp(command, "autodj-qstat")) {
		ut->std_reply(ut, "=== MySQL Queue Status ===");
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
		api->botapi->textfunc->duration(len, buf2);
		sprintf(buf, "Songs in DB: %u seen, %u old/missing", seen, unseen);
		ut->std_reply(ut, buf);
		sprintf(buf, "Length of Songs in DB: %s (est., not all file types can accurately be determined)", buf2);
		ut->std_reply(ut, buf);
		if (unseen > 10 && (ut->Flags & (UFLAG_MASTER|UFLAG_OP))) {
			ut->std_reply(ut, "* Note: if you use !autodj-chroot often it is normal to have a lot of old/missing songs. You can also use !autodj-clear to manually clean the database.");
		}
		return 1;
	}
	return 0;
}


bool init(SD_PLUGIN_API * pApi) {
	api = pApi;
	memset(&qm_config, 0, sizeof(qm_config));

	DS_CONFIG_SECTION * sec = api->botapi->config->GetConfigSection(NULL, "SimpleDJ");
	if (sec == NULL) {
		api->botapi->ib_printf("SimpleDJ -> No 'SimpleDJ' section found in your config!\n");
		return false;
	}

	strcpy(qm_config.mysql_host, "localhost");
	strcpy(qm_config.mysql_user, "root");
	strcpy(qm_config.mysql_db, "IRCBot");
	strcpy(qm_config.mysql_table, "SimpleDJ");

	sec = api->botapi->config->GetConfigSection(sec, "Queue_MySQL");
	if (sec) {
		api->botapi->config->GetConfigSectionValueBuf(sec, "Host", qm_config.mysql_host, sizeof(qm_config.mysql_host));
		api->botapi->config->GetConfigSectionValueBuf(sec, "User", qm_config.mysql_user, sizeof(qm_config.mysql_user));
		api->botapi->config->GetConfigSectionValueBuf(sec, "Pass", qm_config.mysql_pass, sizeof(qm_config.mysql_pass));
		api->botapi->config->GetConfigSectionValueBuf(sec, "DBName", qm_config.mysql_db, sizeof(qm_config.mysql_db));
		api->botapi->config->GetConfigSectionValueBuf(sec, "DBTable", qm_config.mysql_table, sizeof(qm_config.mysql_table));
		if (api->botapi->config->GetConfigSectionLong(sec, "Port") > 0) {
			qm_config.mysql_port = (uint16)api->botapi->config->GetConfigSectionLong(sec, "Port");
		}
		if (api->botapi->config->GetConfigSectionLong(sec, "KeepHistory") > 0) {
			qm_config.keep_history = api->botapi->config->GetConfigSectionLong(sec, "KeepHistory");
		}
	} else {
		api->botapi->ib_printf("SimpleDJ -> No 'Queue_MySQL' section found in your SimpleDJ section, using pure defaults\n");
	}

	bool connected = false;
	for (int tries=0; tries < 5 && !connected; tries++) {
		api->botapi->ib_printf("SimpleDJ (queue_mysql) -> Connecting to MySQL server...\n");
		if (conx.Connect(qm_config.mysql_host, qm_config.mysql_user, qm_config.mysql_pass, qm_config.mysql_db, qm_config.mysql_port)) {
			connected = true;
		} else {
			api->botapi->ib_printf("SimpleDJ (queue_mysql) -> Waiting 2 second for another attempt...\n");
			api->botapi->safe_sleep_seconds(2);
		}
	}
	if (!connected) { return false; }

	COMMAND_ACL acl;

	api->botapi->commands->FillACL(&acl, 0, UFLAG_MASTER|UFLAG_OP, 0);
	api->botapi->commands->RegisterCommand_Proc(api->GetPluginNum(), "autodj-clear", &acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE, QM_Commands, "This command will empty and reset SimpleDJ's Meta Cache and Playlist");
	api->botapi->commands->FillACL(&acl, 0, UFLAG_ADVANCED_SOURCE, 0);
	api->botapi->commands->RegisterCommand_Proc(api->GetPluginNum(), "autodj-qstat", &acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE, QM_Commands, "This command display some information about your song queue");

	return true;
}

void quit() {
	api->botapi->commands->UnregisterCommandByName( "autodj-clear" );
	api->botapi->commands->UnregisterCommandByName( "autodj-qstat" );
}

void on_song_play(QUEUE * Scan) {
	std::stringstream sstr;
	sstr << "UPDATE " << qm_config.mysql_table << " SET LastPlayed=" << time(NULL) << ", PlayCount=PlayCount+1, IsPlaying=1 WHERE ID=" << Scan->ID;
	MYSQL_RES * res = conx.Query(sstr.str().c_str());
	conx.FreeResult(res);
}
void on_song_play_over(QUEUE * Scan) {
	std::stringstream sstr;
	sstr << "UPDATE " << qm_config.mysql_table << " SET IsPlaying=0 WHERE ID=" << Scan->ID;
	MYSQL_RES * res = conx.Query(sstr.str().c_str());
	conx.FreeResult(res);
}
void on_song_request(QUEUE * Scan) {
	std::stringstream sstr;
	sstr << "UPDATE " << qm_config.mysql_table << " SET LastReq=" << time(NULL) << ", ReqCount=ReqCount+1 WHERE ID=" << Scan->ID;
	MYSQL_RES * res = conx.Query(sstr.str().c_str());
	conx.FreeResult(res);

	if (qm_config.keep_history) {
		std::stringstream sstr;
		sstr << "INSERT INTO `" << qm_config.mysql_table << "_RequestHistory` SET `SongID`=" << Scan->ID << ", `TimeStamp`=" << time(NULL);
		res = conx.Query(sstr.str().c_str());
		conx.FreeResult(res);
		std::stringstream sstr2;
		sstr2 << "DELETE FROM `" << qm_config.mysql_table << "_RequestHistory` WHERE `TimeStamp`<" << (time(NULL) - (qm_config.keep_history * 86400));
		res = conx.Query(sstr2.str().c_str());
		conx.FreeResult(res);
	}
}
void on_song_rating(uint32 id, uint32 rating, uint32 votes) {
	std::stringstream sstr;
	sstr << "UPDATE " << qm_config.mysql_table << " SET Rating=" << rating << ", RatingVotes=" << votes << " WHERE ID=" << id;
	MYSQL_RES * res = conx.Query(sstr.str().c_str());
	conx.FreeResult(res);
}

SD_QUEUE_PLUGIN plugin = {
	SD_PLUGIN_VERSION,
	"MySQL Queue",

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

PLUGIN_EXPORT SD_QUEUE_PLUGIN * GetSDQPlugin() { return &plugin; }

