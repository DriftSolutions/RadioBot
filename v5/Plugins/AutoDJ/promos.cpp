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

promoQueue promos;

template<typename RandomAccessIterator>
void knuthShuffle(RandomAccessIterator begin, RandomAccessIterator end) {
  for(unsigned int n = end - begin - 1; n >= 1; --n) {
    unsigned int k = api->genrand_int32() % (n + 1);
    if (k != n) {
      std::iter_swap(begin + k, begin + n);
    }
  }
}

void ScanDirectory(const char * path, int32 * num_files) {
	if (QueueMutex.LockingThread() != GetCurrentThreadId()) {
		api->ib_printf2(pluginnum,_("AutoDJ -> QueueMutex should be locked when calling Scan Directory!\n"));
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
			if ((dec = GetFileDecoder(buf2))) {
				QUEUE * Scan = AllocQueue();
				Scan->fn = zstrdup(buf);
				Scan->path = zstrdup(path);
				//unsigned long id = ;
				//Scan->ID = FileID(buf2);

				TITLE_DATA * td = (TITLE_DATA *)zmalloc(sizeof(TITLE_DATA));
				if (ReadMetaData(buf2, td) == 1) {
					Scan->meta = td;
				} else {
					Scan->meta = NULL;
					zfree(td);
				}

				*num_files = *num_files + 1;
				promos.push_back(Scan);
				//AddToQueue(Scan, qqFirst, qqLast);
				//ad_config.nofiles++;
			}
		} else if (buf[0] != '.') {
#ifdef WIN32
			sprintf(buf2,"%s%s\\",path,buf);
#else
			sprintf(buf2,"%s%s/",path,buf);
#endif
			ScanDirectory(buf2, num_files);
		}
	}
	delete dir;
	//DRIFT_DIGITAL_SIGNATURE();
}

void QueuePromoFiles(const char *path) {
	if (path[0] == 0) { return; }

	AutoMutex(QueueMutex);
	//int32 nopromos=0;

	for (promoQueue::iterator x = promos.begin(); x != promos.end(); x++) {
		FreeQueue(*x);
	}
	promos.clear();

	ad_config.num_promos = 0;
	char * tmp = zstrdup(ad_config.Server.PromoDir);
	char * p2 = NULL;
	char * p = strtok_r(tmp, ";", &p2);
	while (p) {
		ScanDirectory(p, &ad_config.num_promos);
		p = strtok_r(NULL, ";", &p2);
	}
	zfree(tmp);
	DRIFT_DIGITAL_SIGNATURE();

	api->ib_printf2(pluginnum,_("AutoDJ -> %d Promos Loaded...\n"),ad_config.num_promos);
	if (promos.size() > 1) {
		api->ib_printf2(pluginnum,_("AutoDJ -> Shuffling promos...\n"));

		knuthShuffle<promoQueue::iterator>(promos.begin(), promos.end());

		api->ib_printf2(pluginnum,_("AutoDJ -> Shuffle complete.\n"));
	}
}
