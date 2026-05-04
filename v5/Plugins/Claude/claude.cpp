//@AUTOHEADER@BEGIN@
/**********************************************************************\
|                          ShoutIRC RadioBot                           |
|           Copyright 2004-2023 Drift Solutions / Indy Sams            |
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

#include "claude.h"
#include "json.hpp"
using json = nlohmann::json;

BOTAPI_DEF *api = NULL;
int pluginnum; // the number we were assigned

CLAUDE_CONFIG claude_config;
Titus_Mutex chatMutex;
chatListType sessions;
list<string> new_messages;

bool LoadConfig() {
	memset(&claude_config,0,sizeof(claude_config));
	sstrcpy(claude_config.model, "claude-opus-4-7");
	claude_config.max_tokens = 1024;
	claude_config.songMin = 4;
	claude_config.songMax = 6;

	DS_CONFIG_SECTION * sec = api->config->GetConfigSection(NULL, "Claude");
	if (sec == NULL) {
		api->ib_printf(_("Claude -> No 'Claude' section found in config, disabling...\n"));
		return false;
	}

	if (!api->config->GetConfigSectionValueBuf(sec, "API_Key", claude_config.api_key, sizeof(claude_config.api_key)) || claude_config.api_key[0] == 0) {
		api->ib_printf(_("Claude -> No 'API_Key' set in your 'Claude' section, disabling...\n"));
		return false;
	}

	const char* p = api->config->GetConfigSectionValue(sec, "Model");
	if (p && p[0] != 0) {
		sstrcpy(claude_config.model, p);
	}

	int32 x = api->config->GetConfigSectionLong(sec, "MaxTokens");
	if (x > 0) {
		claude_config.max_tokens = x;
	}

	char buf[64];
	if (api->config->GetConfigSectionValueBuf(sec, "EnableSongAnnounce", buf, sizeof(buf))) {
		api->textfunc->get_range(buf, &claude_config.songMin, &claude_config.songMax);
	}
	return true;
}

int init(int num, BOTAPI_DEF * BotAPI) {
	pluginnum=num;
	api=BotAPI;
	if (!LoadConfig()) {
		return 0;
	}

/*
	COMMAND_ACL acl;
	api->commands->FillACL(&acl, 0, UFLAG_MASTER|UFLAG_OP, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "telnet-status", &acl, CM_ALLOW_ALL_PRIVATE, Telnet_Commands, "Shows you any active Telnet sessions");
	*/

	TT_StartThread(ChatThread, NULL, _("Claude Chat Thread"), pluginnum);
	api->ib_printf(_("Claude -> Ready...\n"));
	return 1;
}

void quit() {
	api->ib_printf(_("Claude -> Shutting down...\n"));
	claude_config.shutdown_now=true;
	//main listening socket will be closed by ChatThread when it exits
	time_t timeo = time(NULL) + 30;
	while (TT_NumThreadsWithID(pluginnum) && time(NULL) < timeo) {
		api->ib_printf(_("Claude -> Waiting on (%d) threads to die...\n"), TT_NumThreadsWithID(pluginnum));
		api->safe_sleep_seconds(1);
	}
	chatMutex.Lock();
	sessions.clear();
	new_messages.clear();
	chatMutex.Release();
	api->ib_printf(_("Claude -> Shut down.\n"));
}

static size_t curl_write_string(void *data, size_t size, size_t nmemb, void *clientp) {
	size_t realsize = size * nmemb;
	string * str = (string *)clientp;
	str->append((const char *)data, realsize);
	return realsize;
}

