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

TT_DEFINE_THREAD(Watchdog) {
	TT_THREAD_START

	TT_THREAD_END
}

const char * GetDeckName(AUTODJ_DECKS deck) {
	switch(deck) {
		case DECK_A:
			return "Deck A";
			break;
		case DECK_B:
			return "Deck B";
			break;
		default:
			return "Unknown Deck";
			break;
	}
}

void AddToQueue(QUEUE * q, QUEUE ** qqFirst, QUEUE ** qqLast) {
	if (QueueMutex.LockingThread() != GetCurrentThreadId()) {
		api->ib_printf2(pluginnum,_("AutoDJ -> QueueMutex should be locked when calling AddToQueue!\n"));
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


void ab_realloc(Audio_Buffer * buf, size_t nSize) {
	if (nSize > 0) {
		buf->len = nSize;
		buf->buf = (int16 *)zrealloc(buf->buf, buf->len * sizeof(short));
		if (buf->buf == NULL) {
			abort();
		}
	} else {
		buf->reset(buf);
	}
}
void ab_reset(Audio_Buffer * buf) {
	if (buf->buf) {
		zfree(buf->buf);
		buf->buf = NULL;
	}
	buf->len = 0;
}


Audio_Buffer * AllocAudioBuffer() {
	Audio_Buffer * ret = znew(Audio_Buffer);
	memset(ret, 0, sizeof(Audio_Buffer));
	ret->realloc = ab_realloc;
	ret->reset = ab_reset;
	return ret;
}

void FreeAudioBuffer(Audio_Buffer * buf) {
	buf->reset(buf);
	zfree(buf);
}

inline void CheckAndTrunc(char * str) {
	char * p = (char *)FirstInvalidUTF8(str);
	if (p) {
		api->ib_printf2(pluginnum, _("AutoDJ -> Invalid UTF-8 found in string '%s', truncating at '%s'...\n"), str, p);
		*p = 0;
	}
}

// retval: 0 title based on FN, 1 metadata read from file
int ReadMetaDataEx(const char * fn, TITLE_DATA * td, uint32 * songlen, DECODER * pDec) {
	memset(td, 0, sizeof(TITLE_DATA));
	if (songlen) { *songlen = 0; }
	int ret = 0;
	if (access(fn, 0) == 0) {
		DECODER * dec = pDec ? pDec : GetFileDecoder(fn);
		if (dec) {
			try {
				ret = dec->get_title_data((char *)fn, td, songlen) ? 1 : 0;
				if (ret) {
					CheckAndTrunc(td->title);
					CheckAndTrunc(td->album);
					CheckAndTrunc(td->artist);
					CheckAndTrunc(td->genre);
					CheckAndTrunc(td->album_artist);
					CheckAndTrunc(td->url);
					CheckAndTrunc(td->comment);

					strtrim(td->title);
					strtrim(td->album);
					strtrim(td->artist);
					strtrim(td->genre);
					strtrim(td->album_artist);
					strtrim(td->url);
					strtrim(td->comment);
				}
			} catch (...) {
				api->ib_printf2(pluginnum,_("AutoDJ -> Exception occured while trying to process %s\n"), fn);
				ret = 0;
				if (songlen) { *songlen = 0; }
			}
			if (!strlen(td->title)) {
				ret = 0;
			}
			if (ret == 0) {
				memset(td, 0, sizeof(TITLE_DATA));
			}
		}
	}
	if (ret == 0) {
		memset(td, 0, sizeof(TITLE_DATA));
		const char * p = strrchr(fn, '/');
		if (!p) {
			p = strrchr(fn, '\\');
		}
		if (p) {
			strlcpy(td->title, p+1, sizeof(td->title));
		} else {
			strlcpy(td->title, fn, sizeof(td->title));
		}
		char * ext = strrchr(td->title, '.');
		if (ext) { *ext=0; }
	}
	return ret;
}

char * PrettyTime(int32 seconds, char * buf) {
	int m=0, h=0;
	while (seconds >= 3600) {
		h++;
		seconds -= 3600;
	}
	while (seconds >= 60) {
		m++;
		seconds -= 60;
	}
	if (h) {
		sprintf(buf,"%d:%.2d:%.2d",h,m,seconds);
	} else {
		sprintf(buf,"%.2d:%.2d",m,seconds);
	}
	return buf;
}

sql_rows GetTable(char * query, char **errmsg) {
//int GetTable(char * query, char ***resultp, int *nrow, int *ncolumn, char **errmsg) {
	sql_rows rows;
	int ret = 0, tries = 0;
	char ** errmsg2 = errmsg;
	if (errmsg == NULL) {
		static char * errmsg3 = NULL;
		errmsg2 = &errmsg3;
	}
	int nrow=0, ncol=0;
	char ** resultp;
	while ((ret = api->db->GetTable(query, &resultp, &nrow, &ncol, errmsg2)) == SQLITE_BUSY && tries < 5) { tries++; }
	if (ret != SQLITE_OK) {
		api->ib_printf2(pluginnum,_("GetTable: %s -> %d (%s)\n"), query, ret, *errmsg2);
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
		api->db->FreeTable(resultp);
		//azResult0 = "Name";
		//azResult1 = "Age";
	}
	if (errmsg == NULL && *errmsg2) {
		api->db->Free(*errmsg2);
	}
	return rows;
}
