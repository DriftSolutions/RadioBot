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

#define TWITTER_PLUGIN
#define TIXML_USE_STL
#include "twitter.h"
#include <libjson.h>
#if defined(WIN32)
	#if defined(DEBUG)
	#pragma comment(lib, "libjson_d.lib")
	#else
	#pragma comment(lib, "libjson.lib")
	#endif
#endif

const char * months[] = { "Jan", "Feb", "Mar", "Apr", "May", "Jun", "Jul", "Aug", "Sep", "Oct", "Nov", "Dec" };
int MonToInt(const char * mon) {
	for (int i=0; i < 12; i++) {
		if (!stricmp(mon, months[i])) {
			return i;
		}
	}
	return -1;
}

class Tweet {
private:
	char text[256];
	char from[256];
	uint64 id;
public:
	Tweet() { this->Reset(); }
	void Reset() {
		SetText("");
		SetFrom("");
		SetID(0);
	}

	void SetText(const char * ptext) { sstrcpy(text, ptext); }
	std::string GetText() { return this->text; };
	void SetFrom(const char * ptext) { sstrcpy(from, ptext); }
	std::string GetFrom() { return this->from; };
	void SetID(uint64 uid) { this->id = uid; }
	uint64 GetID() { return this->id; };
	//time_t timestamp;
};
//typedef std::multimap<time_t, Tweet> tweetList;

static char twitter_desc[] = "Twitter";

