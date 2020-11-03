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

#include "sam.h"

bool AddRequestDB(USER_PRESENCE * ut, SAM_SONG * song, char * reply, size_t replySize) {
	bool update_queue = false;
	if (!(ut->Flags & (UFLAG_MASTER|UFLAG_OP))) {
		std::stringstream sstr;
		sstr << "SELECT * FROM queuelist WHERE songID=" << song->ID << " LIMIT 1";
		conx.Ping();
		MYSQL_RES * res = conx.Query(sstr.str().c_str());
		if (res && conx.NumRows(res) > 0) {
			SC_Row row;
			if (conx.FetchRow(res, row)) {
				string p = row.Get("shoutirc_req");
				if (p.length() && atoul(p.c_str()) == 0) {
					update_queue = true;
				}
			}
			if (!update_queue) {
				conx.FreeResult(res);
				strlcpy(reply, _("That file has already been requested, it should play soon..."), replySize);
				return false;
			}
		}
		conx.FreeResult(res);
	}

	bool ret = false;
	reply[0] = 0;
	if ((ut->Flags & (UFLAG_MASTER|UFLAG_OP)) || AllowRequest(song, ut, reply, replySize)) {
		reqMutex.Lock();
		std::stringstream sstr;
		if (update_queue) {
			sstr << "UPDATE queuelist SET sortID=" << GetNextSortID() << ", shoutirc_req=" << ++config.requestNumber;
			if (config.AuxData) {
				sstr << ", auxdata=''";
			}
			sstr << " WHERE songID=" << song->ID;
		} else {
			sstr << "INSERT INTO queuelist SET songID=" << song->ID << ", sortID=" << GetNextSortID() << ", shoutirc_req=" << ++config.requestNumber;
			if (config.AuxData) {
				sstr << ", auxdata=''";
			}
		}
		conx.Ping();
		MYSQL_RES * res = conx.Query(sstr.str().c_str());
		conx.FreeResult(res);

		if (config.AdjustWeight) {
			AdjustWeight(song->ID, config.AdjustWeight);
		}
		reqMutex.Release();
		if (!api->LoadMessage("ReqThanks", reply,replySize)) {
			strlcpy(reply,_("Thank you for your request, it has been added to the queue."),replySize);
		}
		ret = true;
	} else {
		if (!strlen(reply)) {
			strlcpy(reply,_("Sorry, that song was already requested recently..."),replySize);
		}
	}
	return ret;
}

bool AddRequestSAM(USER_PRESENCE * ut, SAM_SONG * song, char * reply, size_t replySize) {
	//http://127.0.0.1:1222/req/?songID=45&host=xhost
	return false;
}

bool AddRequest(USER_PRESENCE * ut, SAM_SONG * song, char * reply, size_t replySize) {
	return AddRequestDB(ut, song, reply, replySize);
}

bool req_iface_from_queue(USER_PRESENCE * ut, SAM_SONG * song, char * reply, size_t replySize) {
	if (!config.EnableRequests) {
		strlcpy(reply, _("Sorry, I am not taking requests right now..."), replySize);
		return false;
	}
	if (!config.playing) {
		strlcpy(reply, _("I am not playing a song right now!"), replySize);
		return false;
	}
	if (song == NULL) {
		strlcpy(reply, _("Sorry, I could not find that file in my playlist!"), replySize);
		return false;
	}

	bool ret = AddRequest(ut, song, reply, replySize);
	FreeSong(song);
	return ret;
}

bool req_iface_from_find(USER_PRESENCE * ut, FIND_RESULT * res, char * reply, size_t replySize) {
	return req_iface_from_queue(ut, GetSongFromID(res->id), reply, replySize);
}
bool req_iface_request(USER_PRESENCE * ut, const char * txt, char * reply, size_t replySize) {
	if (strchr(txt,'/') || strchr(txt,'\\')) {
		strlcpy(reply, _("Sorry, I could not find that file in my playlist!"), replySize);
		return false;
	}

	return req_iface_from_queue(ut, FindFile(txt), reply, replySize);
}

void req_find_free(FIND_RESULTS * res) {
	for (int i=0; i < res->num_songs; i++) {
		zfree(res->songs[i].fn);
	}
	zfree(res);
}

void req_iface_find(const char * fn, int max_results, int start, USER_PRESENCE * ut, find_finish_type find_finish) {
	if (!config.playing || !config.EnableFind) {
		find_finish(ut, NULL, max_results);
		return;
	}
	char buf2[1024];

	sprintf(buf2,"*%s*",fn);
	str_replace(buf2,sizeof(buf2)," ","*");

	FIND_RESULTS * ret = NULL;

	SONG_FIND_RESULT * qRes = FindWild(buf2);
	if (qRes != NULL) {
		if (qRes->num > 0) {
			int num = (qRes->num < MAX_FIND_RESULTS) ? qRes->num:MAX_FIND_RESULTS;
			if (num > max_results) { num = max_results; }
			while ((start+num) > qRes->num && num>0) {
				num--;
			}
			ret = znew(FIND_RESULTS);
			memset(ret, 0, sizeof(FIND_RESULTS));
			sstrcpy(ret->query, fn);
			ret->num_songs = num;
			ret->start = start;
			ret->plugin = pluginnum;
			if (qRes->num > (start+num)) { ret->have_more = true; }
			ret->free = req_find_free;
			for (int ind=0; ind < num; ind++) {
				ret->songs[ind].fn = zstrdup(nopath(qRes->results[start+ind]->fn));
				ret->songs[ind].id = qRes->results[start+ind]->ID;
			}
		}
		FreeFindResult(qRes);
	}

	find_finish(ut, ret, max_results);
}


REQUEST_INTERFACE sam_req_iface = {
	req_iface_find,
	req_iface_from_find,
	req_iface_request
};
