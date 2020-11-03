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

#if defined(IRCBOT_ENABLE_IRC)
bool IRCNETS::IsChannelPrefix(char c) {
	char * p = channel_types;
	while (*p != 0) {
		if (*p++ == c) {
			return true;
		}
	}
	return false;
}

/*
A = Mode that adds or removes a nick or address to a list. Always has a parameter.
B = Mode that changes a setting and always has a parameter.
C = Mode that changes a setting and only has a parameter when set.
D = Mode that changes a setting and never has a parameter.

Note: Modes of type A return the list when there is no parameter present.

Note: Some clients assumes that any mode not listed is of type D.

Note: Modes in PREFIX are not listed but could be considered type B.
*/

void IRCNETS::ResetChanModes() {
	memset(chmode_handling, 0, sizeof(chmode_handling));
	for (int i=0; i < NUM_IRC_MODES; i++) {
		chmode_handling[i] = 4;
	}
}
void IRCNETS::LoadChanModes(string str, bool reset) {
	// "b,k,l,mnpst"
	if (reset) {
		ResetChanModes();
	}

	StrTokenizer st((char *)str.c_str(), ',', true);
	for (unsigned long i=1; i <= st.NumTok(); i++) {
		char * modes = st.GetSingleTok(i);
		char * tmp = modes;
		while (*tmp) {
			if (*tmp >= 'A' && *tmp <= 'Z') {
				int ind = *tmp-'A'+26;
				//ib_printf("ind %c: %d\n", *tmp, ind);
				chmode_handling[ind] = i;
			}
			if (*tmp >= 'a' && *tmp <= 'z') {
				int ind = *tmp-'a';
				//ib_printf("ind %c: %d\n", *tmp, ind);
				chmode_handling[ind] = i;
			}
			tmp++;
		}
		st.FreeString(modes);
	}
	/*
	chmode_handling['n'] = 4;
	chmode_handling['p'] = 4;
	chmode_handling['s'] = 4;
	chmode_handling['t'] = 4;
	chmode_handling['m'] = 4;
	*/
}

void IRCNETS::UpdateChanModesFromUModes() {
	for (int i=1; i < MAX_IRC_CHUMODES && ch_umodes[i]; i += 2) {
		char tmp = ch_umodes[i];
		if (tmp >= 'A' && tmp <= 'Z') {
			tmp = tmp-'A'+26;
			chmode_handling[tmp] = 2;
		}
		if (tmp >= 'a' && tmp <= 'z') {
			tmp = tmp-'a';
			chmode_handling[tmp] = 2;
		}
	}
}

void IRCNETS::LoadChUserModes(string str) {
	//="(ohv)@%+"
	char * tmp = zstrdup(str.c_str());
	if (tmp[0] != '(') {
		ib_printf("LoadChUserModes(): Invalid PREFIX line: %s\n", tmp);
		zfree(tmp);
		return;
	}
	char * mode = tmp+1;
	char * pre = strchr(tmp, ')');
	if (pre == NULL) {
		ib_printf("LoadChUserModes(): Invalid PREFIX line: %s\n", tmp);
		zfree(tmp);
		return;
	}
	pre++;

	memset(ch_umodes, 0, sizeof(ch_umodes));
	int ind = 0;
	while (*mode != ')' && *pre != 0 && ind < (MAX_IRC_CHUMODES*2)) {
		ch_umodes[ind++] = *pre++;
		ch_umodes[ind++] = *mode++;
	}

	zfree(tmp);
}

void IRCNETS::Reset() {

	my_mode[0] = 0;
	cur_nick.clear();
	host.clear();
	pass.clear();
	bindip.clear();
	network.clear();
	server.clear();
	onconnect.clear();
	on512.clear();
	onnickinusecommand.clear();
	removePreText.clear();
	custIdent.clear();

	port = num_chans = ssl = 0;
	sock = NULL;
	log_fp = NULL;
	last_recv = readyTime = 0;

	for (int i=0; i < 16; i++) {
		channels[i].Reset(true);
	}

	log = ready = shortIdent = false;
	enable_cap = regainNick = true;

	sstrcpy(channel_types, "#");
	LoadChUserModes();
	ResetChanModes();
	UpdateChanModesFromUModes();
#if defined(DEBUG)
	log = true;
#endif
}

