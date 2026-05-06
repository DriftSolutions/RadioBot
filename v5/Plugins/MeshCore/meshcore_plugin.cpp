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
bool send_meshcore_privmsg(USER_PRESENCE* ut, const char* str);
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

	ret->send_chan_notice = send_meshcore_channel;
	ret->send_chan        = send_meshcore_channel;
	ret->send_msg         = send_meshcore_privmsg;
	ret->send_notice      = send_meshcore_privmsg;
	ret->std_reply        = send_meshcore_std_reply;
	ret->Desc = meshcore_desc;

	ret->ref   = meshcore_up_ref;
	ret->unref = meshcore_up_unref;

	RefUser(ret);
	return ret;
}

// ── outgoing message ──────────────────────────────────────────────────────

void _send_meshcore_msg_chan(MESHCORE_REF_HANDLE* h, string& topic, json& payload, const char* str) {
	// reverse-lookup the channel index from the stored name
	int send_idx = 0;
	bool found = false;
	for (const auto& kv : channel_names) {
		if (kv.second == h->channel_name) {
			send_idx = kv.first;
			found = true;
			break;
		}
	}
	if (!found) {
		// fallback names are formatted as "#channel-N"
		const char* fallback_prefix = "#channel-";
		size_t plen = strlen(fallback_prefix);
		if (strncmp(h->channel_name, fallback_prefix, plen) == 0) {
			send_idx = atoi(h->channel_name + plen);
		}
	}
	payload["channel"] = send_idx;
	payload["message"] = str;
	topic = string(meshcore_config.topic_prefix) + "/command/send_chan_msg";
}

void _send_meshcore_msg_priv(MESHCORE_REF_HANDLE* h, const string& destination, string& topic, json& payload, const char* str) {
	payload["destination"] = destination;
	payload["message"] = str;
	topic = string(meshcore_config.topic_prefix) + "/command/send_msg";
}

bool _send_meshcore_msg(USER_PRESENCE * ut, const char * str, bool force_privmsg) {
	AutoMutex(hMutex);
	bool ret = false;
	MESHCORE_REF_HANDLE * h = (MESHCORE_REF_HANDLE *)ut->Ptr1;
	if (meshcore_config.connected && meshcore_config.mosq) {
		json payload;
		string topic;

		if (h->is_channel && !force_privmsg) {
			_send_meshcore_msg_chan(h, topic, payload, str);
		} else {
			string pk = h->pubkey;
			if (pk.empty() && !GetUserPubKey(ut->Nick, pk, NULL)) {
				api->ib_printf(_("We have no pubkey to send a message to %s!\n"), ut->Nick);
				return false;
			}
			_send_meshcore_msg_priv(h, pk, topic, payload, str);
		}

		string s = payload.dump();
#ifdef DEBUG
		api->ib_printf("Sending payload to %s: %s\n", topic.c_str(), s.c_str());
#endif
		int rc;
		if ((rc = mosquitto_publish(meshcore_config.mosq, NULL, topic.c_str(), (int)s.size(), s.c_str(), 0, false)) == MOSQ_ERR_SUCCESS) {
			ret = true;
		} else {
#ifndef WIN32
			api->ib_printf(_("MeshCore -> Error sending message to broker %s:%d: %s\n"), meshcore_config.host, meshcore_config.port, mosquitto_strerror(rc));
#else
			api->ib_printf(_("MeshCore -> Error sending message to broker %s:%d!\n"), meshcore_config.host, meshcore_config.port);
#endif
			api->ib_printf("Payload was to %s: %s\n", topic.c_str(), s.c_str());
		}
	}
	return ret;
}

bool send_meshcore_channel(USER_PRESENCE* ut, const char* str) {
	// send to channel if it was said in a channel, otherwise send nothing
	if (ut->Channel != NULL) {
		return _send_meshcore_msg(ut, str, false);
	}
	return false;
}

bool send_meshcore_privmsg(USER_PRESENCE* ut, const char* str) {
	// send PM to the user, even if they said something in-channel
	return _send_meshcore_msg(ut, str, true);
}

bool send_meshcore_std_reply(USER_PRESENCE* ut, const char* str) {
	// send PM to the user, falling back to in-channel if they don't have a pubkey set
	if (_send_meshcore_msg(ut, str, true)) {
		return true;
	}
	return _send_meshcore_msg(ut, str, false);
}

