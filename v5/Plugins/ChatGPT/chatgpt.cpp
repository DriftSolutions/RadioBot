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

#include "chatgpt.h"
#include "json.hpp"
using json = nlohmann::json;

BOTAPI_DEF *api = NULL;
int pluginnum; // the number we were assigned

CHATGPT_CONFIG chatgpt_config;
Titus_Mutex chatMutex;
chatListType sessions;
queue<string> new_messages;

bool LoadConfig() {
	memset(&chatgpt_config,0,sizeof(chatgpt_config));
	chatgpt_config.songMin = 4;
	chatgpt_config.songMax = 6;
	DS_CONFIG_SECTION * sec = api->config->GetConfigSection(NULL, "ChatGPT");
	if (sec == NULL) {
		api->ib_printf(_("ChatGPT -> No 'ChatGPT' section found in config, disabling...\n"));
		return false;
	}

	if (!api->config->GetConfigSectionValueBuf(sec, "API_Key", chatgpt_config.api_key, sizeof(chatgpt_config.api_key)) || chatgpt_config.api_key[0] == 0) {
		api->ib_printf(_("ChatGPT -> No 'API_Key' set in your 'ChatGPT' section, disabling...\n"));
		return false;
	}

	char buf[64];
	if (api->config->GetConfigSectionValueBuf(sec, "EnableSongAnnounce", buf, sizeof(buf))) {
		api->textfunc->get_range(buf, &chatgpt_config.songMin, &chatgpt_config.songMax);
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

	TT_StartThread(ChatThread, NULL, _("ChatGPT Chat Thread"), pluginnum);
	api->ib_printf(_("ChatGPT -> Ready...\n"));
	return 1;
}

void quit() {
	api->ib_printf(_("ChatGPT -> Shutting down...\n"));
	chatgpt_config.shutdown_now=true;
	//main listening socket will be closed by ChatThread when it exits
	time_t timeo = time(NULL) + 30;
	while (TT_NumThreadsWithID(pluginnum) && time(NULL) < timeo) {
		api->ib_printf(_("ChatGPT -> Waiting on (%d) threads to die...\n"), TT_NumThreadsWithID(pluginnum));
		api->safe_sleep_seconds(1);
	}
	chatMutex.Lock();
	for (chatListType::iterator x = sessions.begin(); x != sessions.end(); x = sessions.begin()) {
		delete x->second;
		sessions.erase(x);
	}
	while (new_messages.size()) {
		new_messages.pop();
	}
	chatMutex.Release();
	api->ib_printf(_("ChatGPT -> Shut down.\n"));
}

static size_t curl_write_string(void *data, size_t size, size_t nmemb, void *clientp) {
	size_t realsize = size * nmemb;
	string * str = (string *)clientp;
	str->append((const char *)data, realsize);
	return realsize;
}

bool chatgpt_query(const string& prompt, string& reply, const vector<string> * stop) {
	CURL * ch = api->curl->easy_init();
	if (ch == NULL) {
		api->ib_printf(_("ChatGPT -> Error creating cURL handle!\n"));
		return false;
	}

	curl_slist * headers = api->curl->slist_append(NULL, "Content-Type: application/json");
	stringstream key;
	char * tmp = api->curl->escape(chatgpt_config.api_key, 0);
	key << "Authorization: Bearer " << tmp;
	api->curl->free(tmp);
	headers = api->curl->slist_append(headers, key.str().c_str());

	json post_data = {
		{"model", "text-davinci-003"},
		{"prompt", prompt},
		{"temperature", 0.5},
		{"max_tokens", 256},
		{"top_p", 1},
		{"frequency_penalty", 0.5},
		{"presence_penalty", 0.0}
	};
	if (stop != NULL) {
		post_data["stop"] = *stop;
	}

	bool ret = false;
	string data;
	api->curl->easy_setopt(ch, CURLOPT_URL, "https://api.openai.com/v1/completions");
	//api->curl->easy_setopt(ch, CURLOPT_URL, "https://api.openai.com/v1/chat/completions");
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
	//printf("request: %s\n", post_data.dump().c_str());
	CURLcode res = api->curl->easy_perform(ch);
	if (res == CURLE_OK) {
		//printf("reply: %s\n", data.c_str());
		try {
			json dec = json::parse(data);
			if (dec.find("choices") != dec.end() && dec["choices"].is_array() && dec["choices"].size() > 0) {
				auto c = dec["choices"].at(0);
				if (c.find("text") != c.end() && c["text"].is_string()) {
					reply = c["text"];
					char * tmp = strdup(reply.c_str());
					strtrim(tmp);
					reply = tmp;
					free(tmp);
					ret = (reply.length() > 0);
				}
			}
		} catch (exception& e) {
			api->ib_printf(_("ChatGPT -> Error parsing reply from ChatGPT: %s\n"), e.what());
			api->ib_printf(_("ChatGPT -> Reply was: %s\n"), data.c_str());
		}
	} else {
		api->ib_printf(_("ChatGPT -> Error calling ChatGPT: %s\n"), api->curl->easy_strerror(res));
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
		if (ut->type == IBM_UTT_PRIVMSG && !api->commands->IsCommandPrefix(ut->text[0])) {
			chatMutex.Lock();
			ChatHistory * c = NULL;
			const char * nick = (ut->userpres->User != NULL) ? ut->userpres->User->Nick : ut->userpres->Nick;
			auto x = sessions.find(nick);
			if (x != sessions.end()) {
				c = x->second;
				c->Pres = ut->userpres;
			} else {
				c = new ChatHistory(ut->userpres);
				sessions[nick] = c;
			}
			c->lastSeen = time(NULL);
			c->new_lines.push(ut->text);
			new_messages.push(nick);
			chatMutex.Release();
		}
	} else if (msg == IB_SS_DRAGCOMPLETE) {
		bool * has_changed = (bool *)data;
		if (*has_changed) {
			if (--chatgpt_config.songLeft <= 0) {
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
				string str;
				if (chatgpt_query(buf, str)) {
					//api->BroadcastMsg(NULL, buf);
					api->BroadcastMsg(NULL, str.c_str());
				}
				chatgpt_config.songLeft = api->genrand_range(chatgpt_config.songMin, chatgpt_config.songMax);
			}
		}
	}
	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{881B9141-81D2-4BC2-8349-73533BF1FEF8}",
	"ChatGPT Support",
	"ChatGPT Support Plugin 1.0",
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

