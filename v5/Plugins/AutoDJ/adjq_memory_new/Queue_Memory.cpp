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
#undef zmalloc
#undef zstrdup
#undef zrealloc
#undef znew
#undef zfreenn
#undef zfree
#include "Queue_Memory.h"
#include "meta_cache.h"
#include <assert.h>
#ifndef WIN32
#define _S_IFDIR S_IFDIR
#endif

AD_PLUGIN_API * api=NULL;
typedef std::vector<QUEUE *> songList;
songList songs;

void FreePlayList() {
	if (api->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		api->botapi->ib_printf(_("AutoDJ -> QueueMutex should be locked when calling FreePlayList!\n"));
	}

	for (songList::iterator x = songs.begin(); x != songs.end(); x++) {
		api->FreeQueue(*x);
	}
	songs.clear();
}

QUEUE_FIND_RESULT * FindWild(const char * pat) {
	if (api->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		api->botapi->ib_printf(_("AutoDJ -> QueueMutex should be locked when calling FindWild!\n"));
	}

	QUEUE_FIND_RESULT * ret = (QUEUE_FIND_RESULT *)api->malloc(sizeof(QUEUE_FIND_RESULT));
	memset(ret,0,sizeof(QUEUE_FIND_RESULT));

	for (songList::const_iterator Scan = songs.begin(); Scan != songs.end(); Scan++) {
		if (wildicmp(pat,(*Scan)->fn) || ((*Scan)->meta && (wildicmp(pat,(*Scan)->meta->title) || wildicmp(pat,(*Scan)->meta->artist) || wildicmp(pat,(*Scan)->meta->album)))) {
			if (ret->num == MAX_RESULTS) {
				ret->not_enough = true;
				break;
			}
			ret->results[ret->num] = api->CopyQueue(*Scan);
			ret->num++;
		}
	}

	return ret;
}

QUEUE_FIND_RESULT * FindByMeta(const char * pat, META_TYPES type) {
	if (api->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		api->botapi->ib_printf(_("AutoDJ -> QueueMutex should be locked when calling FindByMeta!\n"));
	}
	QUEUE_FIND_RESULT * ret = (QUEUE_FIND_RESULT *)api->malloc(sizeof(QUEUE_FIND_RESULT));
	memset(ret,0,sizeof(QUEUE_FIND_RESULT));

	for (songList::const_iterator Scan = songs.begin(); Scan != songs.end(); Scan++) {
		if ((*Scan)->meta) {
			char * str = NULL;
			switch(type) {
				case META_FILENAME:
					str = (*Scan)->fn;
					break;
				case META_ARTIST:
					str = (*Scan)->meta->artist;
					break;
				case META_ALBUM:
					str = (*Scan)->meta->album;
					break;
				case META_GENRE:
					str = (*Scan)->meta->genre;
					break;
				case META_TITLE:
				default:
					str = (*Scan)->meta->title;
					break;
			}
			if (wildicmp_multi(pat,str,"|,")) {
				if (ret->num == MAX_RESULTS) {
					ret->not_enough = true;
					break;
				}
				ret->results[ret->num] = api->CopyQueue(*Scan);
				ret->num++;
			}
		}
	}
	return ret;
}

void FreeFindResult(QUEUE_FIND_RESULT * qRes) {
	if (api->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		api->botapi->ib_printf(_("AutoDJ -> QueueMutex should be locked when calling FreeFindResult!\n"));
	}
	for (int i=0; i < qRes->num; i++) {
		api->FreeQueue(qRes->results[i]);
	}
	api->free(qRes);
}

QUEUE * FindFile(const char * fn) {
	if (api->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		api->botapi->ib_printf(_("AutoDJ -> QueueMutex should be locked when calling FindFile!\n"));
	}
	for (songList::const_iterator Scan = songs.begin(); Scan != songs.end(); Scan++) {
		if (!stricmp(fn,(*Scan)->fn)) {
			return api->CopyQueue(*Scan);
		}
	}
	return NULL;
}

