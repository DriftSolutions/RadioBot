//@AUTOHEADER@BEGIN@
/**********************************************************************\
|                          ShoutIRC RadioBot                           |
|           Copyright 2004-2026 Drift Solutions / Indy Sams            |
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

#include "meshcore_plugin.h"
#include "json.hpp"
using json = nlohmann::json;

BOTAPI_DEF * api = NULL;
int pluginnum = 0;
DSL_Mutex hMutex;
MESHCORE_CONFIG meshcore_config;
static map<int, string> channel_names;

static char meshcore_desc[] = "MeshCore";

// forward declaration — defined after alloc_meshcore_presence
bool send_meshcore_channel(USER_PRESENCE* ut, const char* str);
bool send_meshcore_channel_notice(USER_PRESENCE* ut, const char* str);
bool send_meshcore_privmsg(USER_PRESENCE* ut, const char* str);
bool send_meshcore_notice(USER_PRESENCE* ut, const char* str);
bool send_meshcore_std_reply(USER_PRESENCE* ut, const char* str);
bool send_meshcore_get_contacts();

// ── ref-counting ──────────────────────────────────────────────────────────

#if !defined(DEBUG)
void meshcore_up_ref(USER_PRESENCE * u) {
#else
void meshcore_up_ref(USER_PRESENCE * u, const char * fn, int line) {
	api->ib_printf("meshcore_UserPres_AddRef(): %s : %s:%d\n", u->Nick, fn, line);
#endif
	LockMutex(hMutex);
	if (u->User) { RefUser(u->User); }
	u->ref_cnt++;
	RelMutex(hMutex);
}

#if !defined(DEBUG)
void meshcore_up_unref(USER_PRESENCE * u) {
#else
void meshcore_up_unref(USER_PRESENCE * u, const char * fn, int line) {
	api->ib_printf("meshcore_UserPres_DelRef(): %s : %s:%d\n", u->Nick, fn, line);
#endif
	LockMutex(hMutex);
	if (u->User) { UnrefUser(u->User); }
	u->ref_cnt--;
	if (u->ref_cnt == 0) {
		zfree((void *)u->Nick);
		zfree((void *)u->Hostmask);
		if (u->Channel) { zfree((void *)u->Channel); }
		zfree(u->Ptr1);
		zfree(u);
	}
	RelMutex(hMutex);
}

// ── USER_PRESENCE allocation ──────────────────────────────────────────────

static USER_PRESENCE * alloc_meshcore_presence(USER * user, const char * nick, const char* pubkey, const char * hostmask, bool is_channel, const char * channel_name) {
	USER_PRESENCE * ret = znew(USER_PRESENCE);
	memset(ret, 0, sizeof(USER_PRESENCE));

	MESHCORE_REF_HANDLE * h = znew(MESHCORE_REF_HANDLE);
	memset(h, 0, sizeof(MESHCORE_REF_HANDLE));
	h->is_channel = is_channel;
	sstrcpy(h->channel_name, channel_name ? channel_name : "");
	if (pubkey != NULL) {
		sstrcpy(h->pubkey, pubkey);
	}

	ret->User = user;
	ret->Nick = zstrdup(nick);
	ret->Hostmask = zstrdup(hostmask);
	ret->NetNo = -1;
	ret->Flags = user ? user->Flags : api->user->GetDefaultFlags();
	if (channel_name && *channel_name) {
		ret->Channel = zstrdup(channel_name);
	}
	ret->Ptr1 = h;

	ret->send_chan_notice = send_meshcore_channel_notice;
	ret->send_chan        = send_meshcore_channel;
	ret->send_msg         = send_meshcore_privmsg;
	ret->send_notice      = send_meshcore_notice;
	ret->std_reply        = send_meshcore_std_reply;
	ret->Desc = meshcore_desc;

	ret->ref   = meshcore_up_ref;
	ret->unref = meshcore_up_unref;

	RefUser(ret);
	return ret;
}

// ── outgoing message ──────────────────────────────────────────────────────


void _get_split_into_lines(const char* str, vector<string>& lines, size_t len) {
	vector<char*> words;
	char* tmp = strdup(str);
	strtrim(tmp);
	char* p2 = NULL;
	char* p = strtok_r(tmp, " \t", &p2);
	while (p != NULL) {
		if (p[0]) {
			words.push_back(p);
		}
		p = strtok_r(NULL, " \t", &p2);
	}

	string line;
	for (size_t i = 0; i < words.size(); i++) {
		if (line.empty()) {
			line = words[i];
		} else {
			if (line.length() + 1 + strlen(words[i]) > len) {
				lines.push_back(line);
				line = words[i];
			} else {
				line += " ";
				line += words[i];
			}
		}
	}
	if (!line.empty()) {
		lines.push_back(line);
	}

	free(tmp);
}

/*
void split_msg_for_privmsg(const char* str, vector<string>& lines) {
	_get_split_into_lines(str, lines, 133);
}

void split_msg_for_chan(const char* str, vector<string>& lines) {
	size_t len = 133;
	hMutex.Lock();
	if (meshcore_config.self_node[0] && strlen(meshcore_config.self_node) < 64) {
		len -= (strlen(meshcore_config.self_node) + 2);
	} else if (api->irc) {
		// use the bot's IRC nick as a fallback
		const char* p = api->irc->GetDefaultNick(-1);
		if (p[0] && strlen(p) < 64) {
			len -= (strlen(p) + 2);
		} else {
			// use a default of 16 if we don't know the bot's name
			len -= 16;
		}
	} else {
		// use a default of 16 if we don't know the bot's name
		len -= 16;
	}
	hMutex.Release();
	_get_split_into_lines(str, lines, len);
}
*/

