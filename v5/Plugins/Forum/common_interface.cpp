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

#include "forums.h"

Titus_Mutex Forum_Mutex;
static char forum_desc[] = "Forum";

#if !defined(DEBUG)
void forum_up_ref(USER_PRESENCE * u) {
#else
void forum_up_ref(USER_PRESENCE * u, const char * fn, int line) {
	api->ib_printf("forum_UserPres_AddRef(): %s : %s:%d\n", u->User->Nick, fn, line);
#endif
	LockMutex(Forum_Mutex);
	if (u->User) { RefUser(u->User); }
	u->ref_cnt++;
	RelMutex(Forum_Mutex);
}

#if !defined(DEBUG)
void forum_up_unref(USER_PRESENCE * u) {
#else
void forum_up_unref(USER_PRESENCE * u, const char * fn, int line) {
	api->ib_printf("forum_UserPres_DelRef(): %s : %s:%d\n", u->Nick, fn, line);
#endif
	LockMutex(Forum_Mutex);
	if (u->User) { UnrefUser(u->User); }
	u->ref_cnt--;
#if defined(DEBUG)
	api->ib_printf("forum_UserPres_DelRef(): %s : New Refcount %d\n", u->Nick,u->ref_cnt);
#endif
	if (u->ref_cnt == 0) {
		FORUM_POST * po = (FORUM_POST *)u->Ptr1;
		if (po && po->outbuf && po->outbuf[0]) {
			if (po->type == FPT_POST) {
				po->inst->driver->PostToThread(po);
			} else if (po->type == FPT_PM) {
				po->inst->driver->SendPM(po);
			}
		}
		zfree((void *)u->Nick);
		zfree((void *)u->Hostmask);
		zfree(u->Ptr1);
		zfree(u);
	}
	RelMutex(Forum_Mutex);
}

bool send_forum_post(USER_PRESENCE * ut, const char * str) {
	LockMutex(Forum_Mutex);
	FORUM_POST * po = (FORUM_POST *)ut->Ptr1;
	if (po->type == FPT_POST) {
		api->ib_printf("Forum[%d] -> Send to %s in topic %u in reply to post %u: %s\n", po->inst->id, ut->Nick, po->topic_id, po->post_id, str);
	} else {
		api->ib_printf("Forum[%d] -> Send PM to %s in reply to PM %u: %s\n", po->inst->id, ut->Nick, po->msg_id, str);
	}
	po->outbuf = (char *)zrealloc(po->outbuf, (po->outbuf ? strlen(po->outbuf):0)+1+strlen(str)+1);
	strcat(po->outbuf, str);
	strcat(po->outbuf, "\n");
	//po->lastOut = time(NULL);
	RelMutex(Forum_Mutex);
	return true;
}
bool send_forum_dm(USER_PRESENCE * ut, const char * str) {
	LockMutex(Forum_Mutex);
	FORUM_POST * po = (FORUM_POST *)ut->Ptr1;
	api->ib_printf("Forum[%d] -> Send PM to %s in reply to PM %u: %s\n", po->inst->id, ut->Nick, po->msg_id, str);
	po->outbuf = (char *)zrealloc(po->outbuf, (po->outbuf ? strlen(po->outbuf):0)+1+strlen(str)+1);
	strcat(po->outbuf, str);
	strcat(po->outbuf, "\n");
	RelMutex(Forum_Mutex);
	return true;
}

USER_PRESENCE * alloc_forum_post_presence(FORUM_POST * po) {
	USER_PRESENCE * ret = znew(USER_PRESENCE);
	memset(ret, 0, sizeof(USER_PRESENCE));

	ret->Ptr1 = po;
	ret->Nick = zstrdup(po->username);
	ret->Hostmask = zmprintf("%s!%d@%d.%s.forum", po->username, po->user_id, po->inst->id, po->inst->driver->name);
	ret->User = api->user->GetUser(ret->Hostmask);
	ret->NetNo = -1;
	ret->Flags = ret->User ? ret->User->Flags:api->user->GetDefaultFlags();

	ret->send_chan_notice = send_forum_post;
	ret->send_chan = ret->send_chan_notice;
	ret->send_msg = ret->send_chan_notice;
	ret->send_notice = ret->send_chan_notice;
	ret->std_reply = ret->send_chan_notice;
	ret->Desc = forum_desc;

	ret->ref = forum_up_ref;
	ret->unref = forum_up_unref;

	RefUser(ret);
	return ret;
}

void ProcPost(FORUM_POST * po) {
	USER_PRESENCE * ut = alloc_forum_post_presence(po);
	//USER * user = ut->User;
	StrTokenizer st((char *)po->post_text, '\n');
	for (long i=1; i <= st.NumTok(); i++) {
		char * line = st.GetSingleTok(i);
		strtrim(line);
		if (api->commands->IsCommandPrefix(line[0])) {
			char * cmd = line+1;
			char * p = strchr(cmd, ' ');
			if (p) {
				*p = 0;
				p++;
			}
			COMMAND * com = api->commands->FindCommand(cmd);
			uint32 type = (po->type == FPT_PM) ? CM_ALLOW_CONSOLE_PRIVATE:CM_ALLOW_CONSOLE_PUBLIC;
			if (com && com->proc_type == COMMAND_PROC && (com->mask & type)) {
				api->commands->ExecuteProc(com, p, ut, type|CM_FLAG_NOHANG);
			}
		}
		st.FreeString(line);
	}
	UnrefUser(ut);
}

int GetLastPostID(FORUM_INSTANCE * inst, const char * type) {
	stringstream sstr;
	sstr << "SELECT ID FROM phpBB WHERE Account=" << inst->id << " AND Type='" << type << "'";
	int rows,cols;
	char ** results;
	int ret = 0;
	if (api->db->GetTable(sstr.str().c_str(), &results, &rows, &cols, NULL) == SQLITE_OK && rows > 0) {
		ret = atoul(api->db->GetTableEntry(1, 0, results, rows, cols));
		api->db->FreeTable(results);
	}
	return ret;
}

void UpdateLastPostID(FORUM_INSTANCE * inst, uint32 id, const char * type) {
	stringstream sstr;
	sstr << "REPLACE INTO phpBB (`Account`,`Type`,`ID`) VALUES (" << inst->id << ", '" << type << "', " << id << ");";
	api->db->Query(sstr.str().c_str(), NULL, NULL, NULL);
}

static const char * PostTypeToString(FORUM_POST_TYPES type) {
	switch (type) {
		case FPT_POST:
			return "post";
			break;
		case FPT_PM:
			return "pm";
			break;
		default:
			return "unknown";
			break;
	}
}

TT_DEFINE_THREAD(Forum_Posts) {
	TT_THREAD_START
	int i;
	for (i=0; i < MAX_FORUMS; i++) {
		LockMutex(Forum_Mutex);
		if (forum_config.forums[i].driver) {
			if (!forum_config.forums[i].driver->Init(&forum_config.forums[i])) {
				forum_config.forums[i].driver = NULL;
				continue;
			}
			forum_config.forums[i].last_post_id = GetLastPostID(&forum_config.forums[i],"post");
			if (forum_config.forums[i].last_post_id == 0 && forum_config.forums[i].forum_id > 0) {
				forum_config.forums[i].last_post_id = forum_config.forums[i].driver->GetNewestPostID(&forum_config.forums[i]);
			}
			forum_config.forums[i].last_pm_id = GetLastPostID(&forum_config.forums[i],"pm");
			if (forum_config.forums[i].last_pm_id == 0) {
				forum_config.forums[i].last_pm_id = forum_config.forums[i].driver->GetNewestPMID(&forum_config.forums[i]);
			}

			//debug only !!!
			//forum_config.forums[i].last_post_id = 0;
			//forum_config.forums[i].last_pm_id = 0;
		}
		RelMutex(Forum_Mutex);
	}

	while (!forum_config.shutdown_now) {
		for (i=0; i < MAX_FORUMS; i++) {
			LockMutex(Forum_Mutex);
			FORUM_INSTANCE * inst = &forum_config.forums[i];
			if (inst->driver) {
				FORUM_POSTS * p = inst->driver->GetNewPosts(inst);
				if (p) {
					for (int j=0; j < p->num; j++) {
						UpdateLastPostID(inst, p->posts[j]->post_id, PostTypeToString(p->posts[j]->type));
						//api->ib_printf("Forum[%d] -> Post %u by %s", i, p->posts[j]->post_id, p->posts[j]->username);
						ProcPost(p->posts[j]);
					}
					inst->driver->FreePostList(p, false);
				}
			}
			RelMutex(Forum_Mutex);
		}

		for (int si=0; si < 15 && !forum_config.shutdown_now; si++) {
			safe_sleep(1);
		}
	}

	LockMutex(Forum_Mutex);
	for (int i=0; i < MAX_FORUMS; i++) {
		if (forum_config.forums[i].driver && forum_config.forums[i].driver->Quit) {
			forum_config.forums[i].driver->Quit(&forum_config.forums[i]);
		}
	}
	RelMutex(Forum_Mutex);

	TT_THREAD_END
}

#if !defined(IRCBOT_LITE)
void UpdateStatusPosts() {
	char buf[8192];
	STATS * s = api->ss->GetStreamInfo();
	if (s->online) {
		api->LoadMessage("ForumOnline", buf, sizeof(buf));
	} else {
		api->LoadMessage("ForumOffline", buf, sizeof(buf));
	}
	api->ProcText(buf, sizeof(buf));
	for (int i=0; i < MAX_FORUMS; i++) {
		LockMutex(Forum_Mutex);
		if (forum_config.forums[i].driver) {
			forum_config.forums[i].driver->UpdatePost(&forum_config.forums[i], forum_config.forums[i].status_post_id, buf);
		}
		RelMutex(Forum_Mutex);
	}
}
#endif
