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

//Titus_Mutex vBulletin_Mutex;

bool vbulletin_GetMyInfo(FORUM_INSTANCE * inst) {
	stringstream sstr;
	sstr << "SELECT username FROM " << inst->tbl_users << " WHERE userid=" << inst->me.user_id << " LIMIT 1";
	MYSQL_RES * res = inst->conx->Query(sstr.str().c_str());
	bool ret = false;
	if (res && inst->conx->NumRows(res) > 0) {
		SC_Row row;
		if (inst->conx->FetchRow(res, row)) {
			snprintf(inst->me.username, sizeof(inst->me.username), "%s", row.Get("username").c_str());
			//snprintf(inst->me.usercolor, sizeof(inst->me.usercolor), "%s", row.Get("user_colour"));
			ret = true;
		}
	}
	inst->conx->FreeResult(res);
	return ret;
}

bool vbulletin_Init(FORUM_INSTANCE * inst) {

	snprintf(inst->tbl_posts, sizeof(inst->tbl_posts), "%spost", inst->tabprefix);
	snprintf(inst->tbl_threads, sizeof(inst->tbl_topics), "%sthread", inst->tabprefix);
	snprintf(inst->tbl_forums, sizeof(inst->tbl_forums), "%sforum", inst->tabprefix);
	snprintf(inst->tbl_users, sizeof(inst->tbl_users), "%suser", inst->tabprefix);
	snprintf(inst->tbl_privmsgs_to, sizeof(inst->tbl_pm), "%spm", inst->tabprefix);
	snprintf(inst->tbl_privmsgs, sizeof(inst->tbl_pmtext), "%spmtext", inst->tabprefix);

	api->ib_printf(_("Forum[%d] -> Connecting to your MySQL server...\n"), inst->id);
	inst->conx = new DB_MySQL();
	int tries=0;
	bool connected=false;
	while (1) {
		if (inst->conx->Connect(inst->db.host, inst->db.user, inst->db.pass, inst->db.dbname, inst->db.port)) {
			connected = true;
			break;
		} else {
			if (tries < 5) {
				api->ib_printf(_("Forum[%d] -> Error connecting to your MySQL server, will try again in 5 seconds...\n"), inst->id);
				safe_sleep(5);
			} else {
				break;
			}
			tries++;
		}
	}

	if (!connected) {
		api->ib_printf(_("Forum[%d] -> Error connecting to your MySQL server, disabling this account...\n"), inst->id);
		delete inst->conx;
		inst->conx = NULL;
		return false;
	}

	if (vbulletin_GetMyInfo(inst)) {
		api->ib_printf(_("Forum[%d] -> My username is %s\n"), inst->id, inst->me.username);
	} else {
		api->ib_printf(_("Forum[%d] -> Error reading my user info! (is UserID correct?)\n"), inst->id);
		delete inst->conx;
		inst->conx = NULL;
		return false;
	}

	return true;
}

void vbulletin_Quit(FORUM_INSTANCE * inst) {
	if (inst->conx) {
		delete inst->conx;
		inst->conx = NULL;
	}
}

uint32 vbulletin_GetNewestPostID(FORUM_INSTANCE * inst) {
	stringstream sstr;
	sstr << "SELECT lastpostid FROM " << inst->tbl_threads << " WHERE forumid=" << inst->forum_id << " ORDER BY lastpost DESC LIMIT 1";
	MYSQL_RES * res = inst->conx->Query(sstr.str().c_str());
	uint32 ret = 0;
	if (res && inst->conx->NumRows(res) > 0) {
		SC_Row row;
		if (inst->conx->FetchRow(res, row)) {
			ret = atoul(row.Get("lastpostid").c_str());
		}
	}
	inst->conx->FreeResult(res);
	return ret;
}

uint32 vbulletin_GetNewestPMID(FORUM_INSTANCE * inst) {
	stringstream sstr;
	sstr << "SELECT pmid FROM " << inst->tbl_pm << " WHERE userid=" << inst->me.user_id << " ORDER BY pmid DESC LIMIT 1";
	MYSQL_RES * res = inst->conx->Query(sstr.str().c_str());
	uint32 ret = 0;
	if (res && inst->conx->NumRows(res) > 0) {
		SC_Row row;
		if (inst->conx->FetchRow(res, row)) {
			ret = atoul(row.Get("pmid").c_str());
		}
	}
	inst->conx->FreeResult(res);
	return ret;
}

bool vbulletin_GetForumUsername(FORUM_INSTANCE * inst, int user_id, char * username, size_t userSize) {
	stringstream sstr;
	sstr << "SELECT username FROM " << inst->tbl_users << " WHERE userid=" << user_id << " LIMIT 1";
	MYSQL_RES * res = inst->conx->Query(sstr.str().c_str());
	bool ret = false;
	if (res && inst->conx->NumRows(res) > 0) {
		SC_Row row;
		if (inst->conx->FetchRow(res, row)) {
			snprintf(username, userSize, "%s", row.Get("username").c_str());
			str_replace(username, userSize, " ", "_");
			ret = true;
		}
	}
	inst->conx->FreeResult(res);
	return ret;
}

