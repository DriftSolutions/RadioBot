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
#define qm_malloc api->malloc
//#define qm_realloc api->realloc
#define qm_free api->free
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

SD_PLUGIN_API * api=NULL;
QUEUE *qFirst=NULL,*qLast=NULL; // internal list of all files
int32 nofiles=0;

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
	if (api->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		api->botapi->ib_printf("SimpleDJ -> QueueMutex should be locked when calling FreePlayList!\n");
	}

	QUEUE * Scan = qFirst;
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
		//api->FreeQueue(toDel);
	}
	qFirst=NULL;
	qLast=NULL;
}

QUEUE_FIND_RESULT * FindWild(const char * pat) {
	if (api->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		api->botapi->ib_printf("SimpleDJ -> QueueMutex should be locked when calling FindWild!\n");
	}

	QUEUE_FIND_RESULT * ret = (QUEUE_FIND_RESULT *)qm_malloc(sizeof(QUEUE_FIND_RESULT));
	memset(ret,0,sizeof(QUEUE_FIND_RESULT));

#if defined(DEBUG)
	CheckQueue(&qFirst, &qLast);
#endif
	QUEUE * Scan = qFirst;
	while (Scan) {
		if (wildicmp(pat,Scan->fn) || (Scan->meta && (wildicmp(pat,Scan->meta->title) || wildicmp(pat,Scan->meta->artist) || wildicmp(pat,Scan->meta->album)))) {
			if (ret->num == MAX_RESULTS) {
				ret->not_enough = true;
				break;
			}
			ret->results[ret->num] = api->CopyQueue(Scan);
			ret->num++;
		}
		Scan = Scan->Next;
	}

	return ret;
}

QUEUE_FIND_RESULT * FindByMeta(const char * pat, META_TYPES type) {
	if (api->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		api->botapi->ib_printf("SimpleDJ -> QueueMutex should be locked when calling FindByMeta!\n");
	}
	QUEUE_FIND_RESULT * ret = (QUEUE_FIND_RESULT *)qm_malloc(sizeof(QUEUE_FIND_RESULT));
	memset(ret,0,sizeof(QUEUE_FIND_RESULT));

#if defined(DEBUG)
	CheckQueue(&qFirst, &qLast);
#endif
	QUEUE * Scan = qFirst;
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
				break;
			}
			ret->results[ret->num] = api->CopyQueue(Scan);
			ret->num++;
		}
		Scan = Scan->Next;
	}
	return ret;
}

void FreeFindResult(QUEUE_FIND_RESULT * qRes) {
	if (api->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		api->botapi->ib_printf("SimpleDJ -> QueueMutex should be locked when calling FreeFindResult!\n");
	}
	for (int i=0; i < qRes->num; i++) {
		api->FreeQueue(qRes->results[i]);
	}
	qm_free(qRes);
}

QUEUE * FindFile(const char * fn) {
	if (api->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		api->botapi->ib_printf("SimpleDJ -> QueueMutex should be locked when calling FindFile!\n");
	}
#if defined(DEBUG)
	CheckQueue(&qFirst, &qLast);
#endif
	QUEUE * Scan=qFirst;
	while (Scan != NULL) {
		if (!stricmp(fn,Scan->fn)) {
			return api->CopyQueue(Scan);
		}
		Scan=Scan->Next;
	}
	return NULL;
}

QUEUE * FindFileByID(uint32 id) {
	if (api->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		api->botapi->ib_printf("SimpleDJ -> QueueMutex should be locked when calling FindFile!\n");
	}
#if defined(DEBUG)
	CheckQueue(&qFirst, &qLast);
#endif
	QUEUE * Scan=qFirst;
	while (Scan != NULL) {
		if (Scan->ID == id) {
			return api->CopyQueue(Scan);
		}
		Scan=Scan->Next;
	}
	return NULL;
}

