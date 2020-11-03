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

#include "Queue_Memory.h"
#include <assert.h>
#ifndef WIN32
#define _S_IFDIR S_IFDIR
#endif

#define DEBUG_MEM
#if defined(DEBUG_MEM)
#define qm_malloc adapi->malloc
//#define qm_realloc adapi->realloc
#define qm_free adapi->free
#else
void * newalloc(size_t size) {
	return new char[size];
}
void newfree(void * ptr) {
	delete ptr;
}
#define qm_malloc newalloc
//#define qm_realloc dsl_realloc
#define qm_free newfree
#endif

AD_PLUGIN_API * adapi=NULL;
QUEUE *qFirst[MAX_PLAYLISTS],*qLast[MAX_PLAYLISTS]; // internal list of all files
int32 nofiles_in_pl[MAX_PLAYLISTS];
int32 nofiles=0;

void FreePlayList() {
	AutoMutexPtr(adapi->getQueueMutex());

	for (int i=0; i < MAX_PLAYLISTS; i++) {
		QUEUE * Scan = qFirst[i];
		QUEUE * toDel = NULL;
		while (Scan) {
			//DRIFT_DIGITAL_SIGNATURE();
			toDel = Scan;
			Scan=Scan->Next;
			qm_free(toDel->fn);
			qm_free(toDel->path);
			if (toDel->isreq) {
				qm_free(toDel->isreq);
			}
			if (toDel->meta) {
				qm_free(toDel->meta);
			}
			qm_free(toDel);
			//adapi->FreeQueue(toDel);
		}
		nofiles_in_pl[i] = 0;
		qFirst[i]=NULL;
		qLast[i]=NULL;
	}
}

QUEUE_FIND_RESULT * FindWild(const char * pat) {
	AutoMutexPtr(adapi->getQueueMutex());

	QUEUE_FIND_RESULT * ret = (QUEUE_FIND_RESULT *)qm_malloc(sizeof(QUEUE_FIND_RESULT));
	memset(ret,0,sizeof(QUEUE_FIND_RESULT));

	for (int i=0; i < adapi->getNumPlaylists(); i++) {
#if defined(DEBUG)
		CheckQueue(&qFirst[i], &qLast[i]);
#endif
		QUEUE * Scan = qFirst[i];
		while (Scan) {
			if (wildicmp(pat,Scan->fn) || (Scan->meta && (wildicmp(pat,Scan->meta->title) || wildicmp(pat,Scan->meta->artist) || wildicmp(pat,Scan->meta->album)))) {
				if (ret->num == MAX_RESULTS) {
					ret->not_enough = true;
					break;
				}
				ret->results[ret->num] = adapi->CopyQueue(Scan);
				ret->num++;
			}
			Scan = Scan->Next;
		}
	}

	return ret;
}

QUEUE_FIND_RESULT * FindByMeta(const char * pat, META_TYPES type) {
	AutoMutexPtr(adapi->getQueueMutex());

	QUEUE_FIND_RESULT * ret = (QUEUE_FIND_RESULT *)qm_malloc(sizeof(QUEUE_FIND_RESULT));
	memset(ret,0,sizeof(QUEUE_FIND_RESULT));

	for (int i=0; i < adapi->getNumPlaylists(); i++) {
#if defined(DEBUG)
		CheckQueue(&qFirst[i], &qLast[i]);
#endif
		QUEUE * Scan = qFirst[i];
		while (Scan) {
			char * str = NULL;
			switch(type) {
				case META_FILENAME:
					str = Scan->fn;
					break;
				case META_DIRECTORY:
					str = Scan->path;
					break;
				case META_ARTIST:
					str = (Scan->meta != NULL) ? Scan->meta->artist : NULL;
					break;
				case META_ALBUM:
					str = (Scan->meta != NULL) ? Scan->meta->album : NULL;
					break;
				case META_GENRE:
					str = (Scan->meta != NULL) ? Scan->meta->genre : NULL;
					break;
				case META_TITLE:
					str = (Scan->meta != NULL) ? Scan->meta->title : NULL;
					break;
			}
			if (str == NULL) {
				str = Scan->meta ? Scan->meta->title:Scan->fn;
			}
			if (wildicmp_multi(pat,str,"|,")) {
				if (ret->num == MAX_RESULTS) {
					ret->not_enough = true;
					return ret;
				}
				ret->results[ret->num] = adapi->CopyQueue(Scan);
				ret->num++;
			}
			Scan = Scan->Next;
		}
	}
	return ret;
}

