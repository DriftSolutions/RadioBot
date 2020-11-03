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

 #include "ircbot.h"

//MSG_QUEUE *fMsg=NULL,*lMsg=NULL;

#if defined(IRCBOT_ENABLE_SS)

int GetSSInfo(int32 ssnum, SOUND_SERVER * ss) {
	if (ssnum > config.num_ss) { return false; }
	if (ssnum < 0) { return config.num_ss; }
	sstrcpy(ss->host, config.s_servers[ssnum].host.c_str());
	sstrcpy(ss->mount, config.s_servers[ssnum].mount.c_str());
	sstrcpy(ss->admin_pass, config.s_servers[ssnum].pass.c_str());
	ss->port = config.s_servers[ssnum].port;
	ss->type = config.s_servers[ssnum].type;
	ss->streamid = config.s_servers[ssnum].streamid;
	ss->has_source = config.s_stats[ssnum].online;
	return true;
}

STATS * GetStreamInfo() {
	return &config.stats;
}

void EnableRequests(bool activate) {
	if (activate) {
		RequestsOn(NULL, REQ_MODE_HOOKED);
	} else {
		RequestsOff();
	}
}

REQUEST_MODES GetRequestMode() {
	AutoMutex(requestMutex);
	return config.base.req_mode;
}

#endif

void BroadcastMsg(USER_PRESENCE * ut, const char * buf) {
	if (!config.dospam) { return; }

#if defined(IRCBOT_ENABLE_IRC)
	for (int net=0; net < config.num_irc; net++) {
		for (int chan=0; config.ircnets[net].ready == true && chan < config.ircnets[net].num_chans; chan++) {
			if (config.ircnets[net].channels[chan].dospam) {
				stringstream sstr;
				sstr << "PRIVMSG " << config.ircnets[net].channels[chan].channel << " :" << buf << "\r\n";
				bSend(config.ircnets[net].sock,sstr.str().c_str(),sstr.str().length(),PRIORITY_INTERACTIVE);
			}
		}
	}
#endif

	IBM_USERTEXT ibm;
	memset(&ibm, 0, sizeof(ibm));
	ibm.text = buf;
	ibm.type = IBM_UTT_PRIVMSG;
	ibm.userpres = ut;
	SendMessage(-1, IB_BROADCAST_MSG, (char *)&ibm, sizeof(ibm));
}

bool GetDoSpam() {
	return config.dospam;
}

void SetDoSpam(bool dospam) {
	config.dospam = dospam;
}

#if defined(IRCBOT_ENABLE_IRC)
void SetDoSpam(bool dospam, int netno, const char * chan) {
	CHANNELS * c = GetChannel(netno, chan);
	if (c) {
		c->dospam = dospam;
	}
}

void SetDoOnJoin(bool dospam) {
	config.doonjoin = dospam;
}

void SetDoOnJoin(bool dospam, int netno, const char * chan) {
	CHANNELS * c = GetChannel(netno, chan);
	if (c) {
		c->doonjoin = dospam;
	}
}

bool IsIRCSocket(T_SOCKET * sock) {
	if (sock != NULL) {
		for (int i=0; i < config.num_irc; i++) {
			if (config.ircnets[i].sock == sock) {
				return true;
			}
		}
	}
	return false;
}

int SendIRC_Priority(int netno, const char * buf, int32 datalen, uint8 priority) {
	if (netno >= 0 && netno < MAX_IRC_SERVERS && config.ircnets[netno].ready == true) {
		return bSend(config.ircnets[netno].sock,buf,datalen,priority);
	} else {
		return -1;
	}
}

int SendIRC_Delay(int netno, const char * buf, int32 datalen, uint8 priority, uint32 delay) {
	if (netno >= 0 && netno < MAX_IRC_SERVERS && config.ircnets[netno].ready == true) {
		return bSend(config.ircnets[netno].sock,buf,datalen,priority,delay);
	} else {
		return -1;
	}
}

bool LogToChan(const char * buf) {
	if (config.ircnets[0].ready == true && config.base.log_chan.length()) {
		char * buf2 = (char *)zmalloc(strlen(buf) + 256);
		sprintf(buf2,"PRIVMSG %s :%s\r\n",config.base.log_chan.c_str(),buf);
		bSend(config.ircnets[0].sock, buf2, strlen(buf2), PRIORITY_LOWEST);
		zfree(buf2);
		return true;
	}
	return false;
}

