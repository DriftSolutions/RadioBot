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

#undef zmalloc
#undef zstrdup
#undef zrealloc
#undef znew
#undef zfreenn
#undef zfree

//META_CACHE *mFirst=NULL, *mLast=NULL;
//bool mChanged=false;

extern AD_PLUGIN_API * adapi;

typedef std::map<uint32, CACHE_QUEUE *> metaCacheList;
struct CACHE_PAIR {
	uint32 file;
	uint32 playlist;
};
struct CACHE_PL {
	bool wasSeen;
	int Seen;
};
// cache comparison
// returns true if s1 < s2
struct comp_cpair : binary_function<CACHE_PAIR, CACHE_PAIR, bool> {
  bool operator() (const CACHE_PAIR & s1, const CACHE_PAIR & s2) const
    {
		if (s1.playlist != s2.playlist) {
			return (s1.playlist < s2.playlist) ? true : false;
		}
		return (s1.file < s2.file) ? true : false;
    }
}; // end of comp_cpair
typedef std::map<CACHE_PAIR, CACHE_PL, comp_cpair> metaPlaylist;

metaCacheList mList;
metaPlaylist  pList;
unsigned int mcFound = 0;
Titus_Mutex mcMutex;

void FreeMetaCache() {
#if 1
	for (metaCacheList::iterator x = mList.begin(); x != mList.end(); x = mList.begin()) {
		adapi->FreeQueue(x->second->song);
		adapi->free(x->second);
		mList.erase(x);
	}
#else
	for (metaCacheList::iterator x = mList.begin(); x != mList.end(); x++) {
		adapi->FreeQueue(x->second);
	}
	mList.clear();
#endif
	pList.clear();
}

void LoadMetaCache() {
	FreeMetaCache();

	adapi->botapi->ib_printf(_("AutoDJ (queue_mysql) -> Reading playlist data from MySQL...\n"));
	int64 ticks = adapi->botapi->get_cycles();
	MYSQL_RES * res = LoadMetaCacheQuery();
	CACHE_QUEUE * Scan = NULL;
	while ((Scan = GetCacheRowToQueue(res)) != NULL) {
		mList[Scan->song->ID] = Scan;
	}
	conx.FreeResult(res);

	std::stringstream sstr;
	sstr << "SELECT * FROM `" << qm_config.mysql_table << "_Playlists`";
	res = conx.Query(sstr.str().c_str());
	if (res && conx.NumRows(res)) {
		SC_Row row;
		CACHE_PAIR pa;
		CACHE_PL p;
		memset(&p, 0, sizeof(p));
		while (conx.FetchRow(res, row)) {
			pa.playlist = atoul(row.Get("Playlist").c_str());
			pa.file = atoul(row.Get("FileID").c_str());
			p.Seen = atoi(row.Get("Seen").c_str());
			pList[pa] = p;
		}
	}
	conx.FreeResult(res);

	ticks = adapi->botapi->get_cycles() - ticks;
	adapi->botapi->ib_printf(_("AutoDJ (queue_mysql) -> Loaded data in " I64FMT " ms, %u entries\n"), ticks, mList.size());

	mcFound = 0;
}

#ifndef EXPERIMENTAL_MULTI_QUERY
void CommitMetaCache() {
	conx.Query("COMMIT");
	conx.Query("BEGIN");
}
#endif

#ifdef EXPERIMENTAL_MULTI_QUERY
void CalcPlaylistDiff(SQLConxMulti * scm, uint32 * changeall, uint32 * markseen) {
	AutoMutex(mcMutex);
	uint32 cnt_changeall = 0;
	uint32 cnt_markseen = 0;

	for (metaPlaylist::iterator x = pList.begin(); x != pList.end(); x++) {
		if (x->second.Seen == 0 && x->second.wasSeen) {
			cnt_changeall++;
		} else if (x->second.Seen && !x->second.wasSeen) {
			cnt_changeall++;
		}
		if (x->second.wasSeen) {
			cnt_markseen++;
		}
	}

	*changeall = cnt_changeall;
	*markseen = cnt_markseen;
}

