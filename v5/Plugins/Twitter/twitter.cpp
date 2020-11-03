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
#include "twitter.h"

BOTAPI_DEF *api=NULL;
int pluginnum; // the number we were assigned

/*
bool first_run = true;
char last_title[520]={0};
time_t startTime=0;
int songLen = 0;
IBM_SONGINFO si={0};
*/

TWITTER_CONFIG tw_config[MAX_ACCOUNTS];
TWITTER_TIMERS tw_timers[MAX_TWITTER_TIMERS];
bool twitter_enabled = false;
bool twitter_shutdown_now = false;
Titus_Mutex TwitterMutex;

class Tweet {
public:
	int ind;
	string msg;

	Tweet() { ind = -1; }
};
typedef std::vector<Tweet> tweetQueueType;
tweetQueueType tweetQueue;

void stream(unsigned char * out, const unsigned char * in, int cnt) {
	for (int i=0; i < cnt; i++) {
		out[i] = in[i] ^ 0xF8;//(0x31|(i*2));
	}
}

sql_rows GetTable(const char * query, char **errmsg) {
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
		api->ib_printf(_("GetTable: %s -> %d (%s)\n"), query, ret, *errmsg2);
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

bool CheckConfig(int i) {
	if (!strlen(tw_config[i].user)) {
		return false;
	}
	if (!strlen(tw_config[i].token_key) || !strlen(tw_config[i].token_secret)) {
		return false;
	}

	twitter_enabled = tw_config[i].enabled = true;
	tw_config[i].twitter = new twitCurl();
	tw_config[i].twitter->getOAuth().setConsumerKey(CONSUMER_KEY);
	char buf[128];
	memset(buf, 0, sizeof(buf));
	int n = base64_decode(CONSUMER_SECRET, strlen(CONSUMER_SECRET), buf);
	buf[n] = 0;
	stream((unsigned char *)buf, (unsigned char *)buf, n);
	tw_config[i].twitter->getOAuth().setConsumerSecret(buf);
	memset(buf, 0, sizeof(buf));
	tw_config[i].twitter->getOAuth().setOAuthTokenKey(tw_config[i].token_key);
	tw_config[i].twitter->getOAuth().setOAuthTokenSecret(tw_config[i].token_secret);
	string tmp;
	tmp = tw_config[i].user;
	tw_config[i].twitter->setTwitterUsername(tmp);

	if (!GetUserID(i, tmp, tw_config[i].user_id)) {
		return false;
	}
	api->ib_printf("Twitter -> Got my user ID for %s: " U64FMT "\n", tw_config[i].user, tw_config[i].user_id);

	stringstream str;
	str << "SELECT Type,ID FROM TwitterRead WHERE Account=" << i;
	sql_rows ret = GetTable(str.str().c_str(), NULL);
	if (ret.size() > 0) {
		for (sql_rows::const_iterator x = ret.begin(); x != ret.end(); x++) {
			if (!stricmp(x->second.find("Type")->second.c_str(), "dm_id")) {
				tw_config[i].lastReadDMID = atou64(x->second.find("ID")->second.c_str());
			}
			if (!stricmp(x->second.find("Type")->second.c_str(), "tweet_id")) {
				tw_config[i].lastReadTweetID = atou64(x->second.find("ID")->second.c_str());
			}
			/*
			if (!stricmp(x->second.find("Type")->second.c_str(), "dm")) {
				tw_config[i].lastReadDM = atoi64(x->second.find("ID")->second.c_str());
			}
			if (!stricmp(x->second.find("Type")->second.c_str(), "tweets")) {
				tw_config[i].lastReadTweet = atoi64(x->second.find("ID")->second.c_str());
			}
			*/
		}
	}
	return true;
}

void PostTweet(int i, string msg, bool trunc) {
	if (trunc) {
		char buf[144];
		strlcpy(buf, msg.c_str(), sizeof(buf));
		if (strlen(buf) > 140) {
			buf[140] = 0;
			buf[139] = '.';
			buf[138] = '.';
			buf[137] = '.';
			msg = buf;
		}
	}

	Tweet t;
	t.ind = i;
	t.msg = msg;

	LockMutex(TwitterMutex);
	if (i >= 0 && i < MAX_ACCOUNTS && tw_config[i].enabled && tw_config[i].twitter != NULL) {
		tweetQueue.push_back(t);
	}
	RelMutex(TwitterMutex);
}

int Twitter_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
	if (!stricmp(command, "tweet") && (!parms || parms[0] == 0)) {
		ut->std_reply(ut, _("Usage: !tweet num message (if num == -1 tweet will be sent on all accounts)"));
		return 1;
	}

	if (!stricmp(command, "tweet")) {
		ut->std_reply(ut, _("Sending tweet..."));
		LockMutex(TwitterMutex);
		StrTokenizer st((char *)parms, ' ', true);
		int num = atoi(st.stdGetSingleTok(1).c_str());
		string msg = st.stdGetTok(2, st.NumTok());
		if (num == -1) {
			for (int i=0; i < MAX_ACCOUNTS; i++) {
				PostTweet(i, msg);
			}
		} else if (num >= 0 && num < MAX_ACCOUNTS) {
			PostTweet(num, msg);
		}
		RelMutex(TwitterMutex);
		ut->std_reply(ut, _("Tweet sent!"));
		return 1;
	}

	/*
	if (!stricmp(command, "tweet-dm") && (!parms || parms[0] == 0)) {
		ut->std_reply(ut, _("Usage: !tweet num target message (if num == -1 tweet will be sent on all accounts)"));
		return 1;
	}

	if (!stricmp(command, "tweet-dm")) {
		ut->std_reply(ut, _("Sending DM..."));
		LockMutex(TwitterMutex);
		StrTokenizer st((char *)parms, ' ', true);
		int num = atoi(st.stdGetSingleTok(1).c_str());
		string target = st.stdGetSingleTok(2);
		string msg = st.stdGetTok(3, st.NumTok());
		if (num == -1) {
			for (int i=0; i < MAX_ACCOUNTS; i++) {
				PostTweet(i, msg);
			}
		} else if (num >= 0 && num < MAX_ACCOUNTS) {
			PostTweet(num, msg);
		}
		RelMutex(TwitterMutex);
		ut->std_reply(ut, _("DM sent!"));
		return 1;
	}
	*/

	return 0;
}