void FreeFindResult(QUEUE_FIND_RESULT * qRes) {
	if (adapi->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		adapi->botapi->ib_printf(_("AutoDJ -> QueueMutex should be locked when calling FreeFindResult!\n"));
	}
	for (int i=0; i < qRes->num; i++) {
		adapi->FreeQueue(qRes->results[i]);
	}
	qm_free(qRes);
}

QUEUE * FindFile(const char * fn) {
	AutoMutexPtr(adapi->getQueueMutex());
	for (int i=0; i < adapi->getNumPlaylists(); i++) {
#if defined(DEBUG)
		CheckQueue(&qFirst[i], &qLast[i]);
#endif
		QUEUE * Scan=qFirst[i];
		while (Scan != NULL) {
			if (!stricmp(fn,Scan->fn)) {
				return adapi->CopyQueue(Scan);
			}
			Scan=Scan->Next;
		}
	}
	return NULL;
}

QUEUE * FindFileByID(uint32 id) {
	AutoMutexPtr(adapi->getQueueMutex());
	for (int i=0; i < adapi->getNumPlaylists(); i++) {
#if defined(DEBUG)
		CheckQueue(&qFirst[i], &qLast[i]);
#endif
		QUEUE * Scan=qFirst[i];
		while (Scan != NULL) {
			if (Scan->ID == id) {
				return adapi->CopyQueue(Scan);
			}
			Scan=Scan->Next;
		}
	}
	return NULL;
}

QUEUE * GetRandomFile(int playlist) {
	AutoMutexPtr(adapi->getQueueMutex());

	if (playlist < 0 || playlist >= adapi->getNumPlaylists()) {
		adapi->botapi->ib_printf(_("AutoDJ -> Invalid playlist number!!! (%d)\n"), playlist);
		return NULL;
	}
	if (nofiles_in_pl[playlist] == 0) {
		adapi->botapi->ib_printf(_("AutoDJ -> No files in playlist number %d !\n"), playlist);
		return NULL;
	}

	//char buf[4096];

	char pat[1024]="";
	time_t tme = time(NULL);
	TIMER * tmr = adapi->getActiveFilter();
	if (tmr && tme < tmr->extra) {
		sstrcpy(pat, tmr->pattern);
		adapi->ProcText(pat, tme, sizeof(pat));
	}

	int tries = nofiles_in_pl[playlist]+1;
	QUEUE *Scan = qFirst[playlist];

	int go = adapi->botapi->genrand_range(0, nofiles_in_pl[playlist] - 1);
	while (go-- && Scan) {
		Scan=Scan->Next;
		if (Scan == NULL) { Scan=qFirst[playlist]; } // loop about
	}

	while (tries-- > 0) {

		if (tries == 0) {
			if (adapi->getNoRepeatArtist() != 0) {
				adapi->botapi->ib_printf(_("AutoDJ -> Unsetting NoRepeatArtist, hopefully that will solve your problem...\n"));
				adapi->setNoRepeatArtist(0);
				tries = nofiles_in_pl[playlist]+1;
			} else if (adapi->getNoRepeat() != 0) {
				adapi->botapi->ib_printf(_("AutoDJ -> Unsetting NoRepeat, hopefully that will solve your problem...\n"));
				adapi->setNoRepeat(0);
				tries = nofiles_in_pl[playlist]+1;
			} else if (adapi->getMaxSongDuration() != 0) {
				adapi->botapi->ib_printf(_("AutoDJ -> Unsetting MaxSongDuration, hopefully that will solve your problem...\n"));
				adapi->setMaxSongDuration(0);
				tries = nofiles_in_pl[playlist]+1;
			} else if (adapi->getActiveFilter() != NULL) {
				adapi->botapi->ib_printf(_("AutoDJ -> Unsetting active filter, hopefully that will solve your problem...\n"));
				adapi->removeActiveFilter();
				tries = nofiles_in_pl[playlist] + 1;
			} else {
				adapi->botapi->ib_printf(_("AutoDJ -> FATAL ERROR: Can not find any files to play, but GetRandomFile() was called!!!\n"));
				return NULL;
			}
		}

		if (adapi->AllowSongPlayback(Scan, tme, pat)) {
			//success
			break;
		}


		Scan=Scan->Next;
		if (Scan == NULL) { Scan=qFirst[playlist]; } // loop about
	}

	Scan->lastPlayed = time(NULL);
	return adapi->CopyQueue(Scan);
}