#if !defined(DEBUG)
void twitter_up_ref(USER_PRESENCE * u) {
#else
void twitter_up_ref(USER_PRESENCE * u, const char * fn, int line) {
	api->ib_printf("twitter_UserPres_AddRef(): %s : %s:%d\n", u->User->Nick, fn, line);
#endif
	LockMutex(TwitterMutex);
	if (u->User) { RefUser(u->User); }
	u->ref_cnt++;
	RelMutex(TwitterMutex);
}

#if !defined(DEBUG)
void twitter_up_unref(USER_PRESENCE * u) {
#else
void twitter_up_unref(USER_PRESENCE * u, const char * fn, int line) {
	api->ib_printf("twitter_UserPres_DelRef(): %s : %s:%d\n", u->Nick, fn, line);
#endif
	LockMutex(TwitterMutex);
	if (u->User) { UnrefUser(u->User); }
	u->ref_cnt--;
#if defined(DEBUG)
	api->ib_printf("twitter_UserPres_DelRef(): %s : New Refcount %d\n", u->Nick,u->ref_cnt);
#endif
	if (u->ref_cnt == 0) {
		zfree((void *)u->Nick);
		zfree((void *)u->Hostmask);
		zfree(u);
	}
	RelMutex(TwitterMutex);
}

bool send_twitter_tweet(USER_PRESENCE * ut, const char * str) {
	LockMutex(TwitterMutex);
	static char buf[256];
	bool ret = true;
	if (tw_config[ut->NetNo].enabled && strlen(str)) {
		memset(buf, 0, sizeof(buf));
		snprintf(buf, sizeof(buf)-1, "@%s %s", ut->Nick, str);
		buf[sizeof(buf)-1] = 0;
		PostTweet(ut->NetNo, buf, false);
	}
	RelMutex(TwitterMutex);
	return ret;
}
bool send_twitter_dm(USER_PRESENCE * ut, const char * tstr) {
	LockMutex(TwitterMutex);
	bool ret = true;
	if (tw_config[ut->NetNo].enabled) {
		string str = tstr;
		string to = ut->Nick;
		uint64 to_id = 0;
		if (GetUserID(ut->NetNo, ut->Nick, to_id)) {
			ret = tw_config[ut->NetNo].twitter->directMessageSend(to_id, str);
			string msg;
			if (ret) {
				tw_config[ut->NetNo].twitter->getLastWebResponse(msg);
				if (stristr(msg.c_str(), "\"errors\"")) {
					api->ib_printf("Twitter -> directMessageNew web response: %s\n", msg.c_str());
					ret = false;
				} else {
					api->ib_printf("Twitter -> Direct Message sent OK!\n");
				}
			} else {
				tw_config[ut->NetNo].twitter->getLastCurlError(msg);
				api->ib_printf("Twitter -> statusUpdate error: %s\n", msg.c_str());
				ret = false;
			}
		} else {
			api->ib_printf("Twitter -> Error sending DM, could not get user ID for %s!\n", ut->Nick);
			ret = false;
		}
	}
	RelMutex(TwitterMutex);
	return ret;
}

USER_PRESENCE * alloc_twitter_presence(USER * user, int netno, const char * username, const char * hostmask, bool dm) {
	USER_PRESENCE * ret = znew(USER_PRESENCE);
	memset(ret, 0, sizeof(USER_PRESENCE));

	ret->User = user;
	ret->Nick = zstrdup(username);
	ret->Hostmask = zstrdup(hostmask);
	ret->NetNo = netno;
	ret->Flags = user ? user->Flags:api->user->GetDefaultFlags();

	ret->send_chan_notice = dm ? send_twitter_dm:send_twitter_tweet;
	ret->send_chan = ret->send_chan_notice;
	ret->send_msg = ret->send_chan_notice;
	ret->send_notice = ret->send_chan_notice;
	ret->std_reply = ret->send_chan_notice;
	ret->Desc = twitter_desc;

	ret->ref = twitter_up_ref;
	ret->unref = twitter_up_unref;

	RefUser(ret);
	return ret;
};


void ProcTweet(int num, const char * from, const char * text, bool dm) {
	const char * msg = text;
	if (msg) {
		//msg++;
		if (api->commands->IsCommandPrefix(*msg)) {
			msg++;
			stringstream str;
			str << from << "!" << from << "@twitter.com";
			USER * user = api->user->GetUser(str.str().c_str());
			USER_PRESENCE * ut = alloc_twitter_presence(user, num, from, str.str().c_str(), dm);
			StrTokenizer st((char *)msg, ' ');
			string cmd = st.stdGetSingleTok(1);
			if (!stricmp(cmd.c_str(), "followme")) {
				LockMutex(TwitterMutex);
				string uid = from;
				if (tw_config[num].twitter->friendshipCreate(uid, false)) {
					string resp;
					tw_config[num].twitter->getLastWebResponse(resp);
					if (stristr(resp.c_str(), "\"errors\"") == NULL) {
						ut->std_reply(ut, "I am following you now...");
					} else {
						api->ib_printf("Twitter[%d]: Error following %s: %s\n", num, from, resp.c_str());
					}
				} else {
					api->ib_printf("Twitter[%d]: Error following %s: curl error\n", num, from);
				}
				RelMutex(TwitterMutex);
			} else if (!stricmp(cmd.c_str(), "unfollow")) {
				LockMutex(TwitterMutex);
				string uid = from;
				if (tw_config[num].twitter->friendshipDestroy(uid, false)) {
					string resp;
					tw_config[num].twitter->getLastWebResponse(resp);
					if (stristr(resp.c_str(), "\"errors\"") == NULL) {
						ut->std_reply(ut, "I am following you now...");
					} else {
						api->ib_printf("Twitter[%d]: Error following %s: %s\n", num, from, resp.c_str());
					}
				} else {
					api->ib_printf("Twitter[%d]: Error following %s: curl error\n", num, from);
				}
				RelMutex(TwitterMutex);
			} else {
				COMMAND * com = api->commands->FindCommand(cmd.c_str());
				uint32 type = dm ? CM_ALLOW_CONSOLE_PRIVATE:CM_ALLOW_CONSOLE_PUBLIC;
				if (com && com->proc_type == COMMAND_PROC && (com->mask & type)) {
					api->commands->ExecuteProc(com, st.stdGetTok(2, st.NumTok()).c_str(), ut, type|CM_FLAG_SLOW);
				}
			}
			UnrefUser(ut);
			if (user) { UnrefUser(user); }
		}
	}
}

void HandleTweets(int num, const char * xml) {
//	tweetList tweets;
	JSONNODE * doc = json_parse(xml);
	if (doc) {
		if (json_type(doc) == JSON_ARRAY) {
			uint64 last = tw_config[num].lastReadTweetID;
			for (json_index_t i=0; i < json_size(doc); i++) {
				JSONNODE * itm = json_at(doc, i);

				Tweet tmp;
				JSONNODE * el = json_get(itm, "text");
				if (el && json_type(el) == JSON_STRING) {
					tmp.SetText(json_as_string(el));
				} else if (el == NULL) {
					api->ib_printf("Twitter -> Error parsing JSON in timeline response: text entry not found for tweet!\n");
				} else {
					api->ib_printf("Twitter -> Error parsing JSON in timeline response: text entry not a string for tweet!\n");
				}
				el = json_get(itm, "id_str");
				if (el) {
					tmp.SetID(atou64(json_as_string(el)));
				}
				el = json_get(itm, "user");
				//char c = json_type(el);
				if (el && json_type(el) == JSON_NODE) {
					el = json_get(el, "screen_name");
					if (el) {
						tmp.SetFrom(json_as_string(el));
					}
				}

				if (tmp.GetID() && tmp.GetFrom().length() && tmp.GetText().length()) {
					if (tmp.GetID() > tw_config[num].lastReadTweetID) {
						tw_config[num].lastReadTweetID = tmp.GetID();
						stringstream str;
						str << "REPLACE INTO TwitterRead (Account, Type, ID) VALUES (" << num << ", \"tweet_id\", \"" << tmp.GetID() << "\")";
						api->db->Query(str.str().c_str(), NULL, NULL, NULL);
					}
					if (tmp.GetID() > last) {
						api->ib_printf("Tweet: " I64FMT " / %s / %s\n", tmp.GetID(), tmp.GetFrom().c_str(), tmp.GetText().c_str());
						char *p = strdup(tmp.GetText().c_str());
						if (p[0] == '@' && !strnicmp(p+1, tw_config[num].user, strlen(tw_config[num].user)) && p[strlen(tw_config[num].user)+1] == ' ') {
							api->ib_printf("Tweet2: " I64FMT " / %s / %s\n", tmp.GetID(), tmp.GetFrom().c_str(), tmp.GetText().c_str());
							ProcTweet(num, tmp.GetFrom().c_str(), p+strlen(tw_config[num].user)+2, false);
						}
						free(p);
					}
				}
			}
		} else {
			api->ib_printf("Twitter[%d] -> Error parsing JSON in timeline response: not an array!\n", num);
			api->ib_printf("Twitter[%d] -> JSON: %s\n", num, xml);
		}
		json_delete(doc);
	} else {
		api->ib_printf("Twitter[%d] -> Error parsing JSON in timeline response!\n", num);
		api->ib_printf("Twitter[%d] -> JSON: %s\n", num, xml);
	}
}

void HandleDMs(int num, const char * xml) {
	JSONNODE * doc = json_parse(xml);
	if (doc) {
		JSONNODE * doc2 = json_get(doc, "events");
		if (doc2 && json_type(doc2) == JSON_ARRAY) {
		//if (json_type(doc) == JSON_ARRAY) {
			uint64 last = tw_config[num].lastReadDMID;
			for (json_index_t i= json_size(doc2) - 1; i >= 0; i--) {
				JSONNODE * itm = json_at(doc2, i);
				if (itm == NULL) {
					break;
				}

				JSONNODE * el = json_get(itm, "type");
				if (el == NULL || strcmp(json_as_string(el), "message_create")) {
					continue;
				}

				Tweet tmp;

				JSONNODE * sub = json_get(itm, "message_create");
				if (sub == NULL || json_type(sub) != JSON_NODE) {
					continue;
				}

				el = json_get(sub, "sender_id");
				if (el) {
					uint64 user_id = atou64(json_as_string(el));
					if (user_id == tw_config[num].user_id) {
						//ignore message we sent
						continue;
					}
					string from;
					if (!GetScreenName(num, user_id, from)) {
						continue;
					}
					tmp.SetFrom(from.c_str());
				}

				el = json_get(itm, "id");
				if (el) {
					tmp.SetID(atou64(json_as_string(el)));
				}

				JSONNODE * sub2 = json_get(sub, "message_data");
				if (sub2 == NULL || json_type(sub2) != JSON_NODE) {
					continue;
				}
				el = json_get(sub2, "text");
				if (el) {
					tmp.SetText(json_as_string(el));
				}

				if (tmp.GetID() && tmp.GetFrom().length() && tmp.GetText().length()) {
					if (tmp.GetID() > tw_config[num].lastReadDMID) {
						//strcpy(tw_config[num].lastReadTweet, tmp.id.c_str());
						tw_config[num].lastReadDMID = tmp.GetID();
						stringstream str;
						str << "REPLACE INTO TwitterRead (Account, Type, ID) VALUES (" << num << ", \"dm_id\", \"" << tmp.GetID() << "\")";
						api->db->Query(str.str().c_str(), NULL, NULL, NULL);
					}
					if (tmp.GetID() > last) {
	//					const char *p = tmp.text.c_str();
//						if (p[0] == '@' && !strnicmp(p+1, tw_config[num].user, strlen(tw_config[num].user))) {
							api->ib_printf("DM: " I64FMT " / %s / %s\n", tmp.GetID(), tmp.GetFrom().c_str(), tmp.GetText().c_str());
							ProcTweet(num, tmp.GetFrom().c_str(), tmp.GetText().c_str(), true);
//						}
					}
				}
				if (i == 0) {
					break;
				}
			}
		} else {
			api->ib_printf("Twitter -> Error parsing JSON in timeline response: not an array!\n");
		}
		json_delete(doc);
	} else {
		api->ib_printf("Twitter -> Error parsing JSON in timeline response!\n");
	}
}

TT_DEFINE_THREAD(TwitterCheck) {
	TT_THREAD_START
	safe_sleep(10);
	while (!twitter_shutdown_now) {
		int i;
		api->ib_printf("Twitter -> Checking Twitter for new tweets...\n");
		for (i=0; i < MAX_ACCOUNTS; i++) {
			tw_config[0].watchdog = time(NULL);
			LockMutex(TwitterMutex);
			if (tw_config[i].enabled && tw_config[i].enable_interactive) {
				/*
				char buf[128*1024];
				memset(buf, 0, sizeof(buf));
				FILE * fp = fopen("./tmp/twitter-dm.txt", "rb");
				if (fp) {
					fread(buf, 1, sizeof(buf), fp);
					fclose(fp);
					HandleDMs(i, buf);
				}
				*/

				ostringstream parms;
				if (tw_config[i].lastReadTweetID > 0) {
					//parms << "?count=200";
					parms << "?since_id=" << tw_config[i].lastReadTweetID;
				} else {
					parms << "?count=3";
				}
				if (tw_config[i].twitter->mentionsGet(parms.str())) {
					string str;
					tw_config[i].twitter->getLastWebResponse(str);
#if defined(DEBUG)
					FILE * fp = fopen("./tmp/twitter-timeline.txt", "wb");
					if (fp) {
						fwrite(str.c_str(), str.length(), 1, fp);
						fclose(fp);
					}
#endif
					HandleTweets(i, str.c_str());
					//api->ib_printf("Twitter Response -> %s\n", str.c_str());
				}

				RelMutex(TwitterMutex);
				/* just give something else a chance to execute */
				safe_sleep(1);
				LockMutex(TwitterMutex);

				ostringstream parms2;
				/*
				if (tw_config[i].lastReadDMID > 0) {
					//parms << "?count=200";
					parms2 << "?since_id=" << tw_config[i].lastReadDMID;
				} else {
					parms2 << "?count=50";
				}
				*/
				parms2 << "?count=50";
				if (tw_config[i].twitter->directMessageGet(parms2.str())) {
					string str;
					tw_config[i].twitter->getLastWebResponse(str);
#if defined(DEBUG)
					FILE * fp = fopen("./tmp/twitter-dm.txt", "wb");
					if (fp) {
						fwrite(str.c_str(), str.length(), 1, fp);
						fclose(fp);
					}
#endif
					HandleDMs(i, str.c_str());
					//api->ib_printf("Twitter Response -> %s\n", str.c_str());
				}

			}
			RelMutex(TwitterMutex);
		}
		for (i=0; i < 120 && !twitter_shutdown_now; i++) {
			safe_sleep(1);
		}
	}
	TT_THREAD_END
}

bool GetScreenName(int ind, uint64 user_id, string& screen_name) {
	if (user_id <= 0) {
		return false;
	}

	{
		stringstream str;
		str << "SELECT * FROM TwitterUsers WHERE UserID=" << user_id;
		sql_rows ret = GetTable(str.str().c_str());
		if (ret.size() > 0) {
			screen_name = ret[0]["ScreenName"];
			return true;
		}
	}

	AutoMutex(TwitterMutex);

	bool ret = false;
	if (ind >= 0 && ind < MAX_ACCOUNTS && tw_config[ind].enabled && tw_config[ind].twitter != NULL) {
		if (tw_config[ind].twitter->getUserInfoByID(user_id)) {
			string str;
			tw_config[ind].twitter->getLastWebResponse(str);
			JSONNODE * doc = json_parse(str.c_str());
			if (doc) {
				JSONNODE * el = json_get(doc, "screen_name");
				if (el) {
					screen_name = json_as_string(el);
					stringstream str;
					char * tmp = api->db->MPrintf("%Q", screen_name.c_str());
					str << "REPLACE INTO TwitterUsers (UserID, ScreenName) VALUES (" << user_id << ", " << tmp << ")";
					api->db->Free(tmp);
					api->db->Query(str.str().c_str(), NULL, NULL, NULL);
					ret = true;
				} else {
					api->ib_printf("Twitter -> No 'screen_name' for user ID " I64FMT "\n", user_id);
				}
				json_delete(doc);
			} else {
				api->ib_printf("Twitter -> Error getting screen name for user ID " I64FMT "\n", user_id);
			}

		}
	}
	return ret;
}

bool GetUserID(int ind, const string screen_name, uint64& user_id) {
	if (screen_name.length() <= 0) {
		return false;
	}

	{
		stringstream str;
		char * tmp = api->db->MPrintf("%Q", screen_name.c_str());
		str << "SELECT * FROM TwitterUsers WHERE ScreenName=" << tmp;
		api->db->Free(tmp);
		sql_rows ret = GetTable(str.str().c_str());
		if (ret.size() > 0) {
			user_id = atou64(ret[0]["UserID"].c_str());
			return true;
		}
	}

	AutoMutex(TwitterMutex);

	bool ret = false;
	if (ind >= 0 && ind < MAX_ACCOUNTS && tw_config[ind].enabled && tw_config[ind].twitter != NULL) {
		if (tw_config[ind].twitter->getUserInfoByScreenName(screen_name)) {
			string str;
			tw_config[ind].twitter->getLastWebResponse(str);
			JSONNODE * doc = json_parse(str.c_str());
			if (doc) {
				JSONNODE * el = json_get(doc, "id_str");
				if (el) {
					user_id = atou64(json_as_string(el));
					stringstream str;
					char * tmp = api->db->MPrintf("%Q", screen_name.c_str());
					str << "REPLACE INTO TwitterUsers (UserID, ScreenName) VALUES (" << user_id << ", " << tmp << ")";
					api->db->Free(tmp);
					api->db->Query(str.str().c_str(), NULL, NULL, NULL);
					ret = true;
				} else {
					api->ib_printf("Twitter -> No 'id_str' for user %s\n", screen_name.c_str());
				}
				json_delete(doc);
			} else {
				api->ib_printf("Twitter -> Error getting user ID for user %s\n", screen_name.c_str());
			}

		}
	}
	return ret;
}