TT_DEFINE_THREAD(TwitterTimers) {
	TT_THREAD_START
	char buf[1024*2];
	while (!twitter_shutdown_now) {
		for (int i=0; i < MAX_TIMERS && !twitter_shutdown_now; i++) {
			TWITTER_TIMERS *timer = &tw_timers[i];
			if (timer->delaymin > 0 && (timer->action.length() || timer->lines.size() > 0)) {
				timer->nextFire--;
				if (timer->nextFire == 0) {
					timer->nextFire = api->genrand_range(timer->delaymin, timer->delaymax);
					for (int net=0; net < MAX_ACCOUNTS; net++) {
						if ((timer->account == net || timer->account == -1) && tw_config[net].enabled) {
							if (timer->lines.size() > 0) {
								sstrcpy(buf, timer->lines[api->genrand_int32()%timer->lines.size()].c_str());
							} else {
								sstrcpy(buf, timer->action.c_str());
							}
							api->ProcText(buf,sizeof(buf));
							PostTweet(timer->account, buf);
						}
					}
				}
			}
		}
		safe_sleep(1);
	}
	TT_THREAD_END
}

TT_DEFINE_THREAD(TwitterPost) {
	TT_THREAD_START
	//char buf[1024*2];
	while (!twitter_shutdown_now) {
		LockMutex(TwitterMutex);
		tweetQueueType::iterator x = tweetQueue.begin();
		if (x != tweetQueue.end()) {
			Tweet t = *x;
			tweetQueue.erase(x);

			if (t.ind >= 0 && t.ind < MAX_ACCOUNTS && tw_config[t.ind].enabled && tw_config[t.ind].twitter != NULL) {
				api->ib_printf("Twitter -> Attempting to post tweet: %s\n", t.msg.c_str());
				if(tw_config[t.ind].twitter->statusUpdate(t.msg)) {
					tw_config[t.ind].twitter->getLastWebResponse(t.msg);
					if (stristr(t.msg.c_str(), "\"errors\"")) {
						api->ib_printf("Twitter -> statusUpdate web response: %s\n", t.msg.c_str());
					} else {
						api->ib_printf("Twitter -> Posted OK!\n");
					}
				} else {
					tw_config[t.ind].twitter->getLastCurlError(t.msg);
					api->ib_printf("Twitter -> statusUpdate error: %s\n", t.msg.c_str());
				}
			}
			RelMutex(TwitterMutex);
		} else {
			RelMutex(TwitterMutex);
			safe_sleep(1);
		}
	}

	TT_THREAD_END
}