inline int SortCmp(QUEUE * q1, QUEUE * q2) {
	if (!stricmp(q1->fn, q2->fn)) {
		return 0;
	}

	int len1 = strlen(q1->fn);
	int len2 = strlen(q2->fn);
	int len = (len1 > len2) ? len2:len1;
	for (int i=0; i < len; i++) {
		if (tolower(q1->fn[i]) > tolower(q2->fn[i])) {
			return 1;
		}
		if (tolower(q1->fn[i]) < tolower(q2->fn[i])) {
			return -1;
		}
	}
	return (len1 > len2) ? 1:0;
}

void SortQueue(QUEUE ** qqFirst, QUEUE ** qqLast, int playlist) {
	AutoMutexPtr(adapi->getQueueMutex());
	int64 ticks = adapi->botapi->get_cycles();
	if (*qqFirst == NULL) { return; }
	//MergeSort<QUEUE> sorter;
	*qqFirst = SortLinkedList(*qqFirst, SortCmp);
	//*qqFirst = MergeSortLinkedList(*qqFirst, SortCmp);
	QUEUE * Scan = *qqFirst;
	while (Scan->Next) { Scan = Scan->Next; }
	*qqLast = Scan;
	/*
//	return;
	QUEUE *fQue = *qqFirst;
	QUEUE *lQue = *qqLast;
	bool had_one = true;
	while (had_one) {
#if defined(WIN32)
		loops++;
#endif
		QUEUE *Scan = fQue;
		had_one = false;
		while (Scan && Scan->Next) {
			if (SortCmp(Scan, Scan->Next) > 0) {
				had_one = true;
				if (Scan == fQue) {
					if (Scan->Next == lQue) {
						lQue = Scan;
						fQue = Scan->Next;
						fQue->Prev = NULL;
						fQue->Next = lQue;
						lQue->Prev = fQue;
						lQue->Next = NULL;
					} else {
						QUEUE * a = Scan;
						QUEUE * b = Scan->Next;

						a->Next = b->Next;
						if (b->Next) {
							b->Next->Prev = a;
						}
						a->Prev = b;

						fQue = b;
						fQue->Prev = NULL;
						fQue->Next = a;

						Scan = fQue;
					}
				} else if (Scan->Next == lQue) {
					QUEUE * a = Scan;
					QUEUE * b = Scan->Next;

					a->Next = NULL;
					lQue = a;
					if (a->Prev) {
						a->Prev->Next = b;
						b->Prev = a->Prev;
					} else {
						fQue = b;
						b->Prev = NULL;
					}
					a->Prev = b;
					b->Next = a;
					Scan = (b->Prev) ? b->Prev:b;
				} else {
					//printf("Mid\n");
					QUEUE * b = Scan->Prev;
					QUEUE * a = Scan->Next->Next;
					QUEUE * m1 = Scan;
					QUEUE * m2 = Scan->Next;

					b->Next = m2;
					a->Prev = m1;

					m2->Next = m1;
					m2->Prev = b;

					m1->Prev = m2;
					m1->Next = a;

					Scan = b;
				}
			} else {
				Scan = Scan->Next;
			}
		}
	}
	*qqFirst = fQue;
	*qqLast = lQue;
	*/
	adapi->botapi->ib_printf(_("AutoDJ -> Sorted playlist %d in " I64FMT " ms\n"), playlist, adapi->botapi->get_cycles() - ticks);
}