bool claude_query(const list<ClaudeMessage>& context, list<ClaudeMessage>& reply) {
	CURL * ch = api->curl->easy_init();
	if (ch == NULL) {
		api->ib_printf(_("Claude -> Error creating cURL handle!\n"));
		return false;
	}

	curl_slist * headers = api->curl->slist_append(NULL, "Content-Type: application/json");
	headers = api->curl->slist_append(NULL, "anthropic-version: 2023-06-01");
	stringstream key;
	char * tmp = api->curl->escape(claude_config.api_key, 0);
	key << "x-api-key: " << tmp;
	api->curl->free(tmp);
	headers = api->curl->slist_append(headers, key.str().c_str());

	json messages = json::array();
	for (auto& x : context) {
		json entry = {
			{ "role", x.role },
			{ "content", x.content }
		};
		messages.push_back(entry);
	}

	json post_data = {
		{"model", claude_config.model},
		{"system", "You are a general purpose chat bot speaking privately to a user. Reply using plain text only. Do not use markdown, emojis, bullet points, bold, italics, headers, or any special formatting characters. Use only standard ASCII letters, numbers, punctuation, and newlines."},
		{"max_tokens", claude_config.max_tokens },
		{"messages", messages},
	};

	bool ret = false;
	string data;
	api->curl->easy_setopt(ch, CURLOPT_URL, "https://api.anthropic.com/v1/messages");
	api->curl->easy_setopt(ch, CURLOPT_WRITEFUNCTION, curl_write_string);
	api->curl->easy_setopt(ch, CURLOPT_WRITEDATA, &data);
	api->curl->easy_setopt(ch, CURLOPT_NOSIGNAL, 1);
#ifdef DEBUG
	api->curl->easy_setopt(ch, CURLOPT_FAILONERROR, 0);
#else
	api->curl->easy_setopt(ch, CURLOPT_FAILONERROR, 1);
#endif
	api->curl->easy_setopt(ch, CURLOPT_TIMEOUT, 60);
	api->curl->easy_setopt(ch, CURLOPT_CONNECTTIMEOUT, 10);
	api->curl->easy_setopt(ch, CURLOPT_HTTPHEADER, headers);
	api->curl->easy_setopt(ch, CURLOPT_COPYPOSTFIELDS, post_data.dump().c_str());
#ifdef DEBUG_CLAUDE
	printf("request: %s\n", post_data.dump().c_str());
#endif
	CURLcode res = api->curl->easy_perform(ch);
	if (res == CURLE_OK) {
#ifdef DEBUG_CLAUDE
		printf("reply: %s\n", data.c_str());
#endif
		try {
			json dec = json::parse(data);
			if (dec.contains("type") && dec.contains("role") && dec.contains("content") && dec.contains("stop_reason") && dec["type"].is_string() && dec["role"].is_string() && dec["content"].is_array() && dec["content"].size() && dec["stop_reason"].is_string()) {
				if (!stricmp(dec["stop_reason"].get<string>().c_str(), "end_turn") && !stricmp(dec["type"].get<string>().c_str(), "message") && !stricmp(dec["role"].get<string>().c_str(), "assistant")) {
					for (size_t i = 0; i < dec["content"].size(); i++) {
						auto c = dec["content"].at(i);
						if (c.contains("type") && c.contains("text") && c["type"].is_string() && c["text"].is_string()) {
							if (!stricmp(c["type"].get<string>().c_str(), "text")) {
								ClaudeMessage m;
								m.role = "assistant";
								m.content = c["text"].get<string>();
								reply.push_back(std::move(m));
							}
						}
					}
					ret = (reply.size() > 0);
				}
			} else {
				api->ib_printf(_("Claude -> Missing/invalid fields in reply from Claude!\n"));
				api->ib_printf(_("Claude -> Reply was: %s\n"), data.c_str());
			}
		} catch (exception& e) {
			api->ib_printf(_("Claude -> Error parsing reply from Claude: %s\n"), e.what());
			api->ib_printf(_("Claude -> Reply was: %s\n"), data.c_str());
		}
	} else {
		api->ib_printf(_("Claude -> Error calling Claude: %s\n"), api->curl->easy_strerror(res));
	}
	api->curl->easy_cleanup(ch);
	api->curl->slist_free_all(headers);
	return ret;
}

static const char * prompts[] = {
	"give me a one line blurb about the song %songpart%",
	"give me a two line blurb about the song %songpart%",
	"give me a two line blurb about the band %bandpart%",
	"give me a two line blurb about the band %bandpart%",
};
static const size_t num_prompts = sizeof(prompts) / sizeof(const char *);

