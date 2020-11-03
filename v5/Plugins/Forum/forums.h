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

#ifndef ENABLE_MYSQL
#define ENABLE_MYSQL
#endif
#include "../../src/plugins.h"

extern BOTAPI_DEF *api;
extern int pluginnum;
//extern SQLConx conx;
#define MAX_FORUMS 4
#define MAX_FORUM_POSTS 256

struct FORUM_INSTANCE;

enum FORUM_POST_TYPES {
	FPT_POST,
	FPT_PM
};
struct FORUM_POST {
	FORUM_POST_TYPES type;
	const char * post_text;

	union {
		uint32 post_id;
		uint32 msg_id;
	};
	uint32 topic_id;
	uint32 forum_id;
	uint32 user_id;
	char username[64];

	char * outbuf;

	FORUM_INSTANCE * inst;
	//time_t lastOut;
};

struct FORUM_POSTS {
	FORUM_POST * posts[MAX_FORUM_POSTS];
	int num;
};

struct FORUM_DRIVER {
	const char * name;

	bool (*Init)(FORUM_INSTANCE * inst);
	uint32 (*GetNewestPostID)(FORUM_INSTANCE * inst);
	uint32 (*GetNewestPMID)(FORUM_INSTANCE * inst);

	FORUM_POSTS * (*GetNewPosts)(FORUM_INSTANCE * inst);
	void (*FreePost)(FORUM_POST * post);
	void (*FreePostList)(FORUM_POSTS * list, bool free_posts);

	void (*PostToThread)(FORUM_POST * po);
	void (*SendPM)(FORUM_POST * po);
	void (*UpdatePost)(FORUM_INSTANCE * inst, uint32 id, const char * new_text);
	void (*Quit)(FORUM_INSTANCE * inst);
};

struct FORUM_INSTANCE {
	int id;
	FORUM_DRIVER * driver;
	DB_MySQL * conx;

	struct {
		char host[128];
		char user[128];
		char pass[128];
		int port;
		char dbname[128];
	} db;

	struct {
		uint32 user_id;
		char username[64];
		char usercolor[16];
	} me;

	char tabprefix[64];
	uint32 forum_id;
	uint32 status_post_id;
	uint32 last_pm_id;
	uint32 last_post_id;

	union {
		char tbl_topics[64];
		char tbl_threads[64];
	};
	char tbl_posts[64];
	char tbl_users[64];
	char tbl_forums[64];
	union {
		char tbl_privmsgs_to[64];
		char tbl_pm[64];
	};
	union {
		char tbl_privmsgs[64];
		char tbl_pmtext[64];
	};
};

struct FORUM_CONFIG {
	bool shutdown_now;

	FORUM_INSTANCE forums[MAX_FORUMS];

};

extern FORUM_CONFIG forum_config;

TT_DEFINE_THREAD(Forum_Posts);

void UpdatePost(uint32 id, const char * new_text);
void UpdateStatusPosts();
