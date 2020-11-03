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

extern QUEUE * rQue;

bool req_iface_from_queue(USER_PRESENCE * ut, QUEUE * song, char * reply, size_t replySize) {
	if (!ad_config.Options.EnableRequests) {
		strlcpy(reply, _("Sorry, I am not taking requests right now..."), replySize);
		return false;
	}
	if (!ad_config.playing || !ad_config.connected) {
		strlcpy(reply, _("I am not playing a song right now!"), replySize);
		return false;
	}
	if (song == NULL) {
		strlcpy(reply, _("Sorry, I could not find that file in my playlist!"), replySize);
		return false;
	}

	AutoMutex(QueueMutex);

	QUEUE * Scan=NULL;
	if (!(ut->Flags & (UFLAG_MASTER|UFLAG_OP))) {
		int reqcnt = 0;
		Scan=rQue;
		while(Scan != NULL) {
			reqcnt++;
			if (Scan->ID == song->ID) {
				strlcpy(reply, _("That file has already been requested, it should play soon..."), replySize);
				return false;
			}
			Scan = Scan->Next;
		}
		if (ad_config.Options.MaxRequests > 0 && reqcnt >= ad_config.Options.MaxRequests) {
			strlcpy(reply, _("My request queue is currently full, please try again later..."), replySize);
			return false;
		}
	}

	REQ_INFO *req = (REQ_INFO *)zmalloc(sizeof(REQ_INFO));
	memset(req, 0, sizeof(REQ_INFO));
	if (!stricmp(ut->Desc, "IRC")) {
		req->netno=ut->NetNo;
	} else {
		req->netno=-1;
	}
	if (ut->Channel) {
		sstrcpy(req->channel,ut->Channel);
	}
	if (!api->user->uflag_have_any_of(ut->Flags, UFLAG_SOURCE_NO_REQ_BY)) {
		sstrcpy(req->nick,ut->Nick);
	}
	song->isreq = req;
	if (ad_config.Options.RequestDelay) {
		req->playAfter = time(NULL) + ad_config.Options.RequestDelay;
	}
	song->Next = NULL;

	//QUEUE *Scan2 = rQue;
//	char buf[1024];
	bool ret = false;
	if (api->user->uflag_have_any_of(ut->Flags, UFLAG_MASTER|UFLAG_OP) || AllowRequest(song, ut)) {
		int num = AddRequest(song, ut->User ? ut->User->Nick:ut->Nick);
		if (ad_config.Options.RequestDelay) {
			strlcpy(reply,_("Thank you for your request, it has been added to the queue."),replySize);
		} else {
			snprintf(reply,replySize,_("Thank you for your request, it has been added to the queue. It will play after %d more song(s)..."),num);
		}
		ret = true;
	} else {
		strlcpy(reply, _("Sorry, that song has already been requested recently..."), replySize);
		FreeQueue(song);
	}

	return ret;
}

bool req_iface_from_find(USER_PRESENCE * ut, FIND_RESULT * res, char * reply, size_t replySize) {
	return req_iface_from_queue(ut, ad_config.Queue->FindFileByID(res->id), reply, replySize);
}

bool req_iface_request(USER_PRESENCE * ut, const char * txt, char * reply, size_t replySize) {
	if (strchr(txt,'/') || strchr(txt,'\\')) {
		ut->std_reply(ut, _("Sorry, I could not find that file in my playlist!"));
		return false;
	}

	return req_iface_from_queue(ut, ad_config.Queue->FindFile(txt), reply, replySize);
}

void req_find_free(FIND_RESULTS * res) {
	for (int i=0; i < res->num_songs; i++) {
		zfree(res->songs[i].fn);
	}
	zfree(res);
}

void req_iface_find(const char * fn, int max_results, int start, USER_PRESENCE * ut, find_finish_type find_finish) {
	if (!ad_config.playing || !ad_config.connected || !ad_config.Options.EnableFind) {
		find_finish(ut, NULL, max_results);
		return;
	}
	char buf2[1024];

	sprintf(buf2,"*%s*",fn);
	str_replace(buf2,sizeof(buf2)," ","*");

	FIND_RESULTS * ret = NULL;

	LockMutex(QueueMutex);
	QUEUE_FIND_RESULT * qRes = ad_config.Queue->FindWild(buf2);
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
			ret->start = start;
			ret->num_songs = num;
			ret->plugin = pluginnum;
			if (qRes->num > (start+num)) { ret->have_more = true; }
			ret->free = req_find_free;
			for (int ind=0; ind < num; ind++) {
				ret->songs[ind].fn = zstrdup(qRes->results[start+ind]->fn);
				ret->songs[ind].id = qRes->results[start+ind]->ID;
			}
		}
		ad_config.Queue->FreeFindResult(qRes);
	}
	RelMutex(QueueMutex);
	DRIFT_DIGITAL_SIGNATURE();
	find_finish(ut, ret, max_results);
}


REQUEST_INTERFACE adj_req_iface = {
	req_iface_find,
	req_iface_from_find,
	req_iface_request
};