bool send_meshcore_channel_message(int channel_idx, const char* str, int txt_type = 0) {
	AutoMutex(hMutex);
	if (meshcore_config.client == NULL || !meshcore_config.client->connected) {
		return false;
	}

	return meshcore_config.client->SendChannelMsg(channel_idx, str, txt_type);
}

bool send_meshcore_channel_message(const string& channel_name, const char* str, int txt_type = 0) {
	AutoMutex(hMutex);
	if (meshcore_config.client == NULL || !meshcore_config.client->connected) {
		return false;
	}

	int send_idx = 0;
	bool found = false;
	for (const auto& kv : channel_names) {
		if (!stricmp(kv.second.c_str(), channel_name.c_str())) {
			send_idx = kv.first;
			found = true;
			break;
		}
	}
	if (!found) {
		// fallback names are formatted as "#channel-N"
		const char* fallback_prefix = "#channel-";
		size_t plen = strlen(fallback_prefix);
		if (strncmp(channel_name.c_str(), fallback_prefix, plen) == 0) {
			send_idx = atoi(channel_name.c_str() + plen);
		}
	}
	return send_meshcore_channel_message(send_idx, str, txt_type);
}

bool _send_meshcore_msg(USER_PRESENCE * ut, const char * str, bool force_privmsg, int txt_type = 0) {
	AutoMutex(hMutex);
	MESHCORE_REF_HANDLE * h = (MESHCORE_REF_HANDLE *)ut->Ptr1;
	if (meshcore_config.client && meshcore_config.client->connected) {
		if (h->is_channel && !force_privmsg) {
			return send_meshcore_channel_message(h->channel_name, str, txt_type);
		} else {
			string pk = h->pubkey;
			if (pk.empty() && !GetUserPubKey(ut->Nick, pk, NULL)) {
				api->ib_printf(_("We have no pubkey to send a message to %s!\n"), ut->Nick);
				return false;
			}
			return meshcore_config.client->SendDirectMsg(pk, str, txt_type);
		}
	}
	return false;
}