QUEUE * GetRandomFile() {
	if (api->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		api->botapi->ib_printf(_("AutoDJ -> QueueMutex should be locked when calling GetRandomFile!\n"));
	}
	if (songs.size() == 0) { return NULL; }

	//char buf[4096];

	char pat[1024]="";
	time_t tme = time(NULL);
	if (api->getActiveFilter() && tme < api->getActiveFilter()->extra) {
		sstrcpy(pat, api->getActiveFilter()->pattern);
		api->ProcText(pat, tme, sizeof(pat));
	}

	int tries = songs.size()+1;
	songList::iterator Scan = songs.begin();

	int go = (AutoDJ_Rand()%songs.size());
	while (go--) {
		Scan++;
		if (Scan == songs.end()) { Scan = songs.begin(); }
	}

	while (tries-- > 0) {

		if (tries == 0) {
			if (api->getNoRepeat() != 0) {
				api->botapi->ib_printf(_("AutoDJ -> Unsetting NoRepeat, hopefully that will solve your problem...\n"));
				api->setNoRepeat(0);
				tries = songs.size()+1;
			} else if (api->getNoRepeatArtist() != 0) {
				api->botapi->ib_printf(_("AutoDJ -> Unsetting NoRepeatArtist, hopefully that will solve your problem...\n"));
				api->setNoRepeatArtist(0);
				tries = songs.size()+1;
			} else {
				api->botapi->ib_printf(_("AutoDJ -> FATAL ERROR: Can not find any files to play, but GetRandomFile() was called!!!\n"));
				return NULL;
			}
		}

		if (api->AllowSongPlayback(*Scan, tme, pat)) {
			//success
			break;
		}

		if (Scan == songs.end()) { Scan = songs.begin(); }
	}

	(*Scan)->LastPlayed = time(NULL);
	return api->CopyQueue(*Scan);
}

typedef std::multimap<string, QUEUE *, ci_less> sortList;
void SortQueue() {
	if (api->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		api->botapi->ib_printf(_("AutoDJ -> QueueMutex should be locked when calling SortQueue!\n"));
	}

	sortList sort;
#if defined(WIN32)
	uint32 ticks = GetTickCount();
#endif
	for (songList::iterator Scan = songs.begin(); songs.size() > 0; Scan = songs.begin()) {
		sort.insert(pair<string, QUEUE *>((*Scan)->fn, *Scan));
		songs.erase(Scan);
	}
	for (sortList::iterator Scan = sort.begin(); sort.size() > 0; Scan = sort.begin()) {
		songs.push_back(Scan->second);
		sort.erase(Scan);
	}
#if defined(WIN32)
	api->botapi->ib_printf(_("AutoDJ -> Sort completed in %u ticks\n"), GetTickCount() - ticks);
#else
	api->botapi->ib_printf(_("AutoDJ -> Sort completed!\n"));
#endif
}

bool ProcFile(Titus_Mutex * scanMutex, const char * path, const char * fn, const char * ffn, struct stat st);
bool PostProc(READER_HANDLE * fOutList) {
	if (songs.size() > 1) {
		api->botapi->ib_printf(_("AutoDJ -> Sorting files...\n"));
		SortQueue();
	}

	if (fOutList) {
		char buf[1024];
		char last_char = 0;
		for (songList::const_iterator Scan = songs.begin(); Scan != songs.end(); Scan++) {
			if (toupper((*Scan)->fn[0]) != last_char && strlen(api->FileLister->NewChar)) {
				sprintf(buf,"%s\r\n",api->FileLister->NewChar);
				char buf2[2] = { toupper((*Scan)->fn[0]), 0 };
				last_char = toupper((*Scan)->fn[0]);
				str_replace(buf,sizeof(buf),"%char%", buf2);
				fOutList->write(buf,strlen(buf),1,fOutList);
			}
			sprintf(buf,"%s\r\n",api->FileLister->Line);
			str_replace(buf,sizeof(buf),"%file%",(*Scan)->fn);
			fOutList->write(buf,strlen(buf),1,fOutList);
		}
	}

	return true;
}

int32 QueueContentFiles() {
//	char buf[1024];

	LockMutexPtr(api->getQueueMutex());
	FreePlayList();

	LoadMetaCache();

	api->DoFileScan(ProcFile, PostProc);

	SaveMetaCache();
	FreeMetaCache();

	RelMutexPtr(api->getQueueMutex());
	return songs.size();
}