bool send_meshcore_get_contacts() {
	bool ret = false;
	json payload = json::object();
	string topic = string(meshcore_config.topic_prefix) + "/command/get_contacts";
	string s = payload.dump();
#ifdef DEBUG
	api->ib_printf("Sending payload to %s: %s\n", topic.c_str(), s.c_str());
#endif
	int rc;
	if ((rc = mosquitto_publish(meshcore_config.mosq, NULL, topic.c_str(), (int)s.size(), s.c_str(), 0, false)) == MOSQ_ERR_SUCCESS) {
		ret = true;
	} else {
#ifndef WIN32
		api->ib_printf(_("MeshCore -> Error sending message to broker %s:%d: %s\n"), meshcore_config.host, meshcore_config.port, mosquitto_strerror(rc));
#else
		api->ib_printf(_("MeshCore -> Error sending message to broker %s:%d!\n"), meshcore_config.host, meshcore_config.port);
#endif
		api->ib_printf("Payload was to %s: %s\n", topic.c_str(), s.c_str());
	}
	return ret;
}

// ── incoming message handlers ─────────────────────────────────────────────

static void on_channel_msg(const char* from, const char* text, int channel_idx) {
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

		COMMAND* com = api->commands->FindCommand(cmd + 1);
		if (com && com->proc_type == COMMAND_PROC && (com->mask & CM_ALLOW_CONSOLE_PUBLIC)) {
			api->commands->ExecuteProc(com, parms, up, CM_ALLOW_CONSOLE_PUBLIC);
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
		api->ib_printf(_("MeshCore -> Direct msg from %s[%s, %s]: %s\n"), nick, hostmask.c_str(), flagbuf, text);
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
			done = (api->commands->ExecuteProc(com, parms, up, CM_ALLOW_CONSOLE_PRIVATE) > 0);
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

// ── MQTT callbacks ────────────────────────────────────────────────────────

static void on_mqtt_connect(struct mosquitto * mosq, void * userdata, int rc) {
	if (rc != 0) {
		api->ib_printf(_("MeshCore -> MQTT connection refused (rc=%d)\n"), rc);
		return;
	}

	api->ib_printf(_("MeshCore -> Connected to MQTT broker at %s:%d\n"), meshcore_config.host, meshcore_config.port);

	LockMutex(hMutex);
	meshcore_config.connected = true;
	RelMutex(hMutex);

	char topic[256];
	snprintf(topic, sizeof(topic), "%s/message/channel/#", meshcore_config.topic_prefix);
	mosquitto_subscribe(mosq, NULL, topic, 0);

	snprintf(topic, sizeof(topic), "%s/message/direct/#", meshcore_config.topic_prefix);
	mosquitto_subscribe(mosq, NULL, topic, 0);

	snprintf(topic, sizeof(topic), "%s/self_info", meshcore_config.topic_prefix);
	mosquitto_subscribe(mosq, NULL, topic, 0);

	snprintf(topic, sizeof(topic), "%s/channel_info", meshcore_config.topic_prefix);
	mosquitto_subscribe(mosq, NULL, topic, 0);

	snprintf(topic, sizeof(topic), "%s/contacts", meshcore_config.topic_prefix);
	mosquitto_subscribe(mosq, NULL, topic, 0);

	snprintf(topic, sizeof(topic), "%s/new_contact", meshcore_config.topic_prefix);
	mosquitto_subscribe(mosq, NULL, topic, 0);

	/*
#ifdef DEBUG
	snprintf(topic, sizeof(topic), "%s/#", meshcore_config.topic_prefix);
	mosquitto_subscribe(mosq, NULL, topic, 0);
#endif
*/

	send_meshcore_get_contacts();
}

static void on_mqtt_disconnect(struct mosquitto * mosq, void * userdata, int rc) {
	api->ib_printf(_("MeshCore -> Disconnected from MQTT broker (rc=%d)\n"), rc);
	LockMutex(hMutex);
	meshcore_config.connected = false;
	channel_names.clear();
	RelMutex(hMutex);
}

void _handle_contact(const json& e) {
	if (e.contains("type") && e["type"].is_number_integer() && e.contains("public_key") && e["public_key"].is_string() && e.contains("adv_name") && e["adv_name"].is_string()) {
		if (e["type"].get<int>() == 1) { // only learn Companion nodes
			string pubkey = e["public_key"].get<string>();
			string nick = e["adv_name"].get<string>();
			AddPubKey(nick, pubkey, false);
		}
	}
}

static void on_mqtt_message(struct mosquitto * mosq, void * userdata, const struct mosquitto_message * msg) {
	if (!msg->payload || msg->payloadlen <= 0) { return; }

	string raw((const char *)msg->payload, msg->payloadlen);
	string topic(msg->topic);

#ifdef DEBUG
	api->ib_printf("Received message on %s: %s\n", topic.c_str(), raw.c_str());
#endif

	auto j = json::parse(raw, nullptr, false);
	if (j.is_discarded() || !j.contains("payload") || !j["payload"].is_object()) {
		api->ib_printf(_("MeshCore -> Failed to parse JSON on topic %s\n"), msg->topic);
		return;
	}
	auto& payload = j["payload"];

#ifdef DEBUG
	api->ib_printf("Payload only: %s\n", payload.dump().c_str());
#endif

	string prefix_self      = string(meshcore_config.topic_prefix) + "/self_info";
	string prefix_chan_info = string(meshcore_config.topic_prefix) + "/channel_info";
	string prefix_chan      = string(meshcore_config.topic_prefix) + "/message/channel/";
	string prefix_dir       = string(meshcore_config.topic_prefix) + "/message/direct/";
	string prefix_contacts	= string(meshcore_config.topic_prefix) + "/contacts";
	string prefix_new_contact = string(meshcore_config.topic_prefix) + "/new_contact";

	if (topic == prefix_chan_info) {
		if (payload.contains("channel_idx") && payload.contains("name") && payload["name"].is_string()) {
			int idx = payload["channel_idx"].get<int>();
			string name = "#" + payload["name"].get<string>();
			for (char& c : name) {
				if (c == ' ' || c == '!' || c == '@') { c = '-'; }
			}
			LockMutex(hMutex);
			channel_names[idx] = name;
			RelMutex(hMutex);
			api->ib_printf(_("MeshCore -> Channel %d name: %s\n"), idx, name.c_str());
		}
		return;
	}

	if (topic == prefix_new_contact) {
		_handle_contact(payload);
		return;
	}

	if (topic == prefix_contacts) {
		for (auto& e : payload) {
			_handle_contact(e);
		}
		meshcore_config.lastReceivedContacts = time(NULL);
		return;
	}

	if (topic == prefix_self) {
		LockMutex(hMutex);
		if (payload.contains("name") && payload["name"].is_string()) {
			sstrcpy(meshcore_config.self_node, payload["name"].get<string>().c_str());
			api->ib_printf(_("MeshCore -> Self node name: %s\n"), meshcore_config.self_node);
		}
		if (payload.contains("public_key") && payload["public_key"].is_string()) {
			sstrcpy(meshcore_config.self_pubkey, payload["public_key"].get<string>().c_str());
			api->ib_printf(_("MeshCore -> Self public key: %s\n"), meshcore_config.self_pubkey);
		}
		RelMutex(hMutex);
		return;
	}

	if (topic.rfind(prefix_chan, 0) == 0) {
		// text format: "SenderName: actual message"
		string text = payload.value("text", "");
		size_t sep = text.find(": ");
		if (sep == string::npos || sep + 2 >= text.size()) { return; }
		string from     = text.substr(0, sep);
		string msg_text = text.substr(sep + 2);
		int channel_idx = payload.value("channel_idx", 0);
		on_channel_msg(from.c_str(), msg_text.c_str(), channel_idx);
	} else if (topic.rfind(prefix_dir, 0) == 0) {
		string from = payload.value("pubkey_prefix", "");
		string text = payload.value("text", "");
		if (from.empty() || text.empty()) { return; }
		on_direct_msg(from.c_str(), text.c_str());
	}
}

// ── worker thread ─────────────────────────────────────────────────────────

THREADTYPE MeshCoreThread(void * lpData) {
	TT_THREAD_START

	LockMutex(hMutex);
	meshcore_config.connected = false;
	meshcore_config.mosq = NULL;
	RelMutex(hMutex);

	time_t nextTry = time(NULL);
	int64 lastReq = 0;

	while (!meshcore_config.shutdown_now) {
		if (meshcore_config.mosq == NULL && time(NULL) >= nextTry) {
			char client_id[64];
			snprintf(client_id, sizeof(client_id), "radiobot-%d", (int)time(NULL));

			struct mosquitto * mosq = mosquitto_new(client_id, true, NULL);
			if (!mosq) {
				api->ib_printf(_("MeshCore -> Failed to create mosquitto client\n"));
				nextTry = time(NULL) + 30;
			} else {
				mosquitto_connect_callback_set(mosq, on_mqtt_connect);
				mosquitto_disconnect_callback_set(mosq, on_mqtt_disconnect);
				mosquitto_message_callback_set(mosq, on_mqtt_message);

				if (meshcore_config.username[0]) {
					mosquitto_username_pw_set(mosq, meshcore_config.username, meshcore_config.password[0] ? meshcore_config.password : NULL);
				}

				int rc = mosquitto_connect(mosq, meshcore_config.host, meshcore_config.port, 60);
				if (rc != MOSQ_ERR_SUCCESS) {
#ifndef WIN32
					api->ib_printf(_("MeshCore -> Failed to connect to broker %s:%d: %s\n"), meshcore_config.host, meshcore_config.port, mosquitto_strerror(rc));
#else
					api->ib_printf(_("MeshCore -> Failed to connect to broker %s:%d!\n"), meshcore_config.host, meshcore_config.port);
#endif
					mosquitto_destroy(mosq);
					nextTry = time(NULL) + 30;
				} else {
					LockMutex(hMutex);
					meshcore_config.mosq = mosq;
					RelMutex(hMutex);
				}
			}
		}

		if (meshcore_config.mosq) {
			int rc = mosquitto_loop(meshcore_config.mosq, 100, 1);
			if (rc != MOSQ_ERR_SUCCESS && rc != MOSQ_ERR_NO_CONN) {
#ifndef WIN32
				api->ib_printf(_("MeshCore -> MQTT loop error: %s — reconnecting in 30s\n"), mosquitto_strerror(rc));
#else
				api->ib_printf(_("MeshCore -> MQTT loop error: reconnecting in 30s\n"));
#endif
				LockMutex(hMutex);
				mosquitto_destroy(meshcore_config.mosq);
				meshcore_config.mosq = NULL;
				meshcore_config.connected = false;
				meshcore_config.lastReceivedContacts = 0;
				RelMutex(hMutex);
				nextTry = time(NULL) + 30;
			} else if (time(NULL) - meshcore_config.lastReceivedContacts >= 3600 && time(NULL) - lastReq >= 60) {
				send_meshcore_get_contacts();
				lastReq = time(NULL);
			}
		} else {
			safe_sleep(1);
		}
	}

	LockMutex(hMutex);
	if (meshcore_config.mosq) {
		if (meshcore_config.connected) {
			mosquitto_disconnect(meshcore_config.mosq);
		}
		mosquitto_destroy(meshcore_config.mosq);
		meshcore_config.mosq = NULL;
		meshcore_config.connected = false;
	}
	RelMutex(hMutex);

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

		meshcore_config.RequirePrefix = (api->config->GetConfigSectionLong(root, "RequirePrefix") > 0);
		meshcore_config.EnableChannelMessages = api->config->GetConfigSectionLong(root, "EnableChannelMessages") != 0;
		meshcore_config.EnableDirectMessages = api->config->GetConfigSectionLong(root, "EnableDirectMessages") != 0;

		auto chans = api->config->GetConfigSection(root, "Channels");
		if (chans) {
			char buf[32];
			for (int i = 0; i < 32; i++) {
				snprintf(buf, sizeof(buf), "%d", i);
				p = api->config->GetConfigSectionValue(chans, buf);
				if (p && strlen(p) > 1 && p[0] == '#') {
					channel_names[i] = p;
				}
			}
		}
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

	mosquitto_lib_init();
	TT_StartThread(MeshCoreThread, NULL, _("MeshCore Thread"), pluginnum);

	return 1;
}

void quit() {
	meshcore_config.shutdown_now = true;
	api->ib_printf(_("MeshCore -> Shutting down...\n"));
	while (TT_NumThreadsWithID(pluginnum)) {
		safe_sleep(100, true);
	}
	mosquitto_lib_cleanup();
	api->ib_printf(_("MeshCore -> Shut down.\n"));
}

int message(unsigned int msg, char * data, int datalen) {
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