int init(int num, BOTAPI_DEF * BotAPI) {
	pluginnum=num;
	api=BotAPI;

	api->ib_printf(_("Twitter Support -> Loading...\n"));
	if (api->ss == NULL) {
		api->ib_printf2(pluginnum,_("Twitter Support -> This version of RadioBot is not supported!\n"));
		return 0;
	}

	LockMutex(TwitterMutex);
	twitter_shutdown_now = false;
	memset(&tw_config, 0, sizeof(tw_config));

	api->db->Query("CREATE TABLE IF NOT EXISTS TwitterRead (Account INTEGER, Type VARCHAR(16), ID VARCHAR(255))", NULL, NULL, NULL);
	api->db->Query("CREATE UNIQUE INDEX IF NOT EXISTS TwitterReadUnique ON TwitterRead(Account, Type)", NULL, NULL, NULL);

	api->db->Query("CREATE TABLE IF NOT EXISTS TwitterUsers (UserID INTEGER, ScreenName VARCHAR(255))", NULL, NULL, NULL);
	api->db->Query("CREATE UNIQUE INDEX IF NOT EXISTS TwitterUsersUnique ON TwitterUsers(UserID)", NULL, NULL, NULL);

	DS_CONFIG_SECTION *root, *sec;
	bool have_interactive = false;
	int timerind = 0;
	root = api->config->GetConfigSection(NULL, "Twitter");
	if (root) {
		char buf[1024];
		for (int i=0; i < MAX_ACCOUNTS; i++) {
			sprintf(buf, "Account%d", i);
			sec = api->config->GetConfigSection(root, buf);
			if (sec) {
				char * p = api->config->GetConfigSectionValue(sec, "User");
				if (p) {
					sstrcpy(tw_config[i].user, p);
				}
				p = api->config->GetConfigSectionValue(sec, "TokenKey");
				if (p) {
					sstrcpy(tw_config[i].token_key, p);
				}
				p = api->config->GetConfigSectionValue(sec, "TokenSecret");
				if (p) {
					sstrcpy(tw_config[i].token_secret, p);
				}

				tw_config[i].source_only = api->config->GetConfigSectionLong(sec, "SourceOnly") > 0 ? true:false;
				tw_config[i].SongInterval = api->config->GetConfigSectionLong(sec, "SongInterval") > 0 ? api->config->GetConfigSectionLong(sec, "SongInterval"):0;
				tw_config[i].SongIntervalSource = api->config->GetConfigSectionLong(sec, "SongIntervalSource");
				tw_config[i].ReTweet = api->config->GetConfigSectionLong(sec, "ReTweet");
				if (api->config->GetConfigSectionLong(sec, "Interactive") > 0) {
					tw_config[i].enable_interactive = true;
					have_interactive = true;
				}

				CheckConfig(i);
			}
		}

		sec = api->config->GetConfigSection(root, "Timer");
		if (sec) {
			int i=0;
			for (i=0; i < MAX_TWITTER_TIMERS; i++) {
				sprintf(buf,"Timer%d",i);
				DS_CONFIG_SECTION * srv = api->config->GetConfigSection(sec,buf);
				if (srv) {
					buf[0] = 0;
					api->config->GetConfigSectionValueBuf(srv, "Interval", buf, sizeof(buf));
					if (buf[0] == 0) {
						strcpy(buf, "0");
					}
					api->textfunc->get_range(buf, &tw_timers[timerind].delaymin, &tw_timers[timerind].delaymax);
					tw_timers[timerind].nextFire = api->genrand_range(tw_timers[timerind].delaymin, tw_timers[timerind].delaymax);
					tw_timers[timerind].account = api->config->GetConfigSectionLong(srv, "Account");
					char * p = api->config->GetConfigSectionValue(srv, "Action");
					if (p) {
						if (!strnicmp(p, "random:", 7)) {
							FILE * fp = fopen(p+7, "rb");
							if (fp != NULL) {
								while (!feof(fp) && fgets(buf, sizeof(buf), fp)) {
									strtrim(buf);
									if (buf[0] == 0 || buf[0] == '#' || buf[0] == '/') {
										continue;
									}
									tw_timers[timerind].lines.push_back(buf);
								}
								api->ib_printf("Twitter -> Timer%d: Read %u lines from %s.\n", i, tw_timers[timerind].lines.size(), p+7);
								fclose(fp);
							} else {
								api->ib_printf("Twitter -> Timer%d: Error opening %s for reading!\n", i, p+7);
							}
						} else {
							tw_timers[timerind].action = p;
						}
					}
					if (tw_timers[timerind].delaymin > 0 && (tw_timers[timerind].action.length() || tw_timers[timerind].lines.size())) {
						timerind++;
					}
					/*
					for (int j=0; j < 4; j++) {
						char buf2[32],buf3[32];
						sprintf(buf,"Parm%d",j);
						sprintf(buf3,"%%parm%d%%",j);
						val = api->config->GetSectionValue(srv, buf);
						if (val) {
							char * p = NULL;
							if (val->type == DS_TYPE_STRING) {
								p = val->pString;
							} else if (val->type == DS_TYPE_LONG) {
								sprintf(buf2, "%d", val->lLong);
								p = buf2;
							} else if (val->type == DS_TYPE_FLOAT) {
								sprintf(buf2, "%.02f", val->lFloat);
								p = buf2;
							}
							if (tw_timers[timerind].lines.size() > 0) {
								for (int k=0; k < tw_timers[timerind].lines.size(); k++) {
									strlcpy(buf,tw_timers[timerind].lines[k].c_str(),sizeof(buf));
									str_replace(buf, sizeof(buf), buf3, p);
									tw_timers[timerind].lines[k] = buf;
								}
							} else {
								strlcpy(buf,tw_timers[timerind].action.c_str(),sizeof(buf));
								str_replace(buf, sizeof(buf), buf3, p);
								tw_timers[timerind].action = buf;
							}
						}
					}
					*/
				}
			}
		}
	}

	if (!twitter_enabled) {
		api->ib_printf("Twitter -> No accounts have all required config information in ircbot.conf (User, TokenKey, TokenSecret)!\r\n");
	} else {
		COMMAND_ACL acl;
		api->commands->FillACL(&acl, 0, UFLAG_TWEET, 0);
		api->commands->RegisterCommand_Proc(pluginnum, "tweet", &acl, CM_ALLOW_ALL, Twitter_Commands, "This will let you post a tweet from the bot");
		TT_StartThread(TwitterPost, NULL, "Twitter Posting Thread", pluginnum);
		if (have_interactive) {
			TT_StartThread(TwitterCheck, NULL, "Twitter Update Check", pluginnum);
		}
		if (timerind > 0) {
			TT_StartThread(TwitterTimers, NULL, "Twitter Timers", pluginnum);
		}
	}
	RelMutex(TwitterMutex);
	api->ib_printf(_("Twitter Support -> %s!\n"), twitter_enabled?"Loaded succesfully":"Disabled, invalid configuration!");

	return twitter_enabled ? 1:0;
}