QUEUE * GetRandomFile() {
	if (api->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		api->botapi->ib_printf("SimpleDJ -> QueueMutex should be locked when calling GetRandomFile!\n");
	}

	char buf[4096];

	int tries = 0;
	QUEUE *Scan = NULL;
	while (!Scan && tries < 40) {
		tries++;

		if (tries == 30) {
			if (api->getNoRepeat() != 0) {
				api->botapi->ib_printf("SimpleDJ -> Unsetting NoRepeat, hopefully that will solve your problem...\n");
				api->setNoRepeat(0);
				//GetADConfig()->Options.NoRepeat=0;
			}
		}

		int go = (SimpleDJ_Rand()%nofiles);

		Scan=qFirst;
		while (go-- && Scan) {
			Scan=Scan->Next;
			if (Scan == NULL) { Scan=qFirst; } // loop about
		}

		if (api->getActiveFilter() && time(NULL) < api->getActiveFilter()->extra) {

			QUEUE * Start = Scan;
			sstrcpy(buf, api->getActiveFilter()->pattern);
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

		} else {

			if (api->getActiveFilter()) {
				// the filter is expired if it exists here
				api->removeActiveFilter();
				//zfree(GetADConfig()->Filter);
				//GetADConfig()->Filter = NULL;
			}

			//NoPlayFilters do not apply when a filter is active
			sprintf(buf, "%s%s", Scan->path, Scan->fn);
			if (api->MatchesNoPlayFilter(buf)) {
				api->botapi->ib_printf("SimpleDJ -> %s%s blocked by NoPlayFilters...\n",Scan->path,Scan->fn);
				Scan = NULL;
				continue;
			}
		}

		if (api->getNoRepeat() > 0) {
			time_t lp = time(NULL) - Scan->LastPlayed;
			if (Scan->LastPlayed > 0 && lp < api->getNoRepeat()) {
				api->botapi->ib_printf("SimpleDJ -> Skipping Song: %s (last played %d secs ago, NoRepeat = %u)\n",Scan->fn,lp,api->getNoRepeat());
				Scan = NULL;
				continue;
			}
		}
	}

	if (!Scan) {
		api->botapi->ib_printf("SimpleDJ -> FATAL ERROR: Can not find any files to play, but GetRandomFile() was called!!!\n");
		return NULL;
	}

	Scan->LastPlayed = time(NULL);
	return api->CopyQueue(Scan);
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

void SortQueue(QUEUE ** qqFirst, QUEUE ** qqLast) {
	if (api->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		api->botapi->ib_printf("SimpleDJ -> QueueMutex should be locked when calling SortQueue!\n");
	}
#if defined(WIN32)
	uint32 ticks = GetTickCount();
	uint32 loops = 0;
#endif
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
			if (SortCmp(Scan, Scan->Next)) {
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
#if defined(WIN32)
	api->botapi->ib_printf(_("SimpleDJ -> Sort completed in %u ms\n"), GetTickCount() - ticks);
#endif
}

void CheckQueue(QUEUE ** qqFirst, QUEUE ** qqLast) {
	if (api->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		api->botapi->ib_printf("SimpleDJ -> QueueMutex should be locked when calling CheckQueue!\n");
	}
	QUEUE *fQue = *qqFirst;
	QUEUE *lQue = *qqLast;

	QUEUE * oScan = NULL;
	QUEUE * Scan = fQue;
	int n = 0;
	while (Scan) {
		n++;

		if (Scan->Prev != oScan) {
			api->botapi->ib_printf("WARNING: Queue chain is broken at position %d\n", n);
		}
		assert(Scan->Prev == oScan);

		oScan = Scan;
		Scan = Scan->Next;
	}

	if (oScan != lQue) {
		api->botapi->ib_printf(_("WARNING: Queue chain does not reach lQue! (Got to pos: %d)\n"), n);
	}
	if (n != nofiles) {
		api->botapi->ib_printf(_("WARNING: Queue chain does not have all %d files! (Has %d files)\n"), nofiles, n);
	}
}

int32 QueueContentFiles() {
	char buf[1024];

	if (!has_crc_init) {
		GenCRC32Table();
		has_crc_init = true;
	}

	LockMutexPtr(api->getQueueMutex());
	FreePlayList();
	nofiles=0;

	FILE *fOutList=NULL;
	if (strlen(api->FileLister->List) > 0) {
		fOutList=fopen(api->FileLister->List,"wb");
		if (strlen(api->FileLister->Header) && fOutList != NULL) {
			FILE *fOutHeader=fopen(api->FileLister->Header,"rb");
			if (fOutHeader) {
				fseek(fOutHeader,0,SEEK_END);
				long len=ftell(fOutHeader);
				fseek(fOutHeader,0,SEEK_SET);
				char *outfl_buf = (char *)qm_malloc(len);
				if (fread(outfl_buf,len,1,fOutHeader) == 1 && fwrite(outfl_buf,len,1,fOutList) == 1) {
					fflush(fOutList);
				} else {
					printf("Error reading/writing file lister header!\n");
				}
				qm_free(outfl_buf);
				fclose(fOutHeader);
			} else {
				fprintf(fOutList,"Error opening '%s'\r\n",api->FileLister->Header);
				api->botapi->ib_printf("SimpleDJ -> Error opening file lister header file!\n");
			}
		}
	}

	LoadMetaCache();
	api->botapi->ib_printf("SimpleDJ -> Beginning file scan...\n");
	time_t scan_time = time(NULL);
	char * tmp = api->strdup(api->getContentDir());
	char * p2 = NULL;
	char * p = strtok_r(tmp, ";", &p2);
	while (p) {
		ScanDirectory(p, &qFirst, &qLast, &nofiles);
		p = strtok_r(NULL, ";", &p2);
	}
	qm_free(tmp);
	//ScanDirectory(api->getContentDir(), &qFirst, &qLast, &nofiles);
	scan_time = time(NULL) - scan_time;
	api->botapi->ib_printf("SimpleDJ -> %d files scanned in %d seconds...\n", nofiles, scan_time);
	SaveMetaCache();
	FreeMetaCache();
//	DRIFT_DIGITAL_SIGNATURE();

	if (nofiles > 1) {
		api->botapi->ib_printf("SimpleDJ -> Sorting files...\n");
		SortQueue(&qFirst, &qLast);
		api->botapi->ib_printf("SimpleDJ -> Sorting complete!\n");
	}
	CheckQueue(&qFirst, &qLast);

	if (fOutList) {
		char last_char = 0;
		QUEUE * Scan = qFirst;
		while (Scan) {
			if (toupper(Scan->fn[0]) != last_char && strlen(api->FileLister->NewChar)) {
				sprintf(buf,"%s\r\n",api->FileLister->NewChar);
				char buf2[2] = { toupper(Scan->fn[0]), 0 };
				last_char = toupper(Scan->fn[0]);
				str_replace(buf,sizeof(buf),"%char%", buf2);
				fwrite(buf,strlen(buf),1,fOutList);
			}
			sprintf(buf,"%s\r\n",api->FileLister->Line);
			str_replace(buf,sizeof(buf),"%file%",Scan->fn);
			fwrite(buf,strlen(buf),1,fOutList);
			Scan = Scan->Next;
		}

		if (strlen(api->FileLister->Footer) && fOutList != NULL) {
			FILE *fOutHeader=fopen(api->FileLister->Footer,"r");
			if (fOutHeader) {
				fseek(fOutHeader,0,SEEK_END);
				long len=ftell(fOutHeader);
				fseek(fOutHeader,0,SEEK_SET);
				char *outfl_buf = (char *)qm_malloc(len);
				if (fread(outfl_buf,len,1,fOutHeader) == 1 && fwrite(outfl_buf,len,1,fOutList) == 1) {
					fflush(fOutList);
				} else {
					printf("Error reading/writing file lister footer!\n");
				}
				qm_free(outfl_buf);
				fclose(fOutHeader);
			} else {
				fprintf(fOutList,"Error opening '%s'\r\n",api->FileLister->Footer);
				api->botapi->ib_printf("SimpleDJ -> Error opening file lister footer file!\n");
			}
		}
		fclose(fOutList);
	}
	RelMutexPtr(api->getQueueMutex());
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

void ScanDirectory(char * path, QUEUE ** qqFirst, QUEUE ** qqLast, int32 * num_files) {
	if (api->getQueueMutex()->LockingThread() != GetCurrentThreadId()) {
		api->botapi->ib_printf("SimpleDJ -> QueueMutex should be locked when calling Scan Directory!\n");
	}
	//QUEUE *fQue = *qqFirst;
	//QUEUE *lQue = *qqLast;

	char buf[MAX_PATH],buf2[MAX_PATH];
	//QUEUE *Scan=qFirst;

	Directory * dir = new Directory(path);
	DECODER * dec = NULL;
	while (dir->Read(buf, sizeof(buf), NULL)) {
		sprintf(buf2, "%s%s", path, buf);
		struct stat st;
		//time_t mTime = 0;
		if (stat(buf2, &st)) {// error completing stat
			continue;
		}
		if (!S_ISDIR(st.st_mode)) {
			if ((dec = api->GetFileDecoder(buf))) {
				QUEUE * Scan = (QUEUE *)qm_malloc(sizeof(QUEUE));
				memset(Scan,0,sizeof(QUEUE));

				size_t len = strlen(buf)+1;
				Scan->fn = (char *)qm_malloc(len);
				strcpy(Scan->fn, buf);
				len = strlen(path)+1;
				Scan->path = (char *)qm_malloc(len);
				strcpy(Scan->path, path);
				Scan->ID = FileID(buf2);

				TITLE_DATA * td = (TITLE_DATA *)qm_malloc(sizeof(TITLE_DATA));
				memset(td, 0, sizeof(TITLE_DATA));

				META_CACHE * meta = FindFileMeta(Scan->ID);
				if (meta) {
					if (meta->mTime == st.st_mtime) {
						//printf("Using cached results for file ID: %u\n", id);
						meta->used = 1;
						memcpy(td, &meta->meta, sizeof(TITLE_DATA));
						if (strlen(td->title)) {
							Scan->meta = td;
						}
					}
				}

				if (!strlen(td->title) && !meta) {
					bool ret = false;
					try {
						//api->botapi->ib_printf("File: %s\n", buf);
						ret = dec->get_title_data(buf2, td, &Scan->songlen);
						//api->botapi->ib_printf("Scanned\n");
					} catch(...) {
						api->botapi->ib_printf("SimpleDJ -> Exception occured while trying to process %s\n", buf);
						ret = false;
					}
					strtrim(td->title);
					strtrim(td->album);
					strtrim(td->artist);
					strtrim(td->genre);
					SetFileMeta(Scan->ID, st.st_mtime, td);
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
					qm_free(td);
					//delete td;
				}

				*num_files = *num_files + 1;
				AddToQueue(Scan, qqFirst, qqLast);
				//GetADConfig()->nofiles++;
			}
		} else if (buf[0] != '.') {
#ifdef WIN32
			sprintf(buf2,"%s%s\\",path,buf);
#else
			sprintf(buf2,"%s%s/",path,buf);
#endif
			ScanDirectory(buf2, qqFirst, qqLast, num_files);
		}
	}
	delete dir;
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
		api->botapi->db->Query("DROP TABLE IF EXISTS autodj_cache", NULL, NULL, NULL);
		ut->std_reply(ut, "Operation completed");
		return 1;
	}

	return 0;
}


bool init(SD_PLUGIN_API * pApi) {
	api = pApi;
	COMMAND_ACL acl;
	api->botapi->commands->FillACL(&acl, 0, UFLAG_MASTER|UFLAG_OP, 0);
	api->botapi->commands->RegisterCommand_Proc(api->GetPluginNum(), "autodj-clear",	    &acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE, QM_Commands, "This command will empty and reset SimpleDJ's Meta Cache");
	return true;
}

void quit() {
	api->botapi->commands->UnregisterCommandByName( "autodj-clear" );
}

SD_QUEUE_PLUGIN plugin = {
	SD_PLUGIN_VERSION,
	"Memory Queue",

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
	FindFileByID,
	GetRandomFile,

	NULL
};

PLUGIN_EXPORT SD_QUEUE_PLUGIN * GetSDQPlugin() { return &plugin; }