const char * GetCurrentNick(int32 net) {
	if (net >= 0 && net < MAX_IRC_SERVERS) {
		if (config.ircnets[net].ready) {
			return config.ircnets[net].cur_nick.c_str();
		} else {
			return config.ircnets[net].base_nick.c_str();
		}
	}
	return NULL;
}

const char * GetDefaultNick(int32 net) {
	if (net >= 0 && net < MAX_IRC_SERVERS) {
		return config.ircnets[net].base_nick.c_str();
	}
	return config.base.irc_nick.c_str();
}

int NumNetworks() {
	return config.num_irc;
}

bool IsNetworkReady(int32 num) {
	if (num < 0 || num >= config.num_irc) { return false; }
	return (config.ircnets[num].ready && config.ircnets[num].sock) ? true:false;
}

#endif // defined(IRCBOT_ENABLE_IRC)

DS_CONFIG_SECTION * GetConfigSection(DS_CONFIG_SECTION * parent, const char * name) {
	return config.config->GetSection(parent, (char *)name);
};

char * GetConfigSectionValue(DS_CONFIG_SECTION * section, const char * name) {
	if (section == NULL) { return NULL; }
	DS_VALUE * val = config.config->GetSectionValue(section, (char *)name);
	if (!val) { return NULL; }

	if (val->type == DS_TYPE_STRING) {
		return val->pString;
	}

	return NULL;
};

bool GetConfigSectionValueBuf(DS_CONFIG_SECTION * section, const char * name, char * buf, size_t bufSize) {
	if (section == NULL) { return false; }
	DS_VALUE * val = config.config->GetSectionValue(section, (char *)name);
	if (!val) {
		return false;
	}

	memset(buf, 0, bufSize);
	switch(val->type) {
		case DS_TYPE_STRING:
			strlcpy(buf, val->pString, bufSize);
			return true;
			break;
		case DS_TYPE_LONG:
			snprintf(buf, bufSize, "%d", val->lLong);
			return true;
			break;
		case DS_TYPE_FLOAT:
			snprintf(buf, bufSize, "%f", val->lFloat);
			return true;
			break;

		default:
			break;
	}
	return false;
};

long GetConfigSectionLong(DS_CONFIG_SECTION * section, const char * name) {
	if (section == NULL) { return -1; }
	DS_VALUE * val = config.config->GetSectionValue(section, (char *)name);
	if (!val) { return -1; }

	switch(val->type) {
		case DS_TYPE_STRING:
			return atol(val->pString);
			break;
		case DS_TYPE_LONG:
			return val->lLong;
			break;
		case DS_TYPE_FLOAT:
			return val->lFloat;
			break;

		default:
			break;
	}

	return -1;
};

double GetConfigSectionFloat(DS_CONFIG_SECTION * section, const char * name) {
	if (section == NULL) { return -1; }
	DS_VALUE * val = config.config->GetSectionValue(section, (char *)name);
	if (!val) { return -1; }

	switch(val->type) {
		case DS_TYPE_STRING:
			return atof(val->pString);
			break;
		case DS_TYPE_LONG:
			return val->lLong;
			break;
		case DS_TYPE_FLOAT:
			return val->lFloat;
			break;
		default:
			break;
	}

	return -1;
};

bool IsConfigSectionValue(DS_CONFIG_SECTION * section, const char * name) {
	if (section == NULL) { return false; }
	DS_VALUE * val = config.config->GetSectionValue(section, (char *)name);
	if (val) { return true; }
	return false;
}

int32 NumPlugins() {
	int ret=0;
	for (int i=0; i < MAX_PLUGINS; i++) {
		if (config.plugins[i].hHandle && config.plugins[i].plug) {
			ret++;
		}
	}
	return ret;
}

PLUGIN_PUBLIC * GetPlugin(int32 num) {
	if (num >= 0 && num < MAX_PLUGINS) {
		return config.plugins[num].plug;
	}
	return NULL;
}

void safe_sleep_seconds(int32 sleepfor) {
	safe_sleep(sleepfor,false);
}
void safe_sleep_milli(int32 sleepfor) {
	safe_sleep(sleepfor,true);
}

#if defined(IRCBOT_ENABLE_SS)
bool AreRatingsEnabled() {
	return config.base.EnableRatings;
}
unsigned int GetMaxRating() {
	return config.base.MaxRating;
}
#endif

/*
bool HasOwnerSupport(int32 num) {
	return (config.ircnets[num].ready && config.ircnets[num].sock && config.ircnets[num].has_owner) ? true:false;
}
*/