bool send_meshcore_channel(USER_PRESENCE* ut, const char* str) {
	// send to channel if it was said in a channel, otherwise send nothing
	if (ut->Channel != NULL) {
		return _send_meshcore_msg(ut, str, false);
	}
	return false;
}

bool send_meshcore_channel_notice(USER_PRESENCE* ut, const char* str) {
	// send to channel if it was said in a channel, otherwise send nothing
	if (ut->Channel != NULL) {
		return _send_meshcore_msg(ut, str, false, 3);
	}
	return false;
}

bool send_meshcore_privmsg(USER_PRESENCE* ut, const char* str) {
	// send PM to the user, even if they said something in-channel
	return _send_meshcore_msg(ut, str, true);
}

bool send_meshcore_notice(USER_PRESENCE* ut, const char* str) {
	return _send_meshcore_msg(ut, str, true, 3);
}

bool send_meshcore_std_reply(USER_PRESENCE* ut, const char* str) {
	if (ut->Channel) {
		// send NOTICE to the user, falling back to in-channel if they don't have a pubkey set
		if (send_meshcore_notice(ut, str)) {
			return true;
		}
		return send_meshcore_channel_notice(ut, str);
	}

	return _send_meshcore_msg(ut, str, true);
}

bool send_meshcore_get_contacts() {
	AutoMutex(hMutex);
	if (meshcore_config.client && meshcore_config.client->connected) {
		return meshcore_config.client->RequestContacts();
	}
	return false;
}

bool send_meshcore_get_channels() {
	AutoMutex(hMutex);
	if (meshcore_config.client && meshcore_config.client->connected) {
		return meshcore_config.client->RequestChannels();
	}
	return false;
}

// ── incoming message handlers ─────────────────────────────────────────────

static void on_channel_msg(const char* from, const char* text, int channel_idx, int hops) {
	if (!meshcore_config.EnableChannelMessages) { return; }

	if (meshcore_config.self_node[0] && !strcmp(from, meshcore_config.self_node)) {
		return;
	}

	char nick[128];
	sstrcpy(nick, get_sanitized_nick(from).c_str());
	string pubkey;
	if (GetUserPubKey(nick, pubkey, NULL)) {
		pubkey = GetPubKeyPrefix(pubkey);
	} else {
		pubkey = "unknown";
	}

	string hostmask = mprintf("%s!%s@channel.meshcore", nick, pubkey.c_str());
	USER* user = api->user->GetUser(hostmask.c_str());
	// translate channel index to its hashtag name
	string chan_name;
	LockMutex(hMutex);
	auto cn_it = channel_names.find(channel_idx);
	if (cn_it != channel_names.end()) {
		chan_name = cn_it->second;
	} else {
		chan_name = string("#channel-") + std::to_string(channel_idx);
	}
	RelMutex(hMutex);

#ifdef DEBUG
	{
		uint32 flags = user ? user->Flags : api->user->GetDefaultFlags();
		char flagbuf[64];
		api->user->uflag_to_string(flags, flagbuf, sizeof(flagbuf));
		api->ib_printf(_("MeshCore -> Channel[%d/%s] msg from %s[%s, %s]: %s\n"), channel_idx, chan_name.c_str(), nick, hostmask.c_str(), flagbuf, text);
	}
#endif

	USER_PRESENCE* up = alloc_meshcore_presence(user, nick, NULL, hostmask.c_str(), true, chan_name.c_str());

	IBM_USERTEXT ut;
	ut.userpres = up;
	ut.type = IBM_UTT_CHANNEL;
	ut.text = text;
	if (api->SendMessage(-1, IB_ONTEXT, (char*)&ut, sizeof(ut))) {
		UnrefUser(up);
		return;
	}

	if (api->commands->IsCommandPrefix(text[0])) {
		StrTokenizer st((char*)text, ' ');
		char* cmd = st.GetSingleTok(1);
		char* parms = NULL;
		if (st.NumTok() > 1) {
			parms = st.GetTok(2, st.NumTok());
		}

		if (meshcore_config.Location[0] && !stricmp(cmd + 1, "ping")) {
			if (hops == 0xFF) { hops = 0; }
			up->send_chan_notice(up, mprintf("%s: I'm receiving you in %s from %d hops away", from, meshcore_config.Location, hops).c_str());
		} else {
			COMMAND* com = api->commands->FindCommand(cmd + 1);
			if (com && com->proc_type == COMMAND_PROC && (com->mask & CM_ALLOW_CONSOLE_PUBLIC)) {
				api->commands->ExecuteProc(com, parms, up, CM_ALLOW_CONSOLE_PUBLIC | CM_FLAG_SLOW);
			}
		}

		if (parms) { st.FreeString(parms); }
		st.FreeString(cmd);
	}

	UnrefUser(up);
}