void vbulletin_UpdatePost(FORUM_INSTANCE * inst, uint32 id, const char * new_text) {
	stringstream sstr;
	char md5buf[33], md5buf2[33];
	uint32 x = api->genrand_int32();
	hashdata("md5", (uint8 *)&x, sizeof(x), md5buf, sizeof(md5buf));
	//md5((unsigned char *)&x, sizeof(x), md5buf);
	md5buf[8] = 0;
	hashdata("md5", (uint8 *)new_text, strlen(new_text), md5buf2, sizeof(md5buf2));
	sstr << "UPDATE " << inst->tbl_posts << " SET pagetext=\"" << inst->conx->EscapeString(new_text) << "\"  WHERE postid=" << id;
	inst->conx->Query(sstr.str().c_str());
}

void vbulletin_PostToThread(FORUM_POST * po) {
	FORUM_INSTANCE * inst = po->inst;
	stringstream sstr1;
	sstr1 << "INSERT INTO " << inst->tbl_posts << " (`threadid`, `userid`, `username`, `ipaddress`, `dateline` , `title`, `pagetext`, `visible`, `showsignature`, `allowsmilie`) VALUES (" << po->topic_id << ", " << inst->me.user_id << ", \"" << inst->conx->EscapeString(inst->me.username) << "\"";
	sstr1 << ", \"" << inst->conx->EscapeString(api->GetIP()) << "\"";
	time_t tme = time(NULL);
	sstr1 << ", " << tme;
	sstr1 << ", \"" << inst->conx->EscapeString("Bot Reply") << "\"";
	sstr1 << ", \"" << inst->conx->EscapeString(po->outbuf) << "\"";
	sstr1 << ", 1, 1, 0)";
	inst->conx->Query(sstr1.str().c_str());
	uint32 post_id = inst->conx->InsertID();
	if (post_id > 0) {
		stringstream sstr2,sstr3,sstr4;
		sstr2 << "UPDATE " << inst->tbl_topics << " SET replycount=replycount+1, lastpostid=" << post_id << ", lastposter=\"" << inst->conx->EscapeString(inst->me.username) << "\", lastpost=" << tme << " WHERE threadid=" << po->topic_id;
		sstr3 << "UPDATE " << inst->tbl_forums << " SET replycount=replycount+1, lastpost=" << tme << ", lastposter=\"" << inst->conx->EscapeString(inst->me.username) << "\" WHERE forumid=" << po->forum_id;
		sstr4 << "UPDATE " << inst->tbl_users << " SET posts=posts+1, lastvisit=" << tme << ", lastactivity=" << tme << ", lastpost=" << tme << ", lastpostid=" << post_id << " WHERE userid=" << inst->me.user_id;
		inst->conx->Query(sstr2.str().c_str());
		inst->conx->Query(sstr3.str().c_str());
		inst->conx->Query(sstr4.str().c_str());
	} else {
		api->ib_printf(_("Forum[%d] -> Error posting: %s\n"), inst->id, inst->conx->GetErrorString());
	}
}

void vbulletin_SendPM(FORUM_POST * po) {
	FORUM_INSTANCE * inst = po->inst;
	stringstream sstr1;
	time_t tme = time(NULL);
	sstr1 << "INSERT INTO " << inst->tbl_pmtext << " (`fromuserid`, `fromusername`, `dateline`, `showsignature`, `allowsmilie` , `title`, `message`, `touserarray`) VALUES (" << inst->me.user_id << ", \"" << inst->conx->EscapeString(inst->me.username) << "\", " << tme << ", 1, 0";
	sstr1 << ", \"" << inst->conx->EscapeString("Bot Reply") << "\"";
	sstr1 << ", \"" << inst->conx->EscapeString(po->outbuf) << "\"";
	sstr1 << ", \"a:1:{i:" << po->user_id << ";s:" << strlen(po->username) << ":\\\"" << inst->conx->EscapeString(po->username) << "\\\";}\")";
	inst->conx->Query(sstr1.str().c_str());
	uint32 msg_id = inst->conx->InsertID();
	if (msg_id > 0) {
		stringstream sstr2,sstr3;
		sstr2 << "INSERT INTO " << inst->tbl_pm << " (`pmtextid`,`userid`,`folderid`) VALUES (" << msg_id << ", " << po->user_id << ", 0)";
		sstr3 << "UPDATE " << inst->tbl_users << " SET pmtotal=pmtotal+1, pmunread=pmunread+1 WHERE userid=" << po->user_id;
		inst->conx->Query(sstr2.str().c_str());
		inst->conx->Query(sstr3.str().c_str());
	} else {
		api->ib_printf(_("Forum[%d] -> Error PMing: %s\n"), inst->id, inst->conx->GetErrorString());
	}
}