void Rehash(const char * newfn) {
	if (newfn) {
		config.base.fnText = newfn;
	}
	config.rehash = true;
}

int wstr_replace(char * buf, int32 bufSize, const char * find, const char * replace) {
	return str_replaceA(buf, bufSize, find, replace);
}

int cwildcmp(const char *wild, const char *string) {
	return wildcmp(wild, string);
}
int cwildicmp(const char *wild, const char *string) {
	return wildicmp(wild, string);
}

char * cstrtrim(char * str, const char * trim) {
	return strtrim(str, trim);
}

const char * GetBasePath() { return config.base.base_path.c_str(); }

API_textfunc textfuncs = {
	wstr_replace,
	cstrtrim,
	duration2,
	decode_duration,
	cwildcmp,
	cwildicmp,
	get_range
};

void UnregisterCommand2(COMMAND * command) {
	UnregisterCommand(command);
}

bool IsCommandPrefix(char ch) {
	const char * chars = config.base.command_prefixes.c_str();
	for (int i=0; chars[i] != 0; i++) {
		if (chars[i] == ch) { return true; }
	}
	return false;
}

char PrimaryCommandPrefix() {
	return config.base.command_prefixes.c_str()[0];
}

const char * FindMagicCommandPrefixes(const char * str) {
	const char * ret = NULL;
	char buf[3] = {0,0,0};
	const char * chars = config.base.command_prefixes.c_str();
	for (int i=0; chars[i] != 0; i++) {
		buf[0] = buf[1] = chars[i];
		if ((ret = strstr(str, buf)) != NULL) { break; }
	}
	return ret;
}

API_commands commandfuncs = {
	FindCommand,
	RegisterCommand2,
	RegisterCommand2,
	UnregisterCommandsByPlugin,
	UnregisterCommand,
	UnregisterCommand,

	PrimaryCommandPrefix,
	IsCommandPrefix,

	FillACL,
	MakeACL,

	ExecuteProc,
	RegisterCommandHook,
	UnregisterCommandHook

//	CommandOutput_IRC_PM,
//	CommandOutput_IRC_Notice
};

sqlite3 * GetHandle() { return config.hSQL; }

API_db dbfuncs = {
	GetHandle,
	Query,
	Free,
	GetTable,
	GetTableEntry,
	//GetTable,
	FreeTable,
	MPrintf,
	LockDB,
	ReleaseDB,
	InsertID,
};

#if defined(IRCBOT_ENABLE_IRC)
bool is_nick_in_chan(int netno, const char * chan, const char * nick) {
	AutoMutex(config.ircnets[netno].ial->hMutex);
	return (config.ircnets[netno].ready && config.ircnets[netno].ial->IsNickOnChan(chan, nick));
}
bool is_bot_in_chan(int netno, const char * chan) {
	AutoMutex(config.ircnets[netno].ial->hMutex);
	CHANNELS * schan = GetChannel(netno, chan);
	return (schan && config.ircnets[netno].ready && schan->joined);
}

int num_nicks_in_chan(int netno, const char * chan) {
	AutoMutex(config.ircnets[netno].ial->hMutex);
	CHANNELS * schan = GetChannel(netno, chan);
	if (schan && config.ircnets[netno].ready && schan->joined) {
		IAL_CHANNEL * c = config.ircnets[netno].ial->GetChan(chan, true);
		if (c) {
			return c->nicks.size();
		}
	}
	return 0;
}
bool get_nick_in_chan(int netno, const char * chan, int num, char * nick, int nickSize) {
	AutoMutex(config.ircnets[netno].ial->hMutex);
	CHANNELS * schan = GetChannel(netno, chan);
	if (schan && config.ircnets[netno].ready && schan->joined) {
		IAL_CHANNEL * c = config.ircnets[netno].ial->GetChan(chan, true);
		if (c) {
			ialChannelNickList::const_iterator x = c->nicks.begin();
			while (num-- && x != c->nicks.end()) {
				x++;
			}
			if (x != c->nicks.end()) {
				strlcpy(nick, x->Nick->nick.c_str(), nickSize);
				return true;
			}
		}
	}
	return false;
}


API_ial ialfuncs = {
	is_nick_in_chan,
	is_bot_in_chan,
	num_nicks_in_chan,
	get_nick_in_chan
};
#endif

const char * GetIP() {
	return (char *)config.base.myip.c_str();
}