void CheckQueue(QUEUE ** qqFirst, QUEUE ** qqLast) {
	AutoMutexPtr(adapi->getQueueMutex());
	QUEUE *fQue = *qqFirst;
	QUEUE *lQue = *qqLast;

	QUEUE * oScan = NULL;
	QUEUE * Scan = fQue;
	int n = 0;
	while (Scan) {
		n++;

		if (Scan->Prev != oScan) {
			adapi->botapi->ib_printf(_("WARNING: Queue chain is broken at position %d\n"), n);
		}
		assert(Scan->Prev == oScan);

		oScan = Scan;
		Scan = Scan->Next;
	}

	if (oScan != lQue) {
		adapi->botapi->ib_printf(_("WARNING: Queue chain does not reach lQue! (Got to pos: %d)\n"), n);
	}
	/*
	if (n != nofiles) {
		adapi->botapi->ib_printf(_("WARNING: Queue chain does not have all %d files! (Has %d files)\n"), nofiles, n);
	}
	*/
}

bool ProcFile(Titus_Mutex * scanMutex, const char * path, const char * fn, const char * ffn, int playlist, void * uptr);//, struct stat st
bool PostProc(READER_HANDLE * fOutList) {
	adapi->botapi->ib_printf(_("AutoDJ -> Sorting files...\n"));
	for (int i=0; i < adapi->getNumPlaylists(); i++) {
		CheckQueue(&qFirst[i], &qLast[i]);
		if (qFirst[i] != qLast[i]) {
			SortQueue(&qFirst[i], &qLast[i], i);
		}
		CheckQueue(&qFirst[i], &qLast[i]);
	}
	adapi->botapi->ib_printf(_("AutoDJ -> Sorting complete!\n"));

	if (fOutList) {
		adapi->botapi->ib_printf(_("AutoDJ -> Writing playlist...\n"));
		char buf[1024];
		char last_char = 0;
		for (int i = 0; i < adapi->getNumPlaylists(); i++) {
			QUEUE * Scan = qFirst[i];
			while (Scan) {
				if (toupper(Scan->fn[0]) != last_char && strlen(adapi->FileLister->NewChar)) {
					sprintf(buf,"%s\r\n",adapi->FileLister->NewChar);
					char buf2[2] = { toupper(Scan->fn[0]), 0 };
					last_char = toupper(Scan->fn[0]);
					str_replace(buf,sizeof(buf),"%char%", buf2);
					fOutList->write(buf,strlen(buf),fOutList);
				}
				snprintf(buf,sizeof(buf),"%s\r\n",adapi->FileLister->Line);
				str_replace(buf,sizeof(buf),"%file%",Scan->fn);
				fOutList->write(buf,strlen(buf),fOutList);
				Scan = Scan->Next;
			}
		}
	}

	return true;
}

int32 QueueContentFiles() {
//	char buf[1024];

	LockMutexPtr(adapi->getQueueMutex());
	FreePlayList();

	MetaCache * mc = new MetaCache();

	nofiles = 0;
	nofiles = adapi->DoFileScan(ProcFile, PostProc, mc);

	delete mc;

	RelMutexPtr(adapi->getQueueMutex());
	return nofiles;
}