void CalcFilesDiff(SQLConxMulti * scm, uint32 * changeall, uint32 * markseen) {
	AutoMutex(mcMutex);
	uint32 cnt_changeall = 0;
	uint32 cnt_markseen = 0;

	for (metaCacheList::iterator x = mList.begin(); x != mList.end(); x++) {
		CACHE_QUEUE * CScan = x->second;
		//QUEUE * Scan = x->second->song;
		if (!CScan->doSave) {
			if (CScan->Seen && !CScan->wasSeen) {
				cnt_changeall++;
			} else if (CScan->wasSeen) {
				cnt_markseen++;
				if (CScan->Seen == 0) {
					cnt_changeall++;
				}
			}
		}
	}

	*changeall = cnt_changeall;
	*markseen = cnt_markseen;
}

void SaveMetaCache(SQLConxMulti * scm) {
	adapi->botapi->ib_printf(_("AutoDJ (queue_mysql) -> Calculating playlist changes...\n"));
#else
void SaveMetaCache() {
	adapi->botapi->ib_printf(_("AutoDJ (queue_mysql) -> Updating meta cache...\n"));
#endif
	AutoMutex(mcMutex);
	uint32 changeall, markseen;
	CalcFilesDiff(scm, &changeall, &markseen);
	bool seen_mode = false;
	if (markseen < changeall) {
		seen_mode = true;
		stringstream sstr;
		sstr << "UPDATE `" << qm_config.mysql_table << "` SET `Seen`=0";
		scm->Query(sstr.str().c_str());
	}
	for (metaCacheList::iterator x = mList.begin(); x != mList.end(); x++) {
		CACHE_QUEUE * CScan = x->second;
		QUEUE * Scan = x->second->song;
		SONG_RATING r = {0,0};
		if (CScan->doSave) {
			/* Mark as seen and no longer needing to be saved */
			CScan->doSave = false;
			CScan->Seen = 1;

			std::stringstream sstr;
			if (CScan->isNew) {
				sstr << "REPLACE INTO ";
			} else {
				sstr << "UPDATE ";
			}
			if (!IsValidUTF8(Scan->path)) {
				adapi->botapi->ib_printf(_("AutoDJ (queue_mysql) -> Skipping addition of %s%s, invalid UTF-8 in path...\n"), Scan->path, Scan->fn);
				continue;
			}
			if (!IsValidUTF8(Scan->fn)) {
				adapi->botapi->ib_printf(_("AutoDJ (queue_mysql) -> Skipping addition of %s%s, invalid UTF-8 in filename...\n"), Scan->path, Scan->fn);
				continue;
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
				sstr << ", `Year`='" << Scan->meta->year << "'";
			}
			sstr << ", SongLen=" << Scan->songlen;
			adapi->botapi->ss->GetSongRatingByID(Scan->ID, &r);
			sstr << ", Rating=" << r.Rating << ", RatingVotes=" << r.Votes;
			if (!CScan->isNew) {
				sstr << " WHERE ID=" << Scan->ID;
			}
#ifdef EXPERIMENTAL_MULTI_QUERY
			scm->Query(sstr.str().c_str());
#else
			MYSQL_RES * res = conx.Query(sstr.str().c_str());
			conx.FreeResult(res);
#endif
		} else if (!seen_mode && CScan->Seen && !CScan->wasSeen) {
			/* Mark no longer seen */
			CScan->Seen = 0;

			std::stringstream sstr;
			sstr << "UPDATE " << qm_config.mysql_table << " SET Seen=0 WHERE ID=" << Scan->ID;
#ifdef EXPERIMENTAL_MULTI_QUERY
			scm->Query(sstr.str().c_str());
#else
			MYSQL_RES * res = conx.Query(sstr.str().c_str());
			conx.FreeResult(res);
#endif
		} else if (CScan->wasSeen) {
			adapi->botapi->ss->GetSongRatingByID(Scan->ID, &r);
			if (seen_mode || CScan->Seen == 0 || r.Rating != CScan->Rating || r.Votes != CScan->Votes) {
				/* Mark as seen */
				CScan->Seen = 1;

				std::stringstream sstr;
				sstr << "UPDATE ";
				sstr << qm_config.mysql_table << " SET Seen=1";
				sstr << ", Rating=" << r.Rating << ", RatingVotes=" << r.Votes;
				sstr << " WHERE ID=" << Scan->ID;
#ifdef EXPERIMENTAL_MULTI_QUERY
				scm->Query(sstr.str().c_str());
#else
				MYSQL_RES * res = conx.Query(sstr.str().c_str());
				conx.FreeResult(res);
#endif
			}
		} else {
			Scan=Scan;
		}
	}

	CalcPlaylistDiff(scm, &changeall, &markseen);
	//adapi->botapi->ib_printf("AutoDJ (queue_mysql) -> Diff %u / %u\n", changeall, markseen);
	if (changeall < markseen) {
		for (metaPlaylist::iterator x = pList.begin(); x != pList.end(); x++) {
			if (x->second.Seen == 0 && x->second.wasSeen) {
				std::stringstream sstr;
				sstr << "REPLACE INTO `" << qm_config.mysql_table << "_Playlists` (`FileID`,`Playlist`,`Seen`) VALUES (" << x->first.file << ", " << x->first.playlist << ", 1)";
	#ifdef EXPERIMENTAL_MULTI_QUERY
				scm->Query(sstr.str().c_str());
	#else
				MYSQL_RES * res = conx.Query(sstr.str().c_str());
				conx.FreeResult(res);
	#endif
			} else if (x->second.Seen && !x->second.wasSeen) {
				std::stringstream sstr;
				sstr << "REPLACE INTO `" << qm_config.mysql_table << "_Playlists` (`FileID`,`Playlist`,`Seen`) VALUES (" << x->first.file << ", " << x->first.playlist << ", 0)";
	#ifdef EXPERIMENTAL_MULTI_QUERY
				scm->Query(sstr.str().c_str());
	#else
				MYSQL_RES * res = conx.Query(sstr.str().c_str());
				conx.FreeResult(res);
	#endif
			}
		}
	} else {
		stringstream sstr;
		sstr << "UPDATE `" << qm_config.mysql_table << "_Playlists` SET `Seen`=0";
		scm->Query(sstr.str().c_str());
		for (metaPlaylist::iterator x = pList.begin(); x != pList.end(); x++) {
			if (x->second.wasSeen) {
				std::stringstream sstr;
				sstr << "REPLACE INTO `" << qm_config.mysql_table << "_Playlists` (`FileID`,`Playlist`,`Seen`) VALUES (" << x->first.file << ", " << x->first.playlist << ", 1)";
				scm->Query(sstr.str().c_str());
			}
		}
	}
}

CACHE_QUEUE * FindFileMeta(uint32 FileID) {
	AutoMutex(mcMutex);
#ifndef EXPERIMENTAL_MULTI_QUERY
	mcFound++;
	if (mcFound >= 1000) {
		CommitMetaCache();
	}
#endif
	metaCacheList::iterator x = mList.find(FileID);
	if (x != mList.end()) {
		return x->second;
	}
	return NULL;
}

CACHE_QUEUE * AddFileMeta(QUEUE * Scan) {
	AutoMutex(mcMutex);
	CACHE_QUEUE * ret = (CACHE_QUEUE *)adapi->malloc(sizeof(CACHE_QUEUE));
	memset(ret, 0, sizeof(CACHE_QUEUE));
	ret->song = Scan;
	mList[Scan->ID] = ret;
	return ret;
}

void AddPlaylistEntry(uint32 playlist, uint32 file) {
	CACHE_PAIR pa;
	pa.playlist = playlist;
	pa.file = file;
	AutoMutex(mcMutex);
	metaPlaylist::iterator x = pList.find(pa);
	if (x != pList.end()) {
		x->second.wasSeen = true;
	} else {
		CACHE_PL p;
		memset(&p,0,sizeof(p));
		p.wasSeen = true;
		pList[pa] = p;
	}
}