static void on_direct_msg(const char * pubkey, const char * text) {
	if (!meshcore_config.EnableDirectMessages) { return; }

	/*
	// self_node is a human name (e.g. "Drift-2"); direct-message `from` is a pubkey_prefix hex string
	if (meshcore_config.self_node[0] && !strcmp(from, meshcore_config.self_node)) {
		return;
	}
	*/
	// self_pubkey is the full public key; pubkey_prefix is a leading substring of it
	if (meshcore_config.self_pubkey[0] && strncmp(pubkey, meshcore_config.self_pubkey, strlen(pubkey)) == 0) {
		return;
	}

	string nick;
	if (!GetNickFromPubKeyPrefix(pubkey, nick)) {
		nick = pubkey;
	}

	string hostmask = mprintf("%s!%s@chat.meshcore", nick.c_str(), pubkey);
	USER * user = api->user->GetUser(hostmask.c_str());

#ifdef DEBUG
	{
		uint32 flags = user ? user->Flags : api->user->GetDefaultFlags();
		char flagbuf[64];
		api->user->uflag_to_string(flags, flagbuf, sizeof(flagbuf));
		api->ib_printf(_("MeshCore -> Direct msg from %s[%s, %s]: %s\n"), nick.c_str(), hostmask.c_str(), flagbuf, text);
	}
#endif

	USER_PRESENCE* up = alloc_meshcore_presence(user, nick.c_str(), pubkey, hostmask.c_str(), false, NULL);
	bool done = false;

	if (api->commands->IsCommandPrefix(text[0]) || !meshcore_config.RequirePrefix) {
		StrTokenizer st((char*)text, ' ');
		char* cmd2 = st.GetSingleTok(1);
		char* cmd = cmd2;
		char* parms = NULL;
		if (st.NumTok() > 1) {
			parms = st.GetTok(2, st.NumTok());
		}

		if (api->commands->IsCommandPrefix(cmd[0])) {
			cmd++;
		}

		COMMAND* com = api->commands->FindCommand(cmd);
		int ret = 0;
		if (com && com->proc_type == COMMAND_PROC && (com->mask & CM_ALLOW_CONSOLE_PRIVATE)) {
			done = (api->commands->ExecuteProc(com, parms, up, CM_ALLOW_CONSOLE_PRIVATE | CM_FLAG_SLOW) > 0);
		}

		if (parms) { st.FreeString(parms); }
		st.FreeString(cmd2);
	}

	if (!done) {
		IBM_USERTEXT ut;
		ut.userpres = up;
		ut.type = IBM_UTT_PRIVMSG;
		ut.text = text;
		api->SendMessage(-1, IB_ONTEXT, (char*)&ut, sizeof(ut));
	}

	UnrefUser(up);
}

void MQTT_IRC_Client::onChanInfo(int channel_idx, const string& channelName, bool is_private, const UniValue& payload) {
	string name = get_sanitized_nick(channelName);
	if (name.empty()) { return; }
	name.insert(name.begin(), '#');

	LockMutex(hMutex);
	channel_names[channel_idx] = name;
	RelMutex(hMutex);
	api->ib_printf(_("MeshCore -> Channel %d name: %s\n"), channel_idx, channelName.c_str());
}

