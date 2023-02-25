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

//Titus_Mutex phpBB_Mutex;

bool phpbb_GetMyInfo(FORUM_INSTANCE * inst) {
	stringstream sstr;
	sstr << "SELECT username,user_colour FROM " << inst->tbl_users << " WHERE user_id=" << inst->me.user_id << " LIMIT 1";
	MYSQL_RES * res = inst->conx->Query(sstr.str().c_str());
	bool ret = false;
	if (res && inst->conx->NumRows(res) > 0) {
		SC_Row row;
		if (inst->conx->FetchRow(res, row)) {
			snprintf(inst->me.username, sizeof(inst->me.username), "%s", row.Get("username").c_str());
			snprintf(inst->me.usercolor, sizeof(inst->me.usercolor), "%s", row.Get("user_colour").c_str());
			ret = true;
		}
	}
	inst->conx->FreeResult(res);
	return ret;
}

bool phpbb_Init(FORUM_INSTANCE * inst) {

	snprintf(inst->tbl_posts, sizeof(inst->tbl_posts), "%sposts", inst->tabprefix);
	snprintf(inst->tbl_topics, sizeof(inst->tbl_topics), "%stopics", inst->tabprefix);
	snprintf(inst->tbl_forums, sizeof(inst->tbl_forums), "%sforums", inst->tabprefix);
	snprintf(inst->tbl_users, sizeof(inst->tbl_users), "%susers", inst->tabprefix);
	snprintf(inst->tbl_privmsgs_to, sizeof(inst->tbl_privmsgs_to), "%sprivmsgs_to", inst->tabprefix);
	snprintf(inst->tbl_privmsgs, sizeof(inst->tbl_privmsgs), "%sprivmsgs", inst->tabprefix);

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

	if (phpbb_GetMyInfo(inst)) {
		api->ib_printf("Forum[%d] -> My username is %s, color: %s\n", inst->id, inst->me.username, inst->me.usercolor);
	} else {
		api->ib_printf("Forum[%d] -> Error reading my user info! (is UserID correct?)\n", inst->id);
		delete inst->conx;
		inst->conx = NULL;
		return false;
	}

	return true;
}

void phpbb_Quit(FORUM_INSTANCE * inst) {
	if (inst->conx) {
		delete inst->conx;
		inst->conx = NULL;
	}
}

uint32 phpbb_GetNewestPostID(FORUM_INSTANCE * inst) {
	stringstream sstr;
	sstr << "SELECT post_id FROM " << inst->tbl_posts << " WHERE forum_id=" << inst->forum_id << " ORDER BY post_id DESC LIMIT 1";
	MYSQL_RES * res = inst->conx->Query(sstr.str().c_str());
	uint32 ret = 0;
	if (res && inst->conx->NumRows(res) > 0) {
		SC_Row row;
		if (inst->conx->FetchRow(res, row)) {
			ret = atoul(row.Get("post_id").c_str());
		}
	}
	inst->conx->FreeResult(res);
	return ret;
}

uint32 phpbb_GetNewestPMID(FORUM_INSTANCE * inst) {
	stringstream sstr;
	sstr << "SELECT msg_id FROM " << inst->tbl_privmsgs_to << " WHERE user_id=" << inst->me.user_id << " ORDER BY msg_id DESC LIMIT 1";
	MYSQL_RES * res = inst->conx->Query(sstr.str().c_str());
	uint32 ret = 0;
	if (res && inst->conx->NumRows(res) > 0) {
		SC_Row row;
		if (inst->conx->FetchRow(res, row)) {
			ret = atoul(row.Get("msg_id").c_str());
		}
	}
	inst->conx->FreeResult(res);
	return ret;
}

