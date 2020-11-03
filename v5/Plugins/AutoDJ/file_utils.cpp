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

#include "autodj.h"
#include <deque>

void GenCRC32Table();
bool has_crc_init=false;

void QueueContentFiles(bool promos_too) {
	AutoMutex(QueueMutex);
	ad_config.reloading = true;
	ad_config.num_files = ad_config.Queue->QueueContentFiles();
	if (promos_too) {
		QueuePromoFiles(ad_config.Server.PromoDir);
	}
	ad_config.reloading = false;
}

uint32 FileID(const char * fn) {
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

struct SCANTHREAD {
	bool done;
	FSProcFileType proc;
	//int32 * num_files;
	SyncedInt<int32> * num_files;
	void * uptr;
	vector<string> * dirsQueued;
};

THREADTYPE ScanDirectory(void * lpData);
void ScanDirectory(SCANTHREAD * hst);

class ScanDirEntry {
public:
	string dir;
	int playlist;
};

typedef std::deque<ScanDirEntry> dirListType;
dirListType dirList;
Titus_Mutex scanMutex;

bool WasDirQueued(vector<string> * dl, const char * fn) {
	AutoMutex(scanMutex);
	size_t len = strlen(fn);
	for (size_t i=0; i < dl->size(); i++) {
		if (dl->at(i).length() == len && !stricmp(dl->at(i).c_str(), fn)) {
			return true;
		}
	}
	return false;
}

int32 DoFileScan(FSProcFileType proc, FSPostProc post_proc, void * uptr) {
	LockMutex(QueueMutex);

	READER_HANDLE *fOutList=NULL;
	if (strlen(ad_config.FileLister.List) > 0) {
		fOutList = OpenFile(ad_config.FileLister.List, READER_WRONLY|READER_CREATE|READER_TRUNC);
		if (strlen(ad_config.FileLister.Header) && fOutList != NULL) {
			FILE *fOutHeader=fopen(ad_config.FileLister.Header,"rb");
			if (fOutHeader) {
				fseek(fOutHeader,0,SEEK_END);
				long len=ftell(fOutHeader);
				fseek(fOutHeader,0,SEEK_SET);
				char *outfl_buf = (char *)zmalloc(len);
				if (fread(outfl_buf,len,1,fOutHeader) == 1) {
					fOutList->write(outfl_buf,len,fOutList);
				} else {
					api->ib_printf2(pluginnum,_("AutoDJ -> Error reading file lister header file!\n"));
				}
				zfree(outfl_buf);
				fclose(fOutHeader);
			} else {
				//fprintf(fOutList,_("Error opening '%s'\r\n"),ad_config.FileLister.Header);
				api->ib_printf2(pluginnum,_("AutoDJ -> Error opening file lister header file!\n"));
			}
		}
	}

	dirList.clear();
	vector<string> dirsQueued;
	ScanDirEntry de;
	for (int i=0; i < ad_config.num_playlists; i++) {
		de.playlist = i;
		char * tmp = zstrdup(ad_config.Playlists[i].ContentDir);
		char * p2 = NULL;
		char * p = strtok_r(tmp, ";", &p2);
		while (p) {
			de.dir = p;
			dirList.push_back(de);
			dirsQueued.push_back(p);
			p = strtok_r(NULL, ";", &p2);
		}
		zfree(tmp);
	}

	SyncedInt<int32> nofiles;

	int NUM_CPUS = 1;
	int NUM_SCAN_THREADS = 1;
	if (ad_config.Options.ScanThreads < 1) {
		#if defined(WIN32)
			SYSTEM_INFO si;
			memset(&si, 0, sizeof(si));
			GetSystemInfo(&si);
			NUM_CPUS = si.dwNumberOfProcessors;
		#elif defined(_SC_NPROCESSORS_ONLN)
			NUM_CPUS = sysconf(_SC_NPROCESSORS_ONLN);
			if (NUM_CPUS < 1) { NUM_CPUS = 1; }
		#endif
		NUM_SCAN_THREADS = NUM_CPUS*2;
		if (NUM_SCAN_THREADS > 6) { NUM_SCAN_THREADS = 6; }
		api->ib_printf2(pluginnum,_("AutoDJ -> Number of System Processors: %d\n"), NUM_CPUS);
		ad_config.Options.ScanThreads = NUM_SCAN_THREADS;
	} else {
		NUM_SCAN_THREADS = ad_config.Options.ScanThreads;
	}

	api->ib_printf2(pluginnum,_("AutoDJ -> Beginning file scan...\n"));
	int64 scan_time = time(NULL);

	if (NUM_SCAN_THREADS > 1) {
		api->ib_printf2(pluginnum,_("AutoDJ -> Using %d threads for scanning...\n"), NUM_SCAN_THREADS);
		SCANTHREAD * st = new SCANTHREAD[NUM_SCAN_THREADS];
		memset(st, 0, sizeof(SCANTHREAD)*NUM_SCAN_THREADS);
		int cpu_ind = 0;
		for (int i=0; i < NUM_SCAN_THREADS; i++, cpu_ind++) {
			if (cpu_ind == NUM_CPUS) { cpu_ind = 0; }
			st[i].num_files = &nofiles;
			st[i].proc = proc;
			st[i].uptr = uptr;
			st[i].dirsQueued = &dirsQueued;
			TT_StartThread(ScanDirectory, &st[i], _("Scan Thread"), pluginnum);
		}

		bool done = false;
		int step=0;
		while (!done) {
			step++;
			safe_sleep(1000, true);
			done = true;
			for (int i=0; i < NUM_SCAN_THREADS; i++) {
				if (!st[i].done) { done = false; break; }
			}
			if (step == 5) {
				scanMutex.Lock();
				api->ib_printf2(pluginnum,_("AutoDJ -> Scanned %d file(s), %u directories queued for scan...\n"), nofiles.Get(), dirList.size());
				scanMutex.Release();
				step = 0;
			}
		}
		delete [] st;
	} else {
		SCANTHREAD st;
		memset(&st, 0, sizeof(st));
		st.num_files = &nofiles;
		st.proc = proc;
		st.uptr = uptr;
		st.dirsQueued = &dirsQueued;
		ScanDirectory(&st);
	}

	scan_time = time(NULL) - scan_time;
	double x = nofiles.Get();
	x /= (scan_time < 1) ? 1:scan_time;
	char buf[128];
	//sprintf(buf, I64FMT, scan_time);
	api->textfunc->duration(scan_time, buf);
	api->ib_printf2(pluginnum,_("AutoDJ -> %d files scanned in %s (%.02f files/sec)...\n"), nofiles.Get(), buf, x);

	if (post_proc) {
		//this should handle any sorting needed, and FileLister Line writing
		post_proc(fOutList);
	}

	if (fOutList) {
		if (strlen(ad_config.FileLister.Footer)) {
			FILE *fOutHeader=fopen(ad_config.FileLister.Footer,"r");
			if (fOutHeader) {
				fseek(fOutHeader,0,SEEK_END);
				long len=ftell(fOutHeader);
				fseek(fOutHeader,0,SEEK_SET);
				char *outfl_buf = (char *)zmalloc(len);
				if (fread(outfl_buf,len,1,fOutHeader) == 1) {
					fOutList->write(outfl_buf,len,fOutList);
				} else {
					api->ib_printf2(pluginnum,_("AutoDJ -> Error reading file lister footer file!\n"));
				}
				zfree(outfl_buf);
				fclose(fOutHeader);
			} else {
				//fprintf(fOutList,_("Error opening '%s'\r\n"),ad_config.FileLister.Footer);
				api->ib_printf2(pluginnum,_("AutoDJ -> Error opening file lister footer file!\n"));
			}
		}
		fOutList->close(fOutList);
	}

	RelMutex(QueueMutex);
	return nofiles.Get();
}

THREADTYPE ScanDirectory(void * lpData) {
	TT_THREAD_START
	SCANTHREAD * hst = (SCANTHREAD *)tt->parm;
#if defined(WIN32)
	//SetThreadIdealProcessor(tt->hThread, hst->cpu);
#endif
	ScanDirectory(hst);
	TT_THREAD_END
}

void ScanDirectory(SCANTHREAD * hst) {
	char buf[MAX_PATH],buf2[MAX_PATH];

	while (!hst->done && !ad_config.shutdown_now) {
		scanMutex.Lock();
		if (dirList.size() > 0) {
			dirListType::iterator x = dirList.begin();
			char * path = zstrdup(x->dir.c_str());
			int playlist = x->playlist;
			dirList.erase(x);
			scanMutex.Release();

			//api->ib_printf2(pluginnum,"ScanDir: %s\n", path);

			Directory * dir = new Directory(path);
//			DECODER * dec = NULL;
			bool is_dir = false;
			while (!ad_config.shutdown_now && dir->Read(buf, sizeof(buf), &is_dir)) {
				sprintf(buf2, "%s%s", path, buf);
				/*
				struct stat st;
				if (stat(buf2, &st)) {// error completing stat
					continue;
				}
				*/
				if (!is_dir) {
					if (ad_config.Options.ScanDebug) {
						api->ib_printf2(pluginnum,"AutoDJ -> Scanning file: %s\n", buf2);
					}
					if (hst->proc(&scanMutex, path, buf, buf2, playlist, hst->uptr)) {//, st
						//scanMutex.Lock();
						//*hst->num_files = *hst->num_files + 1;
						//scanMutex.Release();
						hst->num_files->Increment();
					}
				} else if (buf[0] != '.') {
		#ifdef WIN32
					sprintf(buf2,"%s%s\\",path,buf);
		#else
					sprintf(buf2,"%s%s/",path,buf);
		#endif
					if (!WasDirQueued(hst->dirsQueued, buf2)) {
						ScanDirEntry de;
						de.playlist = playlist;
						de.dir = buf2;
						scanMutex.Lock();
						hst->dirsQueued->push_back(buf2);
						dirList.push_back(de);
						scanMutex.Release();
					} else {
						api->ib_printf2(pluginnum,"AutoDJ -> Skipping '%s': was already queued.\n", buf2);
					}
					//ScanDirectory(buf2, num_files);
				}
			}
			delete dir;
			zfree(path);
		} else {
			scanMutex.Release();
			hst->done = true;
		}
	}
	hst->done = true;
}

unsigned long crc_table[256];

/* chksum_crc() -- to a given block, this one calculates the
 *				crc32-checksum until the length is
 *				reached. the crc32-checksum will be
 *				the result.
 */
unsigned long QM_CRC32(unsigned char *block, unsigned int length) {
	if (!has_crc_init) {
		GenCRC32Table();
		has_crc_init = true;
	}

	register unsigned long crc;
	unsigned long i;

	crc = 0xFFFFFFFF;
	for (i = 0; i < length; i++) {
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