char * GetBindIP(char * buf, size_t bufSize) {
	*buf=0;
	if (config.base.bind_ip.length()) {
		strlcpy(buf, config.base.bind_ip.c_str(), bufSize);
		return buf;
	}
	return NULL;
}

int get_argc() { return config.base.argc; }
const char * get_argv(int i) {
	std::map<int, std::string>::const_iterator x = config.base.argv.find(i);
	if (x != config.base.argv.end()) {
		return x->second.c_str();
	}
	return NULL;
}

uint32 GetDefaultFlags() {
	return config.base.default_flags;
}

void SetCustomVar(const char * name, const char * val) {
	if (name == NULL || val == NULL) { return; }
	sesMutex.Lock();
	config.customVars[name] = val;
	sesMutex.Release();
}
void DeleteCustomVar(const char * name) {
	if (name == NULL) { return; }
	sesMutex.Lock();
	message_list::iterator x = config.customVars.find(name);
	if (x != config.customVars.end()) {
		config.customVars.erase(x);
	}
	sesMutex.Release();
}

API_curl curlfuncs = {
	curl_easy_init,
	curl_easy_setopt,
	curl_easy_perform,
	curl_easy_cleanup,
	curl_easy_strerror,
	curl_easy_getinfo,

	curl_multi_init,
	curl_multi_add_handle,
	curl_multi_remove_handle,
	curl_multi_perform,
	curl_multi_cleanup,
	curl_multi_info_read,
	curl_multi_strerror,

	curl_formadd,
	curl_formfree,
	curl_slist_append,
	curl_slist_free_all,
	curl_escape,
	curl_unescape,
	curl_free
};

API_memory memfuncs = {
	dsl_malloc,
	dsl_realloc,
	dsl_strdup,
	dsl_wcsdup,
	dsl_free
};

API_user userfuncs = {
	GetUser,
	GetUserByNick,
	IsValidUserName,
	GetUserCount,
	EnumUsersByLastHostmask,
	GetUserFlags,
	AddUser,
	SetUserFlags,
	AddUserFlags,
	DelUserFlags,
	GetDefaultFlags,

	uflag_have_any_of,
	uflag_compare,
	uflag_to_string,
	string_to_uflag,

	mask,

	GetLevelFlags,
	SetLevelFlags,
};

API_config configfuncs = {
	GetConfigSection,
	GetConfigSectionValueBuf,
	GetConfigSectionValue,
	GetConfigSectionLong,
	GetConfigSectionFloat,
	IsConfigSectionValue
};

API_yp ypfuncs = {
	AddToYP,
	TouchYP,
	DelFromYP
};

#if defined(IRCBOT_ENABLE_IRC)
API_irc ircfuncs = {
	LogToChan,
	SendIRC_Priority,
	SendIRC_Delay,
	bSend,
	SendQ_ClearSockEntries,
	GetDoSpam,
	SetDoSpam,
	SetDoSpam,
	SetDoOnJoin,
	SetDoOnJoin,
	GetCurrentNick,
	GetDefaultNick,
	NumNetworks,
	IsNetworkReady
};
#endif

#if defined(IRCBOT_ENABLE_SS)
API_ss ssfuncs = {
	GetSSInfo,
	GetStreamInfo,
	GetRequestMode,
	EnableRequests,
	RequestsOn,
	AreRatingsEnabled,
	GetMaxRating,
	RateSong,
	RateSong,
	GetSongRating,
	GetSongRating
};
#endif

BOTAPI BotAPI = {
	GetBotVersion,
	GetBotVersionString,
	GetIP,
	PLATFORM,
	NULL,
	GetBasePath,
	GetBindIP,
	get_argc,
	get_argv,
	ircbot_get_cycles,
	ib_printf2,
	Rehash,
	NumPlugins,
	GetPlugin,
	LoadPlugin,
	LoadMessage,
	ProcText,
	SetCustomVar,
	DeleteCustomVar,
	BroadcastMsg,
	GetLangString,
	safe_sleep_seconds,
	safe_sleep_milli,
	genrand_int32,
	genrand_range,
	SendRemoteReply,
	SendMessage,
	&ircfuncs,
	&ialfuncs,
	&textfuncs,
	&commandfuncs,
	&userfuncs,
	&configfuncs,
	&dbfuncs,
	&curlfuncs,
	&memfuncs,

#if defined(IRCBOT_ENABLE_SS)
	&ypfuncs,
	&ssfuncs,
#else
	NULL,
	NULL,
#endif
};

BOTAPI * GetAPI() { return &BotAPI; }