FORUM_POSTS * vbulletin_GetNewPosts(FORUM_INSTANCE * inst) {
	FORUM_POSTS * ret = znew(FORUM_POSTS);
	memset(ret, 0, sizeof(FORUM_POSTS));
	MYSQL_RES * res;
	if (inst->forum_id > 0) {
		stringstream sstr;
		sstr << "SELECT * FROM " << inst->tbl_threads << " WHERE forumid=" << inst->forum_id << " AND lastpostid>" << inst->last_post_id << " ORDER BY lastpostid ASC";
		res = inst->conx->Query(sstr.str().c_str());
		if (res && inst->conx->NumRows(res) > 0) {
			SC_Row row2;
			while (inst->conx->FetchRow(res, row2)) {
				uint32 tid = atoul(row2.Get("threadid").c_str());
				stringstream sstr2;
				sstr2 << "SELECT * FROM " << inst->tbl_posts << " WHERE threadid=" << tid << " AND postid>" << inst->last_post_id << " AND userid!=" << inst->me.user_id << " ORDER BY postid ASC";
				MYSQL_RES * res2 = inst->conx->Query(sstr2.str().c_str());
				if (res2 && inst->conx->NumRows(res2) > 0) {
					SC_Row row;
					while (inst->conx->FetchRow(res2, row)) {
						FORUM_POST * post = znew(FORUM_POST);
						memset(post, 0, sizeof(FORUM_POST));
						post->type = FPT_POST;
						post->inst = inst;
						post->post_id = atoul(row.Get("postid").c_str());
						post->topic_id = atoul(row.Get("threadid").c_str());
						post->forum_id = inst->forum_id;
						post->user_id = atoul(row.Get("userid").c_str());
						sstrcpy(post->username, row.Get("username").c_str());
						if (post->username[0] == 0 && post->user_id > 0) {
							vbulletin_GetForumUsername(inst, post->user_id, post->username, sizeof(post->username));
						}
						string pt = row.Get("pagetext");
						if (post->username[0] && pt.length()) {
							inst->last_post_id = post->post_id;
							post->post_text = zstrdup(pt.c_str());
							ret->posts[ret->num++] = post;
						} else {
							api->ib_printf("Forum[%d] -> Skipping post %u, could not retrieve poster info and/or message text...", inst->id, post->post_id);
							zfree(post);
						}
					}
				}
				inst->conx->FreeResult(res2);
			}
		}
		inst->conx->FreeResult(res);
	}

	stringstream sstr2;
	sstr2 << "SELECT * FROM " << inst->tbl_pm << " WHERE userid=" << inst->me.user_id << " AND pmid>" << inst->last_pm_id << " ORDER BY pmid ASC";
	res = inst->conx->Query(sstr2.str().c_str());
	if (res && inst->conx->NumRows(res) > 0) {
		SC_Row row;
		while (inst->conx->FetchRow(res, row)) {
			FORUM_POST * post = znew(FORUM_POST);
			memset(post, 0, sizeof(FORUM_POST));
			post->type = FPT_PM;
			post->inst = inst;
			post->msg_id = atoul(row.Get("pmid").c_str());
			uint32 textid = atoul(row.Get("pmtextid").c_str());
			stringstream sstr3;
			sstr3 << "SELECT * FROM " << inst->tbl_pmtext << " WHERE pmtextid=" << textid;
			MYSQL_RES * res2 = inst->conx->Query(sstr3.str().c_str());
			if (res2 && inst->conx->NumRows(res) > 0) {
				SC_Row row2;
				if (inst->conx->FetchRow(res2, row2)) {
					post->user_id = atoul(row2.Get("fromuserid").c_str());
					sstrcpy(post->username, row2.Get("fromusername").c_str());
					string pt = row2.Get("message");
					if (pt.length()) {
						inst->last_pm_id = post->msg_id;
						post->post_text = zstrdup(pt.c_str());
						ret->posts[ret->num++] = post;
						post = NULL;
					} else {
						api->ib_printf("Forum[%d] -> Skipping PM %u, could not retrieve message text...\n", inst->id, post->msg_id);
					}
				} else {
					api->ib_printf("Forum[%d] -> Skipping PM %u, could not retrieve message text...\n", inst->id, post->msg_id);
				}
			}
			inst->conx->FreeResult(res2);
			zfreenn(post);
		}
	}
	inst->conx->FreeResult(res);
	return ret;
}

void vbulletin_FreePost(FORUM_POST * post) {
	zfreenn((void *)post->post_text);
	zfree(post);
}
void vbulletin_FreePostList(FORUM_POSTS * list, bool free_posts){
	if (free_posts) {
		for (int i=0; i < list->num; i++) {
			vbulletin_FreePost(list->posts[i]);
		}
	}
	zfree(list);
}

FORUM_DRIVER driver_vbulletin = {
	"vBulletin",

	vbulletin_Init,
	vbulletin_GetNewestPostID,
	vbulletin_GetNewestPMID,

	vbulletin_GetNewPosts,//FORUM_POSTS * (*GetNewPosts)(FORUM_INSTANCE * inst);
	vbulletin_FreePost,
	vbulletin_FreePostList,

	vbulletin_PostToThread,
	vbulletin_SendPM,
	vbulletin_UpdatePost,
	vbulletin_Quit
};