bool phpbb_GetForumUsername(FORUM_INSTANCE * inst, int user_id, char * username, size_t userSize) {
	stringstream sstr;
	sstr << "SELECT username FROM " << inst->tbl_users << " WHERE user_id=" << user_id << " LIMIT 1";
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

void phpbb_UpdatePost(FORUM_INSTANCE * inst, uint32 id, const char * new_text) {
	stringstream sstr;
	char md5buf[33], md5buf2[33];
	uint32 x = api->genrand_int32();
	hashdata("md5", (uint8 *)&x, sizeof(x), md5buf, sizeof(md5buf));
	//md5((unsigned char *)&x, sizeof(x), md5buf);
	md5buf[8] = 0;
	hashdata("md5", (uint8 *)new_text, strlen(new_text), md5buf2, sizeof(md5buf2));
	sstr << "UPDATE " << inst->tbl_posts << " SET post_text=\"" << inst->conx->EscapeString(new_text) << "\", bbcode_uid=\"" << md5buf << "\", post_checksum=\"" << md5buf2 << "\"  WHERE post_id=" << id;
	inst->conx->Query(sstr.str().c_str());
}

void phpbb_PostToThread(FORUM_POST * po) {
	FORUM_INSTANCE * inst = po->inst;
	stringstream sstr1;
	sstr1 << "INSERT INTO " << inst->tbl_posts << " (`topic_id`, `forum_id`, `poster_id`, `post_username`, `poster_ip`, `post_time` , `post_subject`, `post_text`, `post_approved`, `enable_bbcode`, `enable_smilies`) VALUES (" << po->topic_id << ", " << po->forum_id << ", " << inst->me.user_id << ", \"\"";
	sstr1 << ", \"" << inst->conx->EscapeString(api->GetIP()) << "\"";
	time_t tme = time(NULL);
	sstr1 << ", " << tme;
	sstr1 << ", \"" << inst->conx->EscapeString("Bot Reply") << "\"";
	sstr1 << ", \"" << inst->conx->EscapeString(po->outbuf) << "\"";
	sstr1 << ", 1, 0, 0)";
	inst->conx->Query(sstr1.str().c_str());
	uint32 post_id = inst->conx->InsertID();
	if (post_id > 0) {
		stringstream sstr2,sstr3,sstr4;
		sstr2 << "UPDATE " << inst->tbl_topics << " SET topic_replies=topic_replies+1, topic_replies_real=topic_replies_real+1, topic_last_post_id=" << post_id << ", topic_last_poster_id=" << inst->me.user_id << ", topic_last_post_subject=\"Bot Reply\", topic_last_post_time=" << tme << ", topic_last_poster_name=\"" << inst->conx->EscapeString(inst->me.username) << "\", topic_last_poster_colour=\"" << inst->conx->EscapeString(inst->me.usercolor) << "\" WHERE topic_id=" << po->topic_id;
		sstr3 << "UPDATE " << inst->tbl_forums << " SET forum_posts=forum_posts+1, forum_last_post_id=" << post_id << ", forum_last_poster_id=" << inst->me.user_id << ", forum_last_poster_name=\"" << inst->conx->EscapeString(inst->me.username) << "\", forum_last_poster_colour=\"" << inst->conx->EscapeString(inst->me.usercolor) << "\", forum_last_post_subject=\"Bot Reply\", forum_last_post_time=" << tme << " WHERE forum_id=" << po->forum_id;
		sstr4 << "UPDATE " << inst->tbl_users << " SET user_posts=user_posts+1, user_lastvisit=" << tme << ", user_lastpost_time=" << tme << " WHERE user_id=" << inst->me.user_id;
		inst->conx->Query(sstr2.str().c_str());
		inst->conx->Query(sstr3.str().c_str());
		inst->conx->Query(sstr4.str().c_str());
	} else {
		api->ib_printf("Forum[%d] -> Error posting: %s\n", inst->id, inst->conx->GetErrorString().c_str());
	}
}

void phpbb_SendPM(FORUM_POST * po) {
	FORUM_INSTANCE * inst = po->inst;
	stringstream sstr1;
	time_t tme = time(NULL);
	sstr1 << "INSERT INTO " << inst->tbl_privmsgs << " (`author_id`, `author_ip`, `message_time`, `enable_bbcode`, `enable_smilies` , `message_subject`, `message_text`, `to_address`) VALUES (" << inst->me.user_id << ", \"" << inst->conx->EscapeString(api->GetIP()) << "\", " << tme << ", 0, 0";
	sstr1 << ", \"" << inst->conx->EscapeString("Bot Reply") << "\"";
	sstr1 << ", \"" << inst->conx->EscapeString(po->outbuf) << "\"";
	sstr1 << ", \"u_" << po->user_id << "\")";
	inst->conx->Query(sstr1.str().c_str());
	uint32 msg_id = inst->conx->InsertID();
	if (msg_id > 0) {
		stringstream sstr2,sstr3;
		sstr2 << "INSERT INTO " << inst->tbl_privmsgs_to << " (`msg_id`,`user_id`,`author_id`,`pm_new`,`pm_unread`,`folder_id`) VALUES (" << msg_id << ", " << po->user_id << ", " << inst->me.user_id << ", 1, 1, 0)";
		sstr3 << "UPDATE " << inst->tbl_users << " SET user_new_privmsg=user_new_privmsg+1, user_unread_privmsg=user_unread_privmsg+1 WHERE user_id=" << po->user_id;
		inst->conx->Query(sstr2.str().c_str());
		inst->conx->Query(sstr3.str().c_str());
	} else {
		api->ib_printf("Forum[%d] -> Error PMing: %s\n", inst->id, inst->conx->GetErrorString().c_str());
	}
}

FORUM_POSTS * phpbb_GetNewPosts(FORUM_INSTANCE * inst) {
	FORUM_POSTS * ret = znew(FORUM_POSTS);
	memset(ret, 0, sizeof(FORUM_POSTS));

	MYSQL_RES * res;
	if (inst->forum_id > 0) {
		stringstream sstr;
		sstr << "SELECT * FROM " << inst->tbl_posts << " WHERE forum_id=" << inst->forum_id << " AND post_id>" << inst->last_post_id << " AND poster_id!=" << inst->me.user_id << " ORDER BY post_id ASC";
		res = inst->conx->Query(sstr.str().c_str());
		if (res && inst->conx->NumRows(res) > 0) {
			SC_Row row;
			while (inst->conx->FetchRow(res, row)) {
				FORUM_POST * post = znew(FORUM_POST);
				memset(post, 0, sizeof(FORUM_POST));
				post->type = FPT_POST;
				post->inst = inst;
				post->post_id = atoul(row.Get("post_id").c_str());
				post->topic_id = atoul(row.Get("topic_id").c_str());
				post->forum_id = atoul(row.Get("forum_id").c_str());
				post->user_id = atoul(row.Get("poster_id").c_str());
				if (post->user_id <= 1) {
					sstrcpy(post->username, row.Get("post_username").c_str());
				}
				if (post->username[0] == 0 && post->user_id > 0) {
					phpbb_GetForumUsername(inst, post->user_id, post->username, sizeof(post->username));
				}
				string pt = row.Get("post_text");
				if (post->username[0] && pt.length()) {
					inst->last_post_id = post->msg_id;
					post->post_text = zstrdup(pt.c_str());
					ret->posts[ret->num++] = post;
				} else {
					api->ib_printf("Forum[%d] -> Skipping post %u, could not retrieve poster info and/or message text...", inst->id, post->post_id);
					zfree(post);
				}
			}
		}
		inst->conx->FreeResult(res);
	}
	stringstream sstr2;
	sstr2 << "SELECT * FROM " << inst->tbl_privmsgs_to << " WHERE user_id=" << inst->me.user_id << " AND msg_id>" << inst->last_pm_id << " AND author_id!=" << inst->me.user_id << " ORDER BY msg_id ASC";
	res = inst->conx->Query(sstr2.str().c_str());
	if (res && inst->conx->NumRows(res) > 0) {
		SC_Row row;
		while (inst->conx->FetchRow(res, row)) {
			FORUM_POST * post = znew(FORUM_POST);
			memset(post, 0, sizeof(FORUM_POST));
			post->type = FPT_PM;
			post->inst = inst;
			post->msg_id = atoul(row.Get("msg_id").c_str());
			post->user_id = atoul(row.Get("author_id").c_str());
			if (phpbb_GetForumUsername(inst, post->user_id, post->username, sizeof(post->username)) && post->username[0]) {
				stringstream sstr3;
				sstr3 << "SELECT * FROM " << inst->tbl_privmsgs << " WHERE msg_id=" << post->msg_id;
				MYSQL_RES * res2 = inst->conx->Query(sstr3.str().c_str());
				if (res2 && inst->conx->NumRows(res) > 0) {
					SC_Row row2;
					if (inst->conx->FetchRow(res2, row2)) {
						string pt = row2.Get("message_text");
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
			} else {
				api->ib_printf("Forum[%d] -> Skipping PM %u, could not retrieve poster info...\n", inst->id, post->msg_id);
			}
			zfreenn(post);
		}
	}
	inst->conx->FreeResult(res);
	return ret;
}

void phpbb_FreePost(FORUM_POST * post) {
	zfreenn((void *)post->post_text);
	zfree(post);
}
void phpbb_FreePostList(FORUM_POSTS * list, bool free_posts){
	if (free_posts) {
		for (int i=0; i < list->num; i++) {
			phpbb_FreePost(list->posts[i]);
		}
	}
	zfree(list);
}

FORUM_DRIVER driver_phpbb = {
	"phpBB",

	phpbb_Init,
	phpbb_GetNewestPostID,
	phpbb_GetNewestPMID,

	phpbb_GetNewPosts,//FORUM_POSTS * (*GetNewPosts)(FORUM_INSTANCE * inst);
	phpbb_FreePost,
	phpbb_FreePostList,

	phpbb_PostToThread,
	phpbb_SendPM,
	phpbb_UpdatePost,
	phpbb_Quit
};