int message_proc(unsigned int msg, char * data, int datalen) {
	if (msg == IB_ONTEXT) {
		IBM_USERTEXT * ut = (IBM_USERTEXT *)data;
		if (ut->type == IBM_UTT_PRIVMSG && !claude_config.shutdown_now && ut->userpres->User && !api->user->uflag_have_any_of(ut->userpres->Flags, UFLAG_IGNORE) && !api->commands->IsCommandPrefix(ut->text[0])) {
			chatMutex.Lock();
			shared_ptr<ChatHistory> c;
			const char * nick = (ut->userpres->User != NULL) ? ut->userpres->User->Nick : ut->userpres->Nick;
			auto x = sessions.find(nick);
			if (x != sessions.end()) {
				c = x->second;
				c->Pres = ut->userpres;
			} else {
				c = make_shared<ChatHistory>(ut->userpres);
				sessions[nick] = c;
			}
			c->lastSeen = time(NULL);
			c->new_lines.push(ut->text);
			new_messages.push_back(nick);
			chatMutex.Release();
		}
	} else if (msg == IB_SS_DRAGCOMPLETE) {
		bool * has_changed = (bool *)data;
		if (*has_changed) {
			if (--claude_config.songLeft <= 0) {
				char buf[256];
				sstrcpy(buf, prompts[api->genrand_int32() % num_prompts]);
				char songpart[256];
				char bandpart[256];
				IBM_SONGINFO si;
				memset(&si, 0, sizeof(si));
				if (api->SendMessage(-1, SOURCE_GET_SONGINFO, (char *)&si, sizeof(si)) > 0 && si.artist[0] && si.title[0]) {
					snprintf(songpart, sizeof(songpart), "\"%s\" by \"%s\"", si.title, si.artist);
					snprintf(bandpart, sizeof(bandpart), "%s", si.artist);
				} else {
					snprintf(songpart, sizeof(songpart), "\"%s\"", api->ss->GetStreamInfo()->cursong);
					snprintf(bandpart, sizeof(bandpart), "%s", api->ss->GetStreamInfo()->cursong);
					char * p = strchr(bandpart, '-');
					if (p != NULL) {
						*p = 0;
						strtrim(bandpart);
					}
				}
				str_replace(buf, sizeof(buf), "%songpart%", songpart);
				str_replace(buf, sizeof(buf), "%bandpart%", bandpart);
				api->ProcText(buf, sizeof(buf));
				ClaudeMessage msg;
				msg.role = "user";
				msg.content = buf;
				list<ClaudeMessage> context = { msg };
				list<ClaudeMessage> reply;
				if (claude_query(context, reply)) {
					for (auto& x : reply) {
						if (!x.content.empty()) {
							char* tmp = strdup(x.content.c_str());
							str_replace(tmp, strlen(tmp) + 1, "\r", "");
							while (strstr(tmp, "\n\n") != NULL) {
								str_replace(tmp, strlen(tmp) + 1, "\n\n", "\n");
							}
							char* p2 = NULL;
							char* p = strtok_r(tmp, "\n", &p2);
							while (p != NULL) {
								char* tmp2 = strdup(p);
								strtrim(tmp2);
								if (tmp2[0]) {
									api->BroadcastMsg(NULL, tmp2);
								}
								free(tmp2);
								p = strtok_r(NULL, "\n", &p2);
							}
							free(tmp);
						}
					}
				}
				claude_config.songLeft = api->genrand_range(claude_config.songMin, claude_config.songMax);
			}
		}
	} else if (msg == IB_SHUTDOWN) {
		claude_config.shutdown_now = true;
		//main listening socket will be closed by ChatThread when it exits
		time_t timeo = time(NULL) + 30;
		while (TT_NumThreadsWithID(pluginnum) && time(NULL) < timeo) {
			api->ib_printf(_("Claude -> Waiting on (%d) threads to die...\n"), TT_NumThreadsWithID(pluginnum));
			api->safe_sleep_seconds(1);
		}
		chatMutex.Lock();
		sessions.clear();
		new_messages.clear();
		chatMutex.Release();
	}

	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{43447F56-AB9F-422A-9849-EFAD3FE6790F}",
	"Claude Support",
	"Claude Support Plugin 1.0",
	"Drift Solutions",
	init,
	quit,
	message_proc,
	NULL,
	NULL,
	NULL,
	NULL,
};

PLUGIN_EXPORT_FULL