bool ProcFile(Titus_Mutex * scanMutex, const char * path, const char * fn, const char * ffn, struct stat st) {
	bool ret = false;
	DECODER * dec = api->GetFileDecoder(ffn);
	if (dec) {
		QUEUE * Scan = (QUEUE *)api->malloc(sizeof(QUEUE));
		memset(Scan,0,sizeof(QUEUE));

		ret = true;
		size_t len = strlen(fn)+1;
		Scan->fn = (char *)api->malloc(len);
		strcpy(Scan->fn, fn);
		len = strlen(path)+1;
		Scan->path = (char *)api->malloc(len);
		strcpy(Scan->path, path);
		Scan->ID = api->FileID(ffn);

		TITLE_DATA * td = (TITLE_DATA *)api->malloc(sizeof(TITLE_DATA));
		memset(td, 0, sizeof(TITLE_DATA));

		LockMutexPtr(scanMutex);
		META_CACHE * meta = FindFileMeta(Scan->ID);
		RelMutexPtr(scanMutex);
		if (meta) {
			if (meta->mTime == st.st_mtime) {
				//printf("Using cached results for file ID: %u\n", id);
				meta->used = 1;
				Scan->songlen = meta->songLen;
				memcpy(td, &meta->meta, sizeof(TITLE_DATA));
				if (strlen(td->title)) {
					Scan->meta = td;
				}
			}
		}

		if (!strlen(td->title) && !meta) {
#if defined(WIN32)
			__try {
				//api->botapi->ib_printf("File: %s\n", buf);
				dec->get_title_data(ffn, td, &Scan->songlen);
				//api->botapi->ib_printf("Scanned\n");
			} __except(EXCEPTION_EXECUTE_HANDLER) {
				api->botapi->ib_printf(_("AutoDJ -> Exception occured while trying to process %s\n"), fn);
				Scan->songlen = 0;
				memset(td, 0, sizeof(TITLE_DATA));
			}
#else
			try {
				//api->botapi->ib_printf("File: %s\n", buf);
				dec->get_title_data(ffn, td, &Scan->songlen);
				//api->botapi->ib_printf("Scanned\n");
			} catch(...) {
				api->botapi->ib_printf(_("AutoDJ -> Exception occured while trying to process %s\n"), fn);
			}
#endif
			strtrim(td->title);
			strtrim(td->album);
			strtrim(td->artist);
			strtrim(td->genre);
			LockMutexPtr(scanMutex);
			SetFileMeta(Scan->ID, st.st_mtime, Scan->songlen, td);
			RelMutexPtr(scanMutex);
			if (ret && strlen(td->title)) {
				Scan->meta = td;
				ASCII_Remap((unsigned char *)Scan->meta->title);
				ASCII_Remap((unsigned char *)Scan->meta->artist);
				ASCII_Remap((unsigned char *)Scan->meta->album);
				ASCII_Remap((unsigned char *)Scan->meta->genre);
				if (strstr(Scan->meta->artist, "Rat Sou")) {
					api->botapi->ib_printf("%s - %s\n", td->artist, td->title);
				}
			}
		}

		if (!strlen(td->title)) {
			Scan->meta = NULL;
			api->free(td);
			//delete td;
		}

		LockMutexPtr(scanMutex);
		songs.push_back(Scan);
		RelMutexPtr(scanMutex);
	}
	return ret;
}

int QM_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {

	if (!stricmp(command, "autodj-clear")) {
		ut->std_reply(ut, _("Clearing AutoDJ Meta Cache..."));
		api->botapi->db->Query("DROP TABLE IF EXISTS autodj_cache", NULL, NULL, NULL);
		ut->std_reply(ut, _("Operation completed"));
		return 1;
	}

	return 0;
}


bool init(AD_PLUGIN_API * pApi) {
	api = pApi;
	api->botapi->commands->RegisterCommand_Proc("autodj-clear",	    2, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE					, QM_Commands, _("This command will empty and reset AutoDJ's Meta Cache"));
	return true;
}

void quit() {
	api->botapi->commands->UnregisterCommandByName( "autodj-clear" );
}

AD_QUEUE_PLUGIN plugin = {
	AD_PLUGIN_VERSION,
	"New Memory Queue",

	init,
	quit,
	NULL,
	NULL,
	NULL,
	NULL,

	QueueContentFiles,
	FreePlayList,

	FindWild,
	FindByMeta,
	FreeFindResult,

	FindFile,
	GetRandomFile,

	NULL
};

PLUGIN_EXPORT AD_QUEUE_PLUGIN * GetADQPlugin() { return &plugin; }