void MQTT_IRC_Client::onContact(const string& nick, const string& pubkey, int type, int hops, const UniValue& payload) {
	if (type == 1) { // only learn Companion nodes
		AddPubKey(nick, pubkey, false);
	}
}

void MQTT_IRC_Client::onContactsComplete(const UniValue& payload) {
	AutoMutex(hMutex);
	if (channel_names.size()) {
		meshcore_config.lastReceivedContacts = time(NULL);
	}
}

void MQTT_IRC_Client::onSelfInfo(const string& nick, const string& pubkey, const UniValue& payload) {
	LockMutex(hMutex);
	if (strcmp(nick.c_str(), meshcore_config.self_node)) {
		sstrcpy(meshcore_config.self_node, nick.c_str());
		api->ib_printf(_("MeshCore -> Self node name: %s\n"), meshcore_config.self_node);
	}
	if (strcmp(pubkey.c_str(), meshcore_config.self_pubkey)) {
		sstrcpy(meshcore_config.self_pubkey, pubkey.c_str());
		api->ib_printf(_("MeshCore -> Self public key: %s\n"), meshcore_config.self_pubkey);
	}
	RelMutex(hMutex);
	return;
}

void MQTT_IRC_Client::onChannelMessage(int channel_idx, const string& from, const string& text, int txt_type, int hops, const UniValue& payload) {
	if (txt_type == 3) {
		return;
	}

	on_channel_msg(from.c_str(), text.c_str(), channel_idx, hops);
}

void MQTT_IRC_Client::onDirectMessage(const string& pubkey_prefix, const string& text, int txt_type, int hops, const UniValue& payload) {
	on_direct_msg(pubkey_prefix.c_str(), text.c_str());
}

// ── worker thread ─────────────────────────────────────────────────────────

THREADTYPE MeshCoreThread(void * lpData) {
	TT_THREAD_START

	time_t nextTry = time(NULL);
	int64 lastReq = 0;
	int64 lastChannelReq = 0;	

	while (!meshcore_config.shutdown_now) {
		meshcore_config.client->Work();

		if (meshcore_config.client->connected) {
			if (time(NULL) - meshcore_config.lastReceivedContacts >= 3600 && time(NULL) - lastReq >= 60) {
				send_meshcore_get_contacts();
				lastReq = time(NULL);
			}
			if (time(NULL) - meshcore_config.lastReceivedChannels >= 3600 && time(NULL) - lastChannelReq >= 60) {
				send_meshcore_get_channels();
				lastChannelReq = time(NULL);
			}				
		} else {
			safe_sleep(1);
		}
	}

	TT_THREAD_END
}

// ── plugin lifecycle ──────────────────────────────────────────────────────