void AddToQueue(QUEUE * q, QUEUE ** qqFirst, QUEUE ** qqLast) {
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

bool ProcFile(Titus_Mutex * scanMutex, const char * path, const char * fn, const char * ffn, int playlist, void * uptr) {//, struct stat st) {
	MetaCache * mc = (MetaCache *)uptr;
	bool ret = false;
	DECODER * dec = adapi->GetFileDecoder(ffn);
	if (dec) {
		QUEUE * Scan = (QUEUE *)qm_malloc(sizeof(QUEUE));
		memset(Scan,0,sizeof(QUEUE));

		ret = true;
		size_t len = strlen(fn)+1;
		Scan->fn = (char *)qm_malloc(len);
		strlcpy(Scan->fn, fn, len);
		len = strlen(path)+1;
		Scan->path = (char *)qm_malloc(len);
		strlcpy(Scan->path, path, len);
		Scan->ID = adapi->FileID(ffn);

		TITLE_DATA * td = (TITLE_DATA *)qm_malloc(sizeof(TITLE_DATA));
		memset(td, 0, sizeof(TITLE_DATA));

		META_CACHE * meta = mc->FindFileMeta(Scan->ID);

		time_t song_mtime = 0;
		if (adapi->getOnlyScanNewFiles() && meta != NULL) {
			song_mtime = meta->mTime;
		} else {
			struct stat64 st;
			memset(&st, 0, sizeof(st));
			if (stat64(ffn, &st) == 0) {
				song_mtime = st.st_mtime;
			} else {
				adapi->botapi->ib_printf(_("AutoDJ -> Error stat()ing file: %s (%d / %s)\n"), ffn, errno, strerror(errno));
			}
		}

		if (meta) {
			if (meta->mTime == song_mtime) {
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
			adapi->ReadMetaDataEx(ffn, td, &Scan->songlen, dec);
			mc->SetFileMeta(Scan->ID, song_mtime, Scan->songlen, td);
			if (ret && strlen(td->title)) {
				Scan->meta = td;
				/*
				ASCII_Remap((unsigned char *)Scan->meta->title);
				ASCII_Remap((unsigned char *)Scan->meta->artist);
				ASCII_Remap((unsigned char *)Scan->meta->album);
				ASCII_Remap((unsigned char *)Scan->meta->genre);
				if (strstr(Scan->meta->artist, "Rat Sou")) {
					adapi->botapi->ib_printf("%s - %s\n", td->artist, td->title);
				}
				*/
			}
		}

		if (Scan->meta == NULL) {
			qm_free(td);
			Scan->meta = NULL;
			//delete td;
		}

		LockMutexPtr(scanMutex);
		AddToQueue(Scan, &qFirst[playlist], &qLast[playlist]);
		RelMutexPtr(scanMutex);
	}
	if (ret) {
		nofiles++;
		nofiles_in_pl[playlist]++;
	}
	return ret;
}

int QM_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {

	if (!stricmp(command, "autodj-clear")) {
		LockMutexPtr(adapi->getQueueMutex());
		ut->std_reply(ut, _("Clearing AutoDJ Meta Cache..."));
		if (remove("autodj.cache") == 0) {
			ut->std_reply(ut, _("Operation completed"));
		} else {
			char buf[128];
			snprintf(buf, sizeof(buf), _("Error clearing meta cache: %s"), strerror(errno));
			ut->std_reply(ut, buf);
		}
		RelMutexPtr(adapi->getQueueMutex());
		return 1;
	}

	return 0;
}


bool init(AD_PLUGIN_API * pApi) {
	adapi = pApi;
	COMMAND_ACL acl;
	for (int i=0; i < MAX_PLAYLISTS; i++) {
		nofiles_in_pl[i] = 0;
		qFirst[i]=NULL;
		qLast[i]=NULL;
	}
	adapi->botapi->commands->FillACL(&acl, 0, UFLAG_MASTER|UFLAG_OP, 0);
	adapi->botapi->commands->RegisterCommand_Proc(adapi->GetPluginNum(), "autodj-clear",	    &acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE, QM_Commands, _("This command will empty and reset AutoDJ's Meta Cache"));
	return true;
}

void quit() {
	adapi->botapi->commands->UnregisterCommandByName( "autodj-clear" );
}

AD_QUEUE_PLUGIN plugin = {
	AD_PLUGIN_VERSION,
	"Memory Queue",

	init,
	quit,
	NULL,
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
	FindFileByID,
	GetRandomFile,

	NULL
};

PLUGIN_EXPORT AD_QUEUE_PLUGIN * GetADQPlugin() { return &plugin; }