bool CHANNELS::Reset(bool first) {
	channel.clear();
	altTopicCommand.clear();
	altJoinCommand.clear();
	key.clear();
	topic.clear();
	modes[0] = 0;
	doonjoin = joined = noTopicCheck = autoVoice = false;
	dospam = dotopic = true;
	songinterval = 0;

	if (first) {
		lastsong = 0;
	}
	return true;
}
#endif // defined(IRCBOT_ENABLE_IRC)

void BASE_CFG::Reset() {
#if defined(IRCBOT_ENABLE_IRC)
	irc_nick.clear();
	log_chan.clear();
	log_chan_key.clear();
#endif
	base_path.clear();
	log_file.clear();
	pid_file.clear();
	myip.clear();
	bind_ip.clear();
	langCode.clear();
	command_prefixes = "!@?";
	memset(&log_fps, 0, sizeof(log_fps));
	default_flags = UFLAG_REQUEST;

	AutoRegOnHello = Fork = Forked = OnlineBackup = false;
	sendq = argc = 0;
	backup_days = 14;

#if defined(IRCBOT_ENABLE_SS)
	dj_name_mode = DJNAME_MODE_STANDARD;
	enable_requests = enable_find = true;
	max_find_results = 10;
	expire_find_results = 15;
	multiserver = 0;
	updinterval = 30;
	req_mode = REQ_MODE_OFF;
#if defined(IRCBOT_ENABLE_IRC)
	req_fallback = "";
	req_modes_on_login = "";
#endif
	anyserver = false;
	allow_pm_requests = true;
	EnableRatings = false;
	MaxRating = 5;
#endif

	argv.clear();
#if defined(IRCBOT_ENABLE_IRC)
	quitmsg.clear();
#endif

	reg_name.clear();
	reg_key.clear();

	ssl_cert.clear();
	fnConf = IRCBOT_CONF;
	fnText = IRCBOT_TEXT;
}

void GLOBAL_CONFIG::Reset() {
	sockets = NULL;
	config = NULL;
	hSQL = NULL;
	//HINSTANCE hInst;
	sesFirst = sesLast = NULL;
	//fUser = lUser = NULL;

	sesMutex.Lock();
	messages.clear();
	customVars.clear();
#if defined(IRCBOT_ENABLE_IRC)
	topicOverride.clear();
#endif
	sesMutex.Release();

	base.Reset();
#if defined(IRCBOT_ENABLE_SS)
	memset(&stats, 0, sizeof(stats));
	memset(&s_stats, 0, sizeof(s_stats));
#endif
	memset(&remote, 0, sizeof(remote));
	remote.port = 10001;

	int i=0;
#if defined(IRCBOT_ENABLE_IRC)
	for (i=0; i < MAX_IRC_SERVERS; i++) {
		ircnets[i].Reset();
	}
	for (i=0; i < MAX_TIMERS; i++) {
		timers[i].action.clear();
		timers[1].lines.clear();
		timers[i].delaymin = timers[i].delaymax = timers[i].network = timers[i].nextFire = 0;
	}
#endif
#if defined(IRCBOT_ENABLE_SS)
	for (i=0; i < MAX_SOUND_SERVERS; i++) {
		s_servers[i].host.clear();
		s_servers[i].mount.clear();
		s_servers[i].user.clear();
		s_servers[i].pass.clear();
		//s_servers[i].has_source = false;
		//s_servers[i].lastlisten = s_servers[i].lastpeak =
		s_servers[i].port = 0;
		s_servers[i].type = SS_TYPE_SHOUTCAST;
	}
#endif
	for (i=0; i < MAX_PLUGINS; i++) {
		plugins[i].fn.clear();
		plugins[i].name.clear();
		plugins[i].hHandle = NULL;
		plugins[i].plug = NULL;
	}

#if defined(IRCBOT_ENABLE_IRC)
	dotopic = dospam = true;
	num_irc = num_timers = 0;
#endif
	shutdown_now = shutdown_reboot = shutdown_sendq = rehash = false;
#if defined(IRCBOT_ENABLE_SS)
	num_ss = 0;
	req_user = NULL;
	req_iface = NULL;
#endif
#ifdef WIN32
	hInst = GetModuleHandle(NULL);
	cSock = NULL;
	cPort = 0;
#endif
	start_time = time(NULL);
	unload_plugin = -1;
}