void quit() {
	api->ib_printf(_("Twitter Support -> Shutting down...\n"));
	twitter_shutdown_now = true;
	int left=30;
	while (TT_NumThreadsWithID(pluginnum) && left--) {
		api->ib_printf(_("Twitter Support -> Waiting for %d threads to exit... (Will wait %d more seconds...)\n"), TT_NumThreadsWithID(pluginnum), left);
		safe_sleep(1);
	}
	LockMutex(TwitterMutex);
	for (int i=0; i < MAX_ACCOUNTS; i++) {
		tw_config[i].enabled = false;
		if (tw_config[i].twitter) {
			delete tw_config[i].twitter;
			tw_config[i].twitter = NULL;
		}
	}
	tweetQueue.clear();
	RelMutex(TwitterMutex);
	api->ib_printf(_("Twitter Support -> Shut down.\n"));
}

int message(unsigned int msg, char * data, int datalen) {
	switch (msg) {
#if !defined(IRCBOT_LITE)
		case IB_SS_DRAGCOMPLETE:{
				bool * b = (bool *)data;
				if (twitter_enabled) {
					int sp = api->SendMessage(-1, SOURCE_IS_PLAYING, NULL, 0);
					int got_info = 0;
					IBM_SONGINFO song_info;
					memset(&song_info, 0, sizeof(song_info));
					if (sp) {
						got_info = api->SendMessage(-1, SOURCE_GET_SONGINFO, (char *)&song_info, sizeof(song_info));
					}
					for (int i=0; i < MAX_ACCOUNTS; i++) {
						LockMutex(TwitterMutex);
						if ((!tw_config[i].source_only || sp == 1) && tw_config[i].enabled && tw_config[i].twitter != NULL) {
							if (*b == true || (tw_config[i].ReTweet > 0 && (time(NULL)-tw_config[i].lastTweet) >= (tw_config[i].ReTweet*60))) {
								STATS * s = api->ss->GetStreamInfo();
								if (s->online) {
									int si = tw_config[i].SongInterval;
									if (sp && tw_config[i].SongIntervalSource >= 0) {
										si = tw_config[i].SongIntervalSource;
									}
									if (si > 0) {
										int64 diff = time(NULL) - tw_config[i].lastTweet;
										if (diff < (si * 60)) {
											api->ib_printf(_("Twitter Support -> Skipping Tweet on account %d, last tweet was " I64FMT " seconds ago...\n"), i, diff);
											continue;
										}
									}

									char buf[1024], buf2[32], buf3[32], buf4[32], buf_artisttag[1024] = { '#', 0 };
									if (got_info) {
										snprintf(buf2, sizeof(buf2), "%u", song_info.file_id);
										if (song_info.artist[0]) {
											sstrcat(buf_artisttag, song_info.artist);
										}
									} else {
										strcpy(buf2, "0");
									}
									if (buf_artisttag[1] == 0) {
										sstrcat(buf_artisttag, s->cursong);
										char * p = strchr(buf_artisttag, '-');
										if (p != NULL) {
											*p = 0;
										}
									}
									strtrim(buf_artisttag);
									str_replace(buf_artisttag, sizeof(buf_artisttag), " ", "");
									str_replace(buf_artisttag, sizeof(buf_artisttag), "\t", "");
									strlwr(buf_artisttag);

									// check for a new DJ
									if (stricmp(s->curdj, s->lastdj)) {
										buf[0] = 0;
										bool loaded = false;
										if (s->lastdj[0]) {
											snprintf(buf3, sizeof(buf3), "TwitterDJChange_%d", i);
											loaded = api->LoadMessage(buf3, buf, sizeof(buf)) ? true : api->LoadMessage("TwitterDJChange", buf, sizeof(buf));
										} else {
											snprintf(buf3, sizeof(buf3), "TwitterDJNew_%d", i);
											loaded = api->LoadMessage(buf3, buf, sizeof(buf)) ? true : api->LoadMessage("TwitterDJNew", buf, sizeof(buf));
										}
										if (loaded) {
											api->ProcText(buf, sizeof(buf));
											PostTweet(i, buf);
										}
									}

									snprintf(buf3, sizeof(buf3), "TwitterSong_%d", i);
									snprintf(buf4, sizeof(buf4), "TwitterSongSource_%d", i);
									if ((sp == 1 && (api->LoadMessage(buf4, buf, sizeof(buf)) || api->LoadMessage("TwitterSongSource", buf, sizeof(buf)))) || api->LoadMessage(buf3, buf, sizeof(buf)) || api->LoadMessage("TwitterSong", buf, sizeof(buf))) {
										api->ProcText(buf, sizeof(buf));
										str_replace(buf, sizeof(buf), "%artisttag%", buf_artisttag);
										str_replace(buf, sizeof(buf), "%songid%", buf2);
										if (got_info && song_info.is_request) {
											strlcat(buf, " #requested", sizeof(buf));
										}
										PostTweet(i, buf);
										tw_config[i].lastTweet = time(NULL);
									}

									snprintf(buf3, sizeof(buf3), "TwitterRequest_%d", i);
									if (got_info && song_info.is_request && song_info.requested_by[0] && (api->LoadMessage(buf3, buf, sizeof(buf)) || api->LoadMessage("TwitterRequest", buf, sizeof(buf)))) {
										str_replace(buf, sizeof(buf), "%requestedby%", song_info.requested_by);
										api->ProcText(buf, sizeof(buf));
										PostTweet(i, buf);
										tw_config[i].lastTweet = time(NULL);
									}
								}
							}
						}
						RelMutex(TwitterMutex);
					}
				}
			}
			break;
#endif

		case IB_BROADCAST_MSG:{
				if (twitter_enabled && api->irc->GetDoSpam()) {
					IBM_USERTEXT * ibm = (IBM_USERTEXT *)data;
					for (int i=0; i < MAX_ACCOUNTS; i++) {
						PostTweet(i, ibm->text);
					}
				}
			}
			break;
	}

	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{6D7DE1B2-028D-4EFB-946F-203CC861A277}",
	"Twitter Plugin",
	"Twitter Plugin " VERSION "",
	"Drift Solutions",
	init,
	quit,
	message,
	NULL,
	NULL,
	NULL,
	NULL,
};

PLUGIN_EXPORT_FULL