int init(int num, BOTAPI_DEF * BotAPI) {
	api = BotAPI;
	pluginnum = num;

	memset(&meshcore_config, 0, sizeof(meshcore_config));

	// defaults
	sstrcpy(meshcore_config.host, "127.0.0.1");
	meshcore_config.port = 1883;
	sstrcpy(meshcore_config.topic_prefix, "meshcore");
	meshcore_config.RequirePrefix = false;
	meshcore_config.EnableChannelMessages = true;
	meshcore_config.EnableDirectMessages = true;

	DS_CONFIG_SECTION * root = api->config->GetConfigSection(NULL, "MeshCore");
	if (root) {
		char * p;

		p = api->config->GetConfigSectionValue(root, "Host");
		if (p) { sstrcpy(meshcore_config.host, p); }

		int n = api->config->GetConfigSectionLong(root, "Port");
		if (n > 0 && n <= 65535) { meshcore_config.port = n; }

		p = api->config->GetConfigSectionValue(root, "User");
		if (p) { sstrcpy(meshcore_config.username, p); }

		p = api->config->GetConfigSectionValue(root, "Pass");
		if (p) { sstrcpy(meshcore_config.password, p); }

		p = api->config->GetConfigSectionValue(root, "TopicPrefix");
		if (p && *p) { sstrcpy(meshcore_config.topic_prefix, p); }

		p = api->config->GetConfigSectionValue(root, "Location");
		if (p && *p) { sstrcpy(meshcore_config.Location, p); }

		meshcore_config.RequirePrefix = (api->config->GetConfigSectionLong(root, "RequirePrefix") > 0);
		meshcore_config.EnableChannelMessages = api->config->GetConfigSectionLong(root, "EnableChannelMessages") != 0;
		meshcore_config.EnableDirectMessages = api->config->GetConfigSectionLong(root, "EnableDirectMessages") != 0;
		meshcore_config.SourceOnly = api->config->GetConfigSectionLong(root, "SourceOnly") > 0 ? true : false;
		meshcore_config.SongInterval = api->config->GetConfigSectionLong(root, "SongInterval") > 0 ? api->config->GetConfigSectionLong(root, "SongInterval") : 0;
		meshcore_config.SongIntervalSource = api->config->GetConfigSectionLong(root, "SongIntervalSource");
	} else {
		api->ib_printf(_("MeshCore -> No 'MeshCore' section in ircbot.conf, using defaults...\n"));
	}

	api->db->Query("CREATE TABLE IF NOT EXISTS meshcore_users(ID INTEGER PRIMARY KEY AUTOINCREMENT, `Nick` TEXT COLLATE NOCASE UNIQUE, `PubKey` TEXT);", NULL, NULL, NULL);
	// Second, check if a pubkey has been saved in the DB
	sql_rows res = GetTable("SELECT * FROM meshcore_users");
	for (auto& x : res) {
		string nick = res.begin()->second["Nick"];
		string pubkey = res.begin()->second["PubKey"];
		AddPubKey(nick, pubkey, true);
	}

	COMMAND_ACL acl;
	api->commands->FillACL(&acl, 0, UFLAG_DJ | UFLAG_HOP | UFLAG_OP | UFLAG_MASTER, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "meshcore-viewuserpubkey", &acl, CM_ALLOW_ALL_PRIVATE, MeshCore_UserDB_Commands, _("View nickname's pubkey"));

	api->commands->FillACL(&acl, 0, UFLAG_OP | UFLAG_MASTER, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "meshcore-saveuserpubkey", &acl, CM_ALLOW_ALL_PRIVATE, MeshCore_UserDB_Commands, _("Save a pubkey for a nickname, so I can reply users who say things in-channel via DM"));
	api->commands->RegisterCommand_Proc(pluginnum, "meshcore-deluserpubkey", &acl, CM_ALLOW_ALL_PRIVATE, MeshCore_UserDB_Commands, _("Delete the pubkey for a nickname"));

	meshcore_config.client = new MQTT_IRC_Client(meshcore_config.host, meshcore_config.port, meshcore_config.username, meshcore_config.password, meshcore_config.topic_prefix);

	TT_StartThread(MeshCoreThread, NULL, _("MeshCore Thread"), pluginnum);

	return 1;
}

void quit() {
	meshcore_config.shutdown_now = true;
	api->ib_printf(_("MeshCore -> Shutting down...\n"));
	while (TT_NumThreadsWithID(pluginnum)) {
		safe_sleep(100, true);
	}

	if (meshcore_config.client != NULL) {
		delete meshcore_config.client;
		meshcore_config.client = NULL;
	}

	api->ib_printf(_("MeshCore -> Shut down.\n"));
}

int message(unsigned int msg, char * data, int datalen) {
	switch (msg) {
#if !defined(IRCBOT_LITE)
		case IB_SS_DRAGCOMPLETE:
			{
				bool* b = (bool*)data;
				int sp = api->SendMessage(-1, SOURCE_IS_PLAYING, NULL, 0);
				int got_info = 0;
				IBM_SONGINFO song_info;
				memset(&song_info, 0, sizeof(song_info));
				if (sp) {
					got_info = api->SendMessage(-1, SOURCE_GET_SONGINFO, (char*)&song_info, sizeof(song_info));
				}
				AutoMutex(hMutex);
				if (meshcore_config.client != NULL && meshcore_config.client->connected) {
					for (auto& c : channel_names) {						 
						if (*b == true && (!meshcore_config.SourceOnly || sp == 1)) {
							STATS* s = api->ss->GetStreamInfo();
							if (s->online) {
								int si = meshcore_config.SongInterval;
								if (sp && meshcore_config.SongIntervalSource >= 0) {
									si = meshcore_config.SongIntervalSource;
								}
								if (si > 0) {
									int64 diff = time(NULL) - meshcore_config.lastPost;
									if (diff < (si * 60)) {
										api->ib_printf(_("MeshCore -> Skipping post on channel %s, last tweet was " I64FMT " seconds ago...\n"), c.second.c_str(), diff);
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
									char* p = strchr(buf_artisttag, '-');
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
										snprintf(buf3, sizeof(buf3), "MeshCoreDJChange_%s", c.second.c_str());
										loaded = api->LoadMessage(buf3, buf, sizeof(buf)) ? true : api->LoadMessage("MeshCoreDJChange", buf, sizeof(buf));
									} else {
										snprintf(buf3, sizeof(buf3), "MeshCoreDJNew_%s", c.second.c_str());
										loaded = api->LoadMessage(buf3, buf, sizeof(buf)) ? true : api->LoadMessage("MeshCoreDJNew", buf, sizeof(buf));
									}
									if (loaded) {
										api->ProcText(buf, sizeof(buf));
										send_meshcore_channel_message(c.first, buf);
									}
								}

								snprintf(buf3, sizeof(buf3), "MeshCoreSong_%s", c.second.c_str());
								snprintf(buf4, sizeof(buf4), "MeshCoreSongSource_%s", c.second.c_str());
								if ((sp == 1 && (api->LoadMessage(buf4, buf, sizeof(buf)) || api->LoadMessage("MeshCoreSongSource", buf, sizeof(buf)))) || api->LoadMessage(buf3, buf, sizeof(buf)) || api->LoadMessage("MeshCoreSong", buf, sizeof(buf))) {
									api->ProcText(buf, sizeof(buf));
									str_replace(buf, sizeof(buf), "%artisttag%", buf_artisttag);
									str_replace(buf, sizeof(buf), "%songid%", buf2);
									if (got_info && song_info.is_request) {
										strlcat(buf, " #requested", sizeof(buf));
									}
									send_meshcore_channel_message(c.first, buf);
									meshcore_config.lastPost = time(NULL);
								}

								snprintf(buf3, sizeof(buf3), "MeshCoreRequest_%s", c.second.c_str());
								if (got_info && song_info.is_request && song_info.requested_by[0] && (api->LoadMessage(buf3, buf, sizeof(buf)) || api->LoadMessage("MeshCoreRequest", buf, sizeof(buf)))) {
									str_replace(buf, sizeof(buf), "%requestedby%", song_info.requested_by);
									api->ProcText(buf, sizeof(buf));
									send_meshcore_channel_message(c.first, buf);
									meshcore_config.lastPost = time(NULL);
								}
							}
						}
					}
				}				
			}
			break;
#endif

		case IB_BROADCAST_MSG:
			{
				if (api->irc == NULL || api->irc->GetDoSpam()) {
					IBM_USERTEXT* ibm = (IBM_USERTEXT*)data;
					AutoMutex(hMutex);
					for (auto& c : channel_names) {
						send_meshcore_channel_message(c.first, ibm->text);
						//PostTweet(i, ibm->text);
					}
				}
			}
			break;
	}
	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{A3F72B14-9D4C-4E8A-B3C1-5F2E6D7A8B9C}",
	"MeshCore",
	"MeshCore MQTT Plugin 1.0",
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
