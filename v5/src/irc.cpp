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
#include "build.h"

#define IRC_TIMEOUT 150
#define IRC_BUFFER_SIZE 8192

#define ERR_UNKNOWNCOMMAND "421"
#define ERR_NOTREGISTERED "451"
#define ERR_NOMOTD "422"
#define RPL_MOTDSTART "375"
#define RPL_MOTD "372"
#define RPL_ENDOFMOTD "376"
#define ERR_NOMOTD "422"
/// invalid chars in nickname
#define ERR_ERRONEUSNICKNAME "432"
#define ERR_NICKNAMEINUSE "433"
#define ERR_NICKCOLLISION "436"
#define RPL_NOTOPIC "331"
#define RPL_TOPIC "332"
#define RPL_TOPICTIME "333"
#define RPL_ENDOFWHO "315"
#define RPL_WHOREPLY "352"
#define RPL_NAMEREPLY "353"
#define RPL_ENDOFNAMES "366"

#define ERR_NOEXTMESSAGES "404"
//"<channel> :No external channel messages (<channel>)"
#define ERR_NOTONCHANNEL "442"
//"<channel> :You're not on that channel"
#define ERR_CHANNELISFULL "471"
//"<channel> :Cannot join channel (+l)"
#define ERR_INVITEONLYCHAN "473"
//"<channel> :Cannot join channel (+i)"
#define ERR_BANNEDFROMCHAN "474"
//"<channel> :Cannot join channel (+b)"
#define ERR_BADCHANNELKEY "475"
//"<channel> :Cannot join channel (+k)"
#define ERR_NEEDREGGEDNICK "477"
//"<channel> :Need registered nick blah blah"
#define ERR_NOTCHANOP "482"
//<channel> :You're not channel operator

#define RPL_STARTTLS "670"
#define ERR_STARTTLS "691"

int irc_read_line(int netno, char * buf, char * buf3, int * pbufind) {
	int bufcnt = *pbufind;

	int n=0;

	if (bufcnt > 0) {
		char *q = buf3;//strchr(buf3,'\n');
		char *p = NULL;
		for (int i=0; i < bufcnt; i++, q++) {
			if (*q == '\n' || *q == '\r') {
				if (q[0] == '\r' && q[1] == '\n') { q++; }
				p = q;
				break;
			}
		}
		if (p != NULL) {
			n = (p - buf3) + 1;
			memcpy(buf, buf3, n);
			buf[n] = 0;
			if (n < bufcnt) {
				bufcnt -= n;
				memmove(buf3, buf3+n, bufcnt);
				//buf3[bufcnt] = 0xFF;
			} else {
				bufcnt = 0;
			}
			*pbufind = bufcnt;
			return n;
		} else if (bufcnt >= IRC_BUFFER_SIZE) {
			ib_printf(_("[irc-%d] Received too long of a line from server!\n"),netno);
			config.ircnets[netno].ready=false;
			return -1;
		}
	}

	n = config.sockets->Select_Read(config.ircnets[netno].sock, 1000);
	if (n < 0) {
		ib_printf(_("[irc-%d] Error receiving data from server: %d (%s)\n"),netno,config.sockets->GetLastError(),config.sockets->GetLastErrorString());
		config.ircnets[netno].ready=false;
		return -1;
	} else if (n == 0) {
		//safe_sleep(50, true);
	} else {
		n = config.sockets->Recv(config.ircnets[netno].sock, &buf3[bufcnt], IRC_BUFFER_SIZE-bufcnt);
		if (n < 0) {
			ib_printf(_("[irc-%d] Error receiving data from server: %d (%s)\n"),netno,config.sockets->GetLastError(),config.sockets->GetLastErrorString());
			config.ircnets[netno].ready=false;
			return -1;
		} else if (n == 0) {
			ib_printf(_("[irc-%d] Disconnected from server: %d (%s)\n"),netno,config.sockets->GetLastError(),config.sockets->GetLastErrorString());
			config.ircnets[netno].ready=false;
			return -1;
		} else {
			bufcnt += n;
			//buf3[bufcnt] = 0;
			*pbufind = bufcnt;
		}
	}

	return 0;
}

int parse_irc_line(char * buf, char ** hostmask, char ** from, char ** cmd, char ** parms, int maxparms) {
	*from=buf;
	if (buf[0] == ':') { *from = buf+1; }
	char *p = strchr(buf,' ');
	if (p) {
		p[0]=0;
		p++;
	} else { return -1; }
	if (hostmask != NULL) {
		*hostmask = zstrdup(*from);
	}
	char * q = strchr(buf,'!');
	if (q != NULL && q < p) {
		*q=0;
	}

	*cmd=p;

	memset(parms,0,sizeof(char *) * maxparms);
	int pcount=0;
	while ((p = strchr(p,' ')) != NULL && pcount < maxparms) {
		p[0]=0;
		p++;
		parms[pcount] = p;
		if (p[0] == ':') { parms[pcount]++; pcount++; break; }
		pcount++;
	}

	return pcount;
}

void apply_irc_modes(char * modes, size_t modeSize, const char * new_modes) {
	int curmode = 0;
	const char * p = new_modes;
	while (*p) {
		if (*p == '+') {
			curmode = 1;
		} else if (*p == '-') {
			curmode = 2;
		} else {
			if (curmode == 1) {
				if (strchr(modes, *p) == NULL) {
					modes[strlen(modes)+1] = 0;
					modes[strlen(modes)] = *p;
				}
			} else if (curmode == 2) {
				char * q = strchr(modes, *p);
				if (q) {
					memmove(q, q+1, strlen(q));
				}
			} else {
				ib_printf("Warning: new modes string has mode without +/- first! (%s)\n", new_modes);
			}
		}
		p++;
	}
}

int GetChannelModeIndex(char tmp) {
	if (tmp >= 'A' && tmp <= 'Z') {
		return tmp-'A'+26;
	}
	if (tmp >= 'a' && tmp <= 'z') {
		return tmp-'a';
	}
	return -1;
}

inline bool IsChUserPrefix(int netno, char tmp, uint8 * mode=NULL) {
	if (mode != NULL) { *mode = 0; }
	for (int i=0; i < (MAX_IRC_CHUMODES*2) && config.ircnets[netno].ch_umodes[i]; i += 2) {
		if (config.ircnets[netno].ch_umodes[i] == tmp) {
			tmp = config.ircnets[netno].ch_umodes[i+1];
			if (mode != NULL) {
				switch (tmp) {
					case 'q':
						*mode |= IAL_CHUFLAG_OWNER;
						break;
					case 'a':
						*mode |= IAL_CHUFLAG_ADMIN;
						break;
					case 'o':
						*mode |= IAL_CHUFLAG_OP;
						break;
					case 'h':
						*mode |= IAL_CHUFLAG_HALFOP;
						break;
					case 'v':
						*mode |= IAL_CHUFLAG_VOICE;
						break;
				}
			}
			return true;
		}
	}
	return false;
}

inline bool IsChUserMode(int netno, char tmp, uint8 * mode=NULL) {
	if (mode != NULL) { *mode = 0; }
	for (int i=1; i < (MAX_IRC_CHUMODES*2) && config.ircnets[netno].ch_umodes[i]; i += 2) {
		if (config.ircnets[netno].ch_umodes[i] == tmp) {
			if (mode != NULL) {
				switch (tmp) {
					case 'q':
						*mode |= IAL_CHUFLAG_OWNER;
						break;
					case 'a':
						*mode |= IAL_CHUFLAG_ADMIN;
						break;
					case 'o':
						*mode |= IAL_CHUFLAG_OP;
						break;
					case 'h':
						*mode |= IAL_CHUFLAG_HALFOP;
						break;
					case 'v':
						*mode |= IAL_CHUFLAG_VOICE;
						break;
				}
			}
			return true;
		}
	}
	return false;
}

/*
1 = Mode that adds or removes a nick or address to a list. Always has a parameter.
2 = Mode that changes a setting and always has a parameter.
3 = Mode that changes a setting and only has a parameter when set.
4 = Mode that changes a setting and never has a parameter.
*/
void apply_irc_channel_modes(int netno, USER_PRESENCE * mode_changer, CHANNELS * chan, char * parms[16], int nparms, int base) {
	IRCNETS * irc = &config.ircnets[netno];
	int curmode = 0;
	int curopt = base+1;
	const char * p = parms[base];
	ib_printf("[irc-%d] Mode Debug: Channel flags before parsing: %s\n", netno, chan->modes);
	while (*p) {
		if (*p == '+') {
			curmode = 1;
		} else if (*p == '-') {
			curmode = 2;
		} else {
			if (curmode == 1) {
			//	bool done = false;
				bool addtostr = true;
				uint8 mode;
				int ind = GetChannelModeIndex(*p);
				if (*p != -1) {
					switch (irc->chmode_handling[ind]) {
						case 1:
							ib_printf("[irc-%d] Mode Debug: Got mode type 1 +%c with parm: %s\n", netno, *p, parms[curopt]);
							if (*p == 'b') {
								ib_printf("[irc-%d] Ban added in %s for '%s' by %s\n", netno, chan->channel.c_str(), parms[curopt], mode_changer->Nick);
								IBM_ON_BAN ibm;
								memset(&ibm, 0, sizeof(ibm));
								ibm.banner = mode_changer;
								ibm.banmask = parms[curopt];
								SendMessage(-1, IB_ONBAN, (char *)&ibm, sizeof(ibm));
							}
							addtostr = false;
							curopt++;
							break;
						case 2:
							if (IsChUserMode(netno, *p, &mode)) {
								if (curopt < nparms) {
									IAL_CHANNEL * chan2 = irc->ial->GetChan(chan->channel.c_str());
									if (chan2) {
										IAL_CHANNEL_NICKS * nick = irc->ial->GetNickChanEntry(chan2, parms[curopt]);
										if (nick) {
											nick->chumode |= mode;
											ib_printf("[irc-%d] Mode Debug: nick %s new modes: %02X\n", netno, nick->Nick->nick.c_str(), nick->chumode);
										}
									}
								}
								addtostr = false;
							}
							ib_printf("[irc-%d] Mode Debug: got mode type 2 +%c (iscu: %d) with parm: %s\n", netno, *p, IsChUserMode(netno, *p), parms[curopt]);
							curopt++;
							break;
						case 3:
							ib_printf("[irc-%d] Mode Debug: Got mode type 3 +%c with parm: %s\n", netno, *p, parms[curopt]);
							curopt++;
							break;
						case 4:
							ib_printf("[irc-%d] Mode Debug: Got mode type 4 +%c\n", netno, *p);
							break;
					}
				}
				if (addtostr && strchr(chan->modes, *p) == NULL) {
					chan->modes[strlen(chan->modes)+1] = 0;
					chan->modes[strlen(chan->modes)] = *p;
				}
			} else if (curmode == 2) {
				int ind = GetChannelModeIndex(*p);
				uint8 mode;
				if (*p != -1) {
					switch (irc->chmode_handling[ind]) {
						case 1:
							ib_printf("[irc-%d] Mode Debug: Got mode type 1 -%c with parm: %s\n", netno, *p, parms[curopt]);
							if (*p == 'b') {
								ib_printf("[irc-%d] Ban removed in %s for '%s' by %s\n", netno, chan->channel.c_str(), parms[curopt], mode_changer->Nick);
								IBM_ON_BAN ibm;
								memset(&ibm, 0, sizeof(ibm));
								ibm.banner = mode_changer;
								ibm.banmask = parms[curopt];
								SendMessage(-1, IB_ONUNBAN, (char *)&ibm, sizeof(ibm));
							}
							curopt++;
							break;
						case 2:
							if (IsChUserMode(netno, *p, &mode)) {
								if (curopt < nparms) {
									IAL_CHANNEL * chan2 = irc->ial->GetChan(chan->channel.c_str());
									if (chan2) {
										IAL_CHANNEL_NICKS * nick = irc->ial->GetNickChanEntry(chan2, parms[curopt]);
										if (nick) {
											nick->chumode &= ~mode;
											ib_printf("Nick %s new modes: %02X\n", nick->Nick->nick.c_str(), nick->chumode);
										}
									}
								}
							}
							ib_printf("[irc-%d] Mode Debug: Got mode type 2 -%c (iscu: %d) with parm: %s\n", netno, *p, IsChUserMode(netno, *p), parms[curopt]);
							curopt++;
							break;
						case 3:
							ib_printf("[irc-%d] Mode Debug: Got mode type 3 -%c\n", netno, *p);
							break;
						case 4:
							ib_printf("[irc-%d] Mode Debug: Got mode type 4 -%c\n", netno, *p);
							break;
					}
				}

				char * q = strchr(chan->modes, *p);
				if (q) {
					memmove(q, q+1, strlen(q));
				}
			} else {
				ib_printf("Warning: new channel modes string has mode without +/- first! (%s)\n", parms[base]);
			}
		}
		p++;
	}
	ib_printf("[irc-%d] Mode Debug: Channel flags after parsing: %s\n", netno, chan->modes);
}

/*
	Strip bold, color, plain, reverse, and underline control codes from a string.
	Remove mIRC color codes, and hex color codes.
*/
char * strip_control_codes(unsigned char *buf) {
	char * orig_str = (char *)buf;
    int ind = 0, nc = 0, len = strlen((const char *)buf), last_len=0;
    bool in_col = false, in_rgb = false;
	unsigned char *last_buf=NULL;

    unsigned char * new_str = (unsigned char *)zmalloc(strlen((const char *)buf)+1);/// the string will only get shorter

    while (len > 0) {
		if ((in_rgb && isxdigit(*buf) && nc < 6) || (in_rgb && *buf == ',' && nc < 7)) {
			/*
				Syntax for RGB is ^DHHHHHH where H is a hex digit.
				If < 6 hex digits are specified, the code is displayed as text
			*/
			nc++;
			if (*buf == ',') {
				nc = 0;
			}
		} else if (in_col && ((isdigit(*buf) && nc < 2) || (*buf == ',' && nc < 3))) {
			nc++;
			if (*buf == ',') {
				nc = 0;
			}
        } else {
				if (in_col) {
					in_col = false;
				}
                if (in_rgb) {
					if (nc != 6) {
						buf = last_buf+1;
						len = last_len-1;
						in_rgb = false;
						continue;
					}
					in_rgb = false;
                }
				switch (*buf) {
					case 3:
						/* color */
						in_col = true;
						nc = 0;
						break;
					case 4:
						/* RGB */
						last_buf = buf;
						last_len = len;
						in_rgb = true;
						nc = 0;
						break;
					case 2: /// bold
					case 15: /// plain
					case 22: /// reverse
					case 31: /// underline
						break;
					default:
						new_str[ind] = *buf;
						ind++;
						break;
				}
		}
		buf++;
		len--;
	}
	new_str[ind] = 0;
	strcpy(orig_str, (char *)new_str);
	zfree(new_str);
	return (char *)buf;
}

static const char irc_desc[] = "IRC";
Titus_Mutex ircMutex;
typedef std::vector<USER_PRESENCE *> presList;
presList ircPres;

#if !defined(DEBUG)
void irc_up_ref(USER_PRESENCE * u) {
#else
void irc_up_ref(USER_PRESENCE * u, const char * fn, int line) {
	ib_printf("IRC_UserPres_AddRef(): %s : %s:%d\n", u->User->Nick, fn, line);
#endif
	ircMutex.Lock();
	if (u->User) { RefUser(u->User); }
	u->ref_cnt++;
	ircMutex.Release();
}

#if !defined(DEBUG)
void irc_up_unref(USER_PRESENCE * u) {
#else
void irc_up_unref(USER_PRESENCE * u, const char * fn, int line) {
	ib_printf("IRC_UserPres_DelRef(): %s : %s:%d\n", u->Nick, fn, line);
#endif
	ircMutex.Lock();
	if (u->User) { UnrefUser(u->User); }
	u->ref_cnt--;
	#if defined(DEBUG)
	ib_printf("IRC_UserPres_DelRef(): %s : New Refcount %d\n", u->Nick,u->ref_cnt);
	#endif
	if (u->ref_cnt == 0) {
		for (presList::iterator x = ircPres.begin(); x != ircPres.end(); x++) {
			if (*x == u) {
				ircPres.erase(x);
				break;
			}
		}
		zfree((void *)u->Hostmask);
		if (u->Channel) { zfree((void *)u->Channel); }
		zfree((void *)u->Nick);
		zfree(u);
	}
	ircMutex.Release();
}

bool send_irc_pm(USER_PRESENCE * ut, const char * str) {
	if (str == NULL || *str == 0) { return true; }
	if (ut->NetNo < 0 || ut->NetNo > config.num_irc || ut->Sock != config.ircnets[ut->NetNo].sock || !config.ircnets[ut->NetNo].ready) { return false; }
	char buf[8192];
	snprintf(buf, sizeof(buf), "PRIVMSG %s :%s\r\n", ut->Nick, str);
	return bSend(ut->Sock, buf, strlen(buf), PRIORITY_INTERACTIVE) > 0 ? true:false;
}
bool send_irc_notice(USER_PRESENCE * ut, const char * str) {
	if (str == NULL || *str == 0) { return true; }
	if (ut->NetNo < 0 || ut->NetNo > config.num_irc || ut->Sock != config.ircnets[ut->NetNo].sock || !config.ircnets[ut->NetNo].ready) { return false; }
	char buf[8192];
	snprintf(buf, sizeof(buf), "NOTICE %s :%s\r\n", ut->Nick, str);
	return bSend(ut->Sock, buf, strlen(buf), PRIORITY_INTERACTIVE) > 0 ? true:false;
}
bool send_irc_chan_pm(USER_PRESENCE * ut, const char * str) {
	if (str == NULL || *str == 0) { return true; }
	if (ut->NetNo < 0 || ut->NetNo > config.num_irc || ut->Sock != config.ircnets[ut->NetNo].sock || !config.ircnets[ut->NetNo].ready) { return false; }
	char buf[8192];
	snprintf(buf, sizeof(buf), "PRIVMSG %s :%s\r\n", ut->Channel ? ut->Channel : ut->Nick, str);
	return bSend(ut->Sock, buf, strlen(buf), PRIORITY_INTERACTIVE) > 0 ? true:false;
}
bool send_irc_chan_notice(USER_PRESENCE * ut, const char * str) {
	if (str == NULL || *str == 0) { return true; }
	if (ut->NetNo < 0 || ut->NetNo > config.num_irc || ut->Sock != config.ircnets[ut->NetNo].sock || !config.ircnets[ut->NetNo].ready) { return false; }
	char buf[8192];
	snprintf(buf, sizeof(buf), "NOTICE %s :%s\r\n", ut->Channel ? ut->Channel : ut->Nick, str);
	return bSend(ut->Sock, buf, strlen(buf), PRIORITY_INTERACTIVE) > 0 ? true:false;
}

USER_PRESENCE * alloc_irc_presence(USER * user, T_SOCKET * sock, int netno, const char * nick, const char * hostmask, const char * channel) {
	uint32 uflags = user ? user->Flags : GetDefaultFlags();


	ircMutex.Lock();
	for (presList::const_iterator x = ircPres.begin(); x != ircPres.end(); x++) {
		if ((*x)->NetNo == netno && (*x)->Sock == sock && (*x)->User == user && (*x)->Flags == uflags && !stricmp((*x)->Hostmask, hostmask) && ((channel == NULL && (*x)->Channel == NULL) || ((*x)->Channel != NULL && channel != NULL && !stricmp((*x)->Channel, channel)))) {
			ircMutex.Release();
			RefUser(*x);
			if (user) {
				UpdateUserSeen(*x);
			}
			return *x;
		}
	}

	USER_PRESENCE * ret = znew(USER_PRESENCE);
	memset(ret, 0, sizeof(USER_PRESENCE));

	ret->User = user;
	ret->Sock = sock;
	ret->Nick = zstrdup(nick);
	ret->Hostmask = zstrdup(hostmask);
	ret->Flags = uflags;
	ret->NetNo = netno;
	ret->Channel = channel ? zstrdup(channel):NULL;

	ret->send_chan_notice = send_irc_chan_notice;
	ret->send_chan = send_irc_chan_pm;
	ret->send_msg = send_irc_pm;
	ret->send_notice = send_irc_notice;
	if (ret->Channel) {
		ret->std_reply = ret->send_notice;
	} else {
		ret->std_reply = ret->send_msg;
	}

	/*
#if defined(DEBUG)
	if (netno == 2) {//config.ircnets[netno].is_bitlbee
		ret->std_reply = send_irc_pm;
		ret->send_notice = send_irc_pm;
		ret->send_chan_notice = send_irc_chan_pm;
	}
#endif
	*/

	ret->Desc = irc_desc;

	ret->ref = irc_up_ref;
	ret->unref = irc_up_unref;

	ircPres.push_back(ret);
	RefUser(ret);
	ircMutex.Release();
	if (user) {
		UpdateUserSeen(ret);
	}
	return ret;
};

USER_PRESENCE * alloc_irc_presence(T_SOCKET * sock, int netno, const char * nick, const char * hostmask, const char * channel) {
	USER * user = GetUser(hostmask);
	USER_PRESENCE * ret = alloc_irc_presence(user, sock, netno, nick, hostmask, channel);
	if (user) { UnrefUser(user); }
	return ret;
}

void ProcParms(char * buf, unsigned int bufSize, char * parms) {
	StrTokenizer st(parms, ' ');
	//int num = st.NumTok();
	char *p = buf;

	bool in=false, in_first=false;
	char first[16], last[16];
	char * start=NULL;
	int len;

	while (*p) {
		if (in) {
			if (isdigit(*p)) {
				if (in_first) {
					strncat(first, p, 1);
				} else {
					strncat(last, p, 1);
				}
				len++;
			} else if (*p == '-' && in_first) {
				len++;
				in_first = false;
			} else {
				in = false;
				if (strlen(first)) {
					int f = atoi(first)+1;
					int l = f;
					if (strlen(last)) {
						l = atoi(last)+1;
						if (l < f) { l=f; }
					} else if (!in_first) {
						l = st.NumTok();
					}
					char buf2[32];
					memset(&buf2, 0, sizeof(buf2));
					strncpy(buf2, start, len);
					str_replaceA(start, bufSize, buf2, st.stdGetTok(f,l).c_str());
				}
			}
		} else if (*p == '$') {
			char * q = p + 1;
			if (*q == '$') {
				memmove(p, q, strlen(p));
			} else {
				in = true;
				in_first = true;
				start = p;
				len = 1;
				memset(first, 0, sizeof(first));
				memset(last, 0, sizeof(last));
			}
		}
		p++;
	}
}

CHANNELS * GetChannel(int netno, const char * chan) {
	if (netno >= 0 && netno < MAX_IRC_SERVERS) {
		for (int chans=0; chans < config.ircnets[netno].num_chans; chans++) {
			if (!stricmp(chan,config.ircnets[netno].channels[chans].channel.c_str())) {
				return &config.ircnets[netno].channels[chans];
			}
		}
	}
	return NULL;
}

#if defined(DEBUG)
void DumpCommands();
#endif

string irc_check_ip(int netno) {
	if (config.ircnets[netno].bindip.length()) {
		return config.ircnets[netno].bindip;
	} else if (config.base.bind_ip.length()) {
		return config.base.bind_ip;
	}
	return config.ircnets[netno].host;
}

bool irc_connect(int netno) {
	config.ircnets[netno].sock = config.sockets->Create(config.sockets->GetFamilyHint(irc_check_ip(netno).c_str(),config.ircnets[netno].port), SOCK_STREAM, IPPROTO_TCP);
	string bind_ip = config.ircnets[netno].bindip.length() ? config.ircnets[netno].bindip : config.base.bind_ip;
	if (bind_ip.length()) {
		if (config.sockets->BindToAddr(config.ircnets[netno].sock, bind_ip.c_str(), 0)) {
			ib_printf(_("[irc-%d] Bound to %s\n"), netno, bind_ip.c_str());
		} else {
			ib_printf(_("[irc-%d] Error binding to %s (%s)\n"), netno, bind_ip.c_str(), config.sockets->GetLastErrorString(config.ircnets[netno].sock));
		}
	}
	config.sockets->SetRecvTimeout(config.ircnets[netno].sock, 150000);
	config.sockets->SetSendTimeout(config.ircnets[netno].sock, 150000);
	if (config.sockets->ConnectWithTimeout(config.ircnets[netno].sock,config.ircnets[netno].host.c_str(),config.ircnets[netno].port,15000)) {
		return true;
	}
	return false;
}

bool irc_login(int netno, char * buf, char * recvbuf) {
	if (config.ircnets[netno].ssl == 1) {
		if (config.sockets->SwitchToSSL_Client(config.ircnets[netno].sock)) {
			ib_printf(_("[irc-%d] TLS connection established!\n"), netno);
		} else {
			ib_printf(_("[irc-%d] Error enabling TLS connection: %s\n"), netno, config.sockets->GetLastErrorString());
			return false;
		}
	} else if (config.ircnets[netno].ssl == 2) {
		bSend(config.ircnets[netno].sock, "STARTTLS\r\n", 0, PRIORITY_IMMEDIATE);

		config.ircnets[netno].last_recv = time(NULL);
		int bufind = 0;
		bool ssl_success = false;

		while (1) {

			int n = irc_read_line(netno, buf, recvbuf, &bufind);
			if (n == 0) {
				if ((time(NULL) - config.ircnets[netno].last_recv) > IRC_TIMEOUT) {
					ib_printf(_("[irc-%d] Error receiving data from server: timed out, no data in over 150 seconds.\n"),netno);
					config.ircnets[netno].ready=false;
					break;
				} else {
					continue;
				}
			}

			if (n < 0) { break; }
			strtrim(buf, "\r\n");
			if (buf[0] == 0) { continue; }

			if (config.ircnets[netno].log_fp) {
				char durbuf[32];
				fprintf(config.ircnets[netno].log_fp, "%s < %s\r\n", ircbot_cycles_ts(durbuf,sizeof(durbuf)), buf);
				fflush(config.ircnets[netno].log_fp);
			}

			if (!strnicmp(buf, ":ERROR ", 7) || !strnicmp(buf, "ERROR ", 6)) {
				ib_printf(_("[irc-%d] %s\n"),netno,buf);
				break;
			}

			char *cmd=NULL,*from=NULL,*parms[32];
			int nparms = parse_irc_line(buf, NULL, &from, &cmd, (char **)&parms, 32);
			if (nparms < 0) { continue; }

			if (!stricmp(cmd,"NOTICE") && (parms[0] != NULL && !stricmp(parms[0],"AUTH")) && parms[1] != NULL) {
				ib_printf(_("[irc-%d] %s\n"),netno,parms[1]);
			} else if (!stricmp(cmd, RPL_STARTTLS)) {
				ssl_success = config.sockets->SwitchToSSL_Client(config.ircnets[netno].sock);
				if (ssl_success) {
					ib_printf(_("[irc-%d] TLS connection established!\n"), netno);
				} else {
					ib_printf(_("[irc-%d] Error enabling TLS connection via STARTTLS: %s\n"), netno, config.sockets->GetLastErrorString(config.ircnets[netno].sock));
				}
				break;
			} else if (!stricmp(cmd, ERR_STARTTLS)) {
				ib_printf(_("[irc-%d] Error enabling TLS connection via STARTTLS: %s\n"), netno, parms[1]);
				break;
			} else if (!stricmp(cmd, ERR_UNKNOWNCOMMAND)) {
				if (!stricmp(parms[1], "STARTTLS")) {
					ib_printf(_("[irc-%d] This server does not support STARTTLS!\n"), netno);
					break;
				} else {
					ib_printf(_("[irc-%d] Unknown command: %s\n"),netno,parms[1]);
				}
			} else if (!stricmp(cmd, ERR_NOTREGISTERED)) {
				ib_printf(_("[irc-%d] This server does not support STARTTLS!\n"), netno);
				break;
			}
		}

		if (!ssl_success) {
			return false;
		}
	}

	if (config.ircnets[netno].enable_cap) {
		bSend(config.ircnets[netno].sock, "CAP LS\r\n", 0, PRIORITY_IMMEDIATE);
		bool cap_was_support = false;
		config.ircnets[netno].last_recv = time(NULL);
		int bufind = 0;
		while (1) {

			int n = irc_read_line(netno, buf, recvbuf, &bufind);
			if (n == 0) {
				if ((time(NULL) - config.ircnets[netno].last_recv) > IRC_TIMEOUT) {
					ib_printf(_("[irc-%d] Error receiving data from server: timed out, no data in over 150 seconds.\n"), netno);
					config.ircnets[netno].ready = false;
					break;
				} else {
					continue;
				}
			}

			if (n < 0) { break; }
			strtrim(buf, "\r\n");
			if (buf[0] == 0) { continue; }

			if (config.ircnets[netno].log_fp) {
				char durbuf[32];
				fprintf(config.ircnets[netno].log_fp, "%s < %s\r\n", ircbot_cycles_ts(durbuf, sizeof(durbuf)), buf);
				fflush(config.ircnets[netno].log_fp);
			}

			if (!strnicmp(buf, ":ERROR ", 7) || !strnicmp(buf, "ERROR ", 6)) {
				ib_printf(_("[irc-%d] %s\n"), netno, buf);
				break;
			}

			char *cmd = NULL, *from = NULL, *parms[32];
			int nparms = parse_irc_line(buf, NULL, &from, &cmd, (char **)&parms, 32);
			if (nparms < 0) { continue; }

			if (!stricmp(cmd, "NOTICE") && (parms[0] != NULL && !stricmp(parms[0], "AUTH")) && parms[1] != NULL) {
				ib_printf(_("[irc-%d] %s\n"), netno, parms[1]);
			} else if (!stricmp(cmd, "CAP")) {
				cap_was_support = true;
				char * list = NULL;
				if (nparms >= 3 && !stricmp(parms[0], "*") && !stricmp(parms[1], "LS")) {
					list = parms[2];
				} else if (nparms >= 2 && !stricmp(parms[0], "LS")) {
					list = parms[1];
				}
				if (list != NULL) {
					//ib_printf("cap list: %s\n", list);
					char * p2 = NULL;
					char * cap = strtok_r(list, " ", &p2);
					while (cap != NULL) {
						if (!stricmp(cap, "twitch.tv/membership")) {
							stringstream sstr;
							sstr << "CAP REQ :" << cap << "\r\n";
							bSend(config.ircnets[netno].sock, sstr.str().c_str(), 0, PRIORITY_IMMEDIATE);
						}
						cap = strtok_r(NULL, " ", &p2);
					}
				}
				break;
			} else if (!stricmp(cmd, ERR_UNKNOWNCOMMAND)) {
				if (!stricmp(parms[0], "CAP") || !stricmp(parms[1], "CAP")) {
					ib_printf(_("[irc-%d] This server does not support CAP, skipping negotiation...\n"), netno);
					config.ircnets[netno].enable_cap = false;
					break;
				} else {
					ib_printf(_("[irc-%d] Unknown command: %s\n"), netno, parms[1]);
				}
			} else if (!stricmp(cmd, ERR_NOTREGISTERED)) {
				if (!stricmp(parms[0], "CAP")) {
					ib_printf(_("[irc-%d] This server does not support CAP, skipping negotiation...\n"), netno);
					config.ircnets[netno].enable_cap = false;
				} else {
					ib_printf(_("[irc-%d] %s\n"), netno, parms[1]);
				}
				break;
			} else if (!stricmp(from,"PING")) {
				ib_printf(_("[irc-%d] Ping? Pong!\n"),netno);
				stringstream sstr;
				sstr << "PONG :" << parms[0] << "\r\n";
				bSend(config.ircnets[netno].sock, sstr.str().c_str(), 0, PRIORITY_IMMEDIATE);
				continue;
			}
		}
		if (cap_was_support) {
			bSend(config.ircnets[netno].sock, "CAP END\r\n", 0, PRIORITY_IMMEDIATE);
		}
	}
	if (config.ircnets[netno].pass.length()) {
		sprintf(buf, "PASS %s\r\n", config.ircnets[netno].pass.c_str());
		bSend(config.ircnets[netno].sock,buf,strlen(buf), PRIORITY_IMMEDIATE);
	}
	sprintf(buf, "NICK %s\r\n", config.ircnets[netno].base_nick.c_str());
	config.ircnets[netno].cur_nick  = config.ircnets[netno].base_nick;
	bSend(config.ircnets[netno].sock,buf,strlen(buf), PRIORITY_IMMEDIATE);
	stringstream name;
	if (config.ircnets[netno].custIdent.length() == 0) {
		name << "ShoutIRC Bot ";
		if (GetBotVersion() & IRCBOT_VERSION_FLAG_LITE) {
			name << "Lite ";
		}
		name << GetBotVersionString();
		if (!config.ircnets[netno].shortIdent) {
			name << " - www.shoutirc.com";
		}
	} else {
		name << config.ircnets[netno].custIdent;
	}
	sprintf(buf,"USER ircbot 8 * :%s\r\n\0Editing the USER string is strictly forbidden by the RadioBot EULA",name.str().c_str());
	//ib_printf("USER: %s", buf);
	bSend(config.ircnets[netno].sock, buf, strlen(buf), PRIORITY_IMMEDIATE);
	//int n = 0;

	//config.sockets->SetNonBlocking(config.ircnets[netno].sock,true);
	config.ircnets[netno].last_recv = time(NULL);
	return true;
}

void JoinChannel(int netno, const char * chan, const char * key) {
	CHANNELS * c = GetChannel(netno, chan);
	if (c) {
		JoinChannel(netno, c);
	} else {
		char * buf = NULL;
		if (key && key[0]) {
			buf = zmprintf("JOIN %s %s\r\n", chan, key);
		} else {
			buf = zmprintf("JOIN %s\r\n", chan);
		}
		bSend(config.ircnets[netno].sock, buf, 0, PRIORITY_IMMEDIATE);
		zfree(buf);
	}
}

void JoinChannel(int netno, CHANNELS * c) {
	if (config.ircnets[netno].ready) {
		char buf[512];
		if (c->altJoinCommand.length()) {
			snprintf(buf, sizeof(buf), "%s\r\n", c->altJoinCommand.c_str());
			str_replace(buf, sizeof(buf), "%chan", c->channel.c_str());
			str_replace(buf, sizeof(buf), "%key", c->key.c_str());
		} else if (c->key.length()) {
			snprintf(buf, sizeof(buf), "JOIN %s %s\r\n", c->channel.c_str(), c->key.c_str());
		} else {
			snprintf(buf, sizeof(buf), "JOIN %s\r\n", c->channel.c_str());
		}
		bSend(config.ircnets[netno].sock, buf, 0, PRIORITY_IMMEDIATE);
	}
}

THREADTYPE ircThread(VOID *lpData) {
	TT_THREAD_START
	int netno = voidptr2int(tt->parm);

	safe_sleep(netno*5);

	char buf[IRC_BUFFER_SIZE],buf2[IRC_BUFFER_SIZE],buf3[IRC_BUFFER_SIZE];
	char recvbuf[IRC_BUFFER_SIZE];
	//ib_printf(_("IRC Thread Started: %d\n"),netno);
	int tries = 1;

	while (!config.shutdown_now) {

		IAL ial;
		ial.Clear();

		ib_printf(_("[irc-%d] Connecting to: %s:%d\n"),netno,config.ircnets[netno].host.c_str(),config.ircnets[netno].port);
		config.ircnets[netno].ready = false;
		config.ircnets[netno].readyTime = 0;
		config.ircnets[netno].ial = &ial;
		config.ircnets[netno].LoadChanModes();
		config.ircnets[netno].LoadChUserModes();
		config.ircnets[netno].UpdateChanModesFromUModes();

		for (int chans=0; chans < config.ircnets[netno].num_chans; chans++) {
			config.ircnets[netno].channels[chans].joined = false;
			config.ircnets[netno].channels[chans].topic.clear();
			config.ircnets[netno].channels[chans].modes[0] = 0;
		}

		//strstr(config.ircnets[netno].host,":") != NULL ? PF_INET6:PF_INET
		config.ircnets[netno].log_fp = NULL;
		if (config.ircnets[netno].log) {
			sprintf(buf, "./logs/irc-debug-%02d.log", netno);
			if (access(buf, 0) == 0) {
				backup_file(buf);
			}
			config.ircnets[netno].log_fp = fopen(buf, "wb");
		}

		tries++;
		if (irc_connect(netno)) {
			ib_printf(_("[irc-%d] Connected to %s:%d (via IPv%c)\n"),netno,config.ircnets[netno].host.c_str(),config.ircnets[netno].port, config.ircnets[netno].sock->family == PF_INET ? '4':'6');
			if (!irc_login(netno, buf, recvbuf)) {
				break;
			}

			char * hostmask = NULL;
			int bufind = 0;
			while (1) {
				int n = irc_read_line(netno, buf, recvbuf, &bufind);
				if (n == 0) {
					if ((time(NULL) - config.ircnets[netno].last_recv) > IRC_TIMEOUT) {
						ib_printf(_("[irc-%d] Error receiving data from server: timed out, no data in over 150 seconds.\n"),netno);
						config.ircnets[netno].ready=false;
						break;
					} else {
						continue;
					}
				}
				if (n < 0) { break; }
				strtrim(buf, "\r\n");
				if (buf[0] == 0) { continue; }

				config.ircnets[netno].last_recv = time(NULL);

				#if defined(DEBUG)
				//ib_printf("[irc-%d] recvline2: %d -> %s\n", netno, n, buf);
				#endif

				if (config.ircnets[netno].log_fp) {
					char durbuf[32];
					fprintf(config.ircnets[netno].log_fp, "%s < %s\r\n", ircbot_cycles_ts(durbuf,sizeof(durbuf)), buf);
					fflush(config.ircnets[netno].log_fp);
				}

				//memcpy(&buf2,&buf,sizeof(buf));
				//sstrcpy(buf2, buf);
				bool dobreak=false;
				for (int pln=0; pln < MAX_PLUGINS; pln++) {
					if (config.plugins[pln].hHandle && config.plugins[pln].plug) {
						if (config.plugins[pln].plug->raw) {
							if (config.plugins[pln].plug->raw(netno,buf)) { dobreak=true; break; }
						}
					}
				}
				/*
				for (int pln=0; pln < NumPlugins5(); pln++) {
					IRCBotPlugin * Scan = GetPlugin5(pln);
					if (Scan) {
						if (Scan->OnRaw(netno,buf)) { dobreak=true; break; }
					}
				}
				*/
				if (dobreak) { continue; }

				if (!strnicmp(buf, ":ERROR ", 7) || !strnicmp(buf, "ERROR ", 6)) {
					ib_printf(_("[irc-%d] %s\n"),netno,buf);
					config.ircnets[netno].ready = false;
					break;
				}

				char *cmd=NULL,*from=NULL,*parms[32];
				if (hostmask) { zfree(hostmask); hostmask=NULL; }
				//ib_printf("InputRaw -> %s\n",buf);
				int nparms = parse_irc_line(buf,&hostmask,&from,&cmd,(char **)&parms,32);
				//ib_printf("InputPar -> %d | %s | %s | %s | %s | %s\n",nparms,from,cmd,parms[0],parms[1],parms[2]);

				if (!stricmp(from,"PING")) {
					ib_printf(_("[irc-%d] Ping? Pong!\n"),netno);
					sprintf(buf,"PONG %s\r\n",cmd);
					bSend(config.ircnets[netno].sock,buf,strlen(buf), PRIORITY_IMMEDIATE);
					continue;
				}

				if (!strcmp(cmd,"001")) { // rizon patch
					ib_printf(_("[irc-%d] %s\n"),netno,parms[1]);
					config.ircnets[netno].server = from;
					//if (config.ircnets[netno].liberalLoginDetect && !config.ircnets[netno].ready) { on_irc_login(netno); }
					continue;
				}

				if (!strcmp(cmd,"002") || !strcmp(cmd,"003") || !strcmp(cmd,"008")) {
					ib_printf(_("[irc-%d] %s\n"),netno,parms[1]);
					//if (config.ircnets[netno].liberalLoginDetect && !config.ircnets[netno].ready) { on_irc_login(netno); }
					continue;
				}

				if (!strcmp(cmd,"004") && nparms >= 2) {
					config.ircnets[netno].server = parms[1];
					if (nparms >= 3 && stristr(parms[2], "Unreal3.2")) {
						sprintf(buf,"MODE %s +B\r\n",config.ircnets[netno].cur_nick.c_str());
						bSend(config.ircnets[netno].sock,buf,strlen(buf), PRIORITY_INTERACTIVE);
					}
					continue;
				}

				if (!strcmp(cmd,"005")) {
					for (int i=1; i < (nparms - 1); i++) {
						//ib_printf("[irc-%d] %s\n",netno,parms[i]);
						StrTokenizer st(parms[i], '=', true);
						char * var = st.GetSingleTok(1);
						char * val = st.GetTok(2, st.NumTok());
						if (!stricmp(var,"NETWORK")) {
							config.ircnets[netno].network = val;
						} else if (!stricmp(var,"CHANMODES")) {
							config.ircnets[netno].LoadChanModes(val, false);
						} else if (!stricmp(var,"CHANTYPES")) {
							sstrcpy(config.ircnets[netno].channel_types, val);
						} else if (!stricmp(var,"PREFIX")) {
							config.ircnets[netno].LoadChUserModes(val);
							config.ircnets[netno].UpdateChanModesFromUModes();
						} else if (!stricmp(var,"NAMESX")) {
							bSend(config.ircnets[netno].sock, "PROTOCTL NAMESX\r\n", 0, PRIORITY_IMMEDIATE);
						}
						st.FreeString(var);
						st.FreeString(val);
					}
					continue;
				}

				if (!strcmp(cmd,"042")) {
					ib_printf(_("[irc-%d] %s: %s\n"),netno,parms[2],parms[1]);
				}

				if (!strcmp(cmd,"251") || !strcmp(cmd,"255") || !strcmp(cmd,"265") || !strcmp(cmd,"266")) {
					ib_printf(_("[irc-%d] %s\n"),netno,parms[1]);
					continue;
				}

				if (!strcmp(cmd,"252") || !strcmp(cmd,"253") || !strcmp(cmd,"254")) {
					ib_printf(_("[irc-%d] %s %s\n"),netno,parms[1],parms[2]);
					continue;
				}

				if (!strcmp(cmd,"302")) {
					if (config.base.bind_ip.length() == 0 && parms[1]) {
						char * p = stristr(parms[1], config.ircnets[netno].cur_nick.c_str());
						if (p) {
							p = strchr(p, '@');
							if (p) {
								p++;
								char * q = strchr(p, ' ');
								if (q) { *q = 0; }
								std::string tmp = config.sockets->GetHostIP(p, SOCK_STREAM, 0);
								if (tmp.length() > 0) {
									config.base.myip = tmp;
									ib_printf(_("[irc-%d] Updated local IP from userhost: %s -> %s\n"), netno, p, config.base.myip.c_str());
								} else {
									ib_printf(_("[irc-%d] Error resolving local hostname: %s\n"), netno, p);
								}
							}
						}
					}
					continue;
				}

				if (!strcmp(cmd,"324")) {
					// MODE reply from mode query on join
					CHANNELS * chan = GetChannel(netno, parms[1]);
					if (chan) {
						USER_PRESENCE * up = alloc_irc_presence(config.ircnets[netno].sock, netno, from, hostmask, config.ircnets[netno].IsChannelPrefix(parms[1][0]) ? parms[1]:NULL);
						apply_irc_channel_modes(netno, up, chan, parms, nparms, 2);
						UnrefUser(up);
					}
					continue;
				}

				if (!strcmp(cmd,"329")) {
					time_t x = atoi64(parms[2]);
					tm tme;
					localtime_r(&x, &tme);
					strftime(buf, sizeof(buf), "%X on %x", &tme);
					ib_printf(_("[irc-%d] Channel %s was created at %s\n"),netno,parms[1],buf);
					continue;
				}

				if (!strcmp(cmd,RPL_NOTOPIC)) {
					ib_printf(_("[irc-%d] No topic set in %s\n"),netno,parms[1]);
					CHANNELS * chan = GetChannel(netno, parms[1]);
					if (chan) {
						chan->topic.clear();
					}
					continue;
				}
				if (!strcmp(cmd,RPL_TOPIC)) {
					ib_printf(_("[irc-%d] Topic in %s is '%s'\n"),netno,parms[1],parms[2]);
					CHANNELS * chan = GetChannel(netno, parms[1]);
					if (chan) {
						chan->topic = parms[2];
					}
					continue;
				}
				if (!strcmp(cmd,RPL_TOPICTIME)) {
					time_t x = atoi64(parms[3]);
					tm tme;
					localtime_r(&x, &tme);
					strftime(buf, sizeof(buf), "%X on %x", &tme);
					ib_printf(_("[irc-%d] Topic in %s set by %s at %s\n"),netno,parms[1],parms[2],buf);
					continue;
				}

				if (!strcmp(cmd,RPL_WHOREPLY)) {
					IAL_CHANNEL * Chan = ial.GetChan(parms[1], true);
					if (Chan) {
						snprintf(buf2, sizeof(buf2), "%s!%s@%s", parms[5], parms[2], parms[3]);
						IAL_CHANNEL_NICKS * Nick = ial.GetNickChanEntry(Chan, parms[5]);
						if (Nick) {
							ial.hMutex.Lock();
							Nick->Nick->hostmask = buf2;
							ial.hMutex.Release();
							char *p = parms[6];
							while (*p) {
								uint8 mode=0;
								if (*p != 'H' && *p != '*' && *p != 'G' && *p != 'd') {
									if (IsChUserPrefix(netno, *p, &mode)) {
										Nick->chumode |= mode;
									}
								}
								p++;
							}
						}
					}
					continue;
				}
				if (!strcmp(cmd,RPL_ENDOFWHO)) {
					continue;
				}

				if (!strcmp(cmd,RPL_NAMEREPLY)) {
					if (!strcmp(parms[1],"=")) {
						IAL_CHANNEL * Chan = ial.GetChan(parms[2]);
						char * cnick2 = NULL;
						char * cnick = strtok_r(parms[3], " ", &cnick2);
						uint8 mode=0, mode2=0;
						while (cnick) {
							//char * nick = parms[i];
							char * tmp = cnick;
							//uint8 flags = 0;
							while (IsChUserPrefix(netno, *tmp, &mode2)) {
								mode |= mode2;
								tmp++;
							}
							//strtrim(cnick, "@+~&", TRIM_LEFT);
							IAL_CHANNEL_NICKS * n = ial.GetNickChanEntry(Chan, tmp);
							if (n) {
								n->chumode |= mode;
							}
							//ib_printf("[irc-%d] %s -> %s\n",netno,parms[2],cnick);
							cnick = strtok_r(NULL, " ", &cnick2);
						}
					}
					continue;
				}
				if (!strcmp(cmd,RPL_ENDOFNAMES)) {
					continue;
				}

				if (!strcmp(cmd,RPL_MOTDSTART)) {
					ib_printf(_("[irc-%d] Begin /MOTD\n"),netno);
					continue;
				}
				if (!strcmp(cmd,RPL_MOTD)) {
					ib_printf(_("[irc-%d] MOTD: %s\n"),netno, parms[1]);
					continue;
				}
				if (!strcmp(cmd,RPL_ENDOFMOTD)) {
					ib_printf(_("[irc-%d] End of /MOTD command.\n"),netno);
					// RPL_ENDOFMOTD is used later to detect when bot is connected, so don't continue here
				}

				if (!strcmp(cmd,"401") || !strcmp(cmd,"404")) { // 401 unknown command, 404 no colors
					ib_printf(_("[irc-%d] %s (%s)\n"),netno,parms[2],parms[1]);
					continue;
				}

				if (!strcmp(cmd,ERR_UNKNOWNCOMMAND)) { // unknown command
					if (!strncmp(parms[1],"IRCBOT_IDLE_",strlen("IRCBOT_IDLE_"))) {
						char *p = parms[1] + strlen("IRCBOT_IDLE_");
						double tme = ircbot_get_cycles() - atoi64(p);
						tme /= 1000;
						ib_printf(_("[irc-%d] Server Round Trip Time: %.02f secs\n"),netno,tme);
					} else {
						ib_printf(_("[irc-%d] Unknown command: %s\n"),netno,parms[1]);
					}
					continue;
				}

				if (!strcmp(cmd,ERR_NOMOTD)) { // MOTD File is missing
					ib_printf(_("[irc-%d] %s\n"),netno,parms[1]);
					// 422 is used later to detect when bot is connected, so don't continue here
				}

				//				Nick in use							Nick has bad chars					Nickname collision
				if (!strcmp(cmd,ERR_NICKNAMEINUSE) || !strcmp(cmd,ERR_ERRONEUSNICKNAME) || !strcmp(cmd,ERR_NICKCOLLISION)) {

					if (!config.ircnets[netno].ready) {
						std::stringstream sstr;
						sstr << config.ircnets[netno].base_nick << (rand()%10);
						sprintf(buf2, "NICK %s\r\n", sstr.str().c_str());
						bSend(config.ircnets[netno].sock,buf2, strlen(buf2), PRIORITY_IMMEDIATE);
						config.ircnets[netno].cur_nick = sstr.str();
					}

					ib_printf(_("[irc-%d] Nickname error (%s): %s\r\n"), netno, parms[1], parms[2]);

					if (!stricmp(config.ircnets[netno].base_nick.c_str(),parms[1]) && config.ircnets[netno].onnickinusecommand.length()) {
						sprintf(buf, "%s\r\n", config.ircnets[netno].onnickinusecommand.c_str());
						str_replace(buf, sizeof(buf), "%me", config.ircnets[netno].cur_nick.c_str());
						str_replace(buf, sizeof(buf), "%basenick", config.ircnets[netno].base_nick.c_str());
						ib_printf(_("[irc-%d] Sending OnNickInUse line: %s"), netno, buf);
						safe_sleep(1);
						bSend(config.ircnets[netno].sock,buf,strlen(buf), PRIORITY_IMMEDIATE);
					}
					continue;
				}

				if (!strcmp(cmd,ERR_BANNEDFROMCHAN) || !strcmp(cmd,ERR_CHANNELISFULL) || !strcmp(cmd,ERR_INVITEONLYCHAN) || !strcmp(cmd,ERR_BADCHANNELKEY)) {
					ib_printf(_("[irc-%d] Error joining %s: %s\n"), netno, parms[1], parms[2]);
					continue;
				}

				if (!strcmp(cmd,ERR_NEEDREGGEDNICK)) {
					ib_printf(_("[irc-%d] %s (%s)\n"), netno, parms[2], parms[1]);
					continue;
				}

				if (!strcmp(cmd,ERR_NOEXTMESSAGES)) {
					ib_printf(_("[irc-%d] %s\n"), netno, parms[1]);
					continue;
				}

				if (!strcmp(cmd,ERR_NOTONCHANNEL)) {
					ib_printf(_("[irc-%d] %s (%s)\n"), netno, parms[2], parms[1]);
					CHANNELS *c = GetChannel(netno, parms[1]);
					//mark the channel as not joined, in case we don't know already
					if (c) {
						c->joined = false;
					}
					continue;
				}

				if (!strcmp(cmd,ERR_NOTCHANOP)) {
					ib_printf(_("[irc-%d] Error (%s): %s\n"), netno, parms[1], parms[2]);
					continue;
				}

				if (!strcmp(cmd,"512")) { // chatnet auth
					if (config.ircnets[netno].on512.length()) {
						ib_printf(_("[irc-%d] Sending ChatNet Authentication...\n"), netno);
						sprintf(buf,"%s\r\n",config.ircnets[netno].on512.c_str());
						str_replaceA(buf, sizeof(buf), "%me", config.ircnets[netno].cur_nick.c_str());
						ProcText(buf, sizeof(buf));
						bSend(config.ircnets[netno].sock,buf,strlen(buf), PRIORITY_IMMEDIATE);
					}
					continue;
				}

				if (!strcmp(cmd,"972")) {
					ib_printf(_("[irc-%d] Error could not perform command %s: %s\n"), netno, parms[1], parms[2]);
					continue;
				}

				if (!stricmp(cmd,"NOTICE") && (parms[0] != NULL && !stricmp(parms[0],"AUTH")) && parms[1] != NULL) {
					ib_printf(_("[irc-%d] %s\n"),netno,parms[1]);
					continue;
				}

				if ((!strcmp(cmd,RPL_ENDOFMOTD) || !strcmp(cmd,ERR_NOMOTD) || !strcmp(cmd,"042")) && !config.ircnets[netno].ready) { // login to server successful
					ib_printf(_("[irc-%d] Login complete\n"),netno);

					if (config.ircnets[netno].onconnect.length()) {
						ib_printf(_("[irc-%d] Performing OnConnect's...\n"),netno);
						sprintf(buf,"%s\r\n",config.ircnets[netno].onconnect.c_str());
						str_replaceA(buf, sizeof(buf), "%me", config.ircnets[netno].cur_nick.c_str());
						ProcText(buf, sizeof(buf));
						bSend(config.ircnets[netno].sock,buf,strlen(buf), PRIORITY_IMMEDIATE);
					}

					sprintf(buf,"USERHOST %s\r\n",config.ircnets[netno].cur_nick.c_str());
					bSend(config.ircnets[netno].sock,buf,strlen(buf), PRIORITY_IMMEDIATE);
					/*
					if (config.base.bind_ip.length() == 0) {
						sprintf(buf,"USERHOST %s\r\n",config.ircnets[netno].cur_nick.c_str());
						bSend(config.ircnets[netno].sock,buf,strlen(buf), PRIORITY_IMMEDIATE);
					}
					*/

					ib_printf(_("[irc-%d] Joining channels...\n"),netno);
					if (config.base.log_chan.length() && netno == 0) {
						sprintf(buf,"JOIN %s\r\n",config.base.log_chan.c_str());
						bSend(config.ircnets[netno].sock,buf,strlen(buf), PRIORITY_IMMEDIATE);
					}
					config.ircnets[netno].readyTime = time(NULL);
					config.ircnets[netno].ready=true;
					for (int i=0; i<config.ircnets[netno].num_chans; i++) {
						JoinChannel(netno, &config.ircnets[netno].channels[i]);
					}
					tries=1;
					continue;
				}

				DRIFT_DIGITAL_SIGNATURE();

				if (!stricmp(cmd,"PRIVMSG") && parms[1] != NULL) { // a message
					USER_PRESENCE * up = alloc_irc_presence(config.ircnets[netno].sock, netno, from, hostmask, config.ircnets[netno].IsChannelPrefix(parms[0][0]) ? parms[0]:NULL);

					if (up->Flags & UFLAG_IGNORE) {
						UnrefUser(up);
						continue;
					}

					strip_control_codes((unsigned char *)parms[1]);

					if (config.ircnets[netno].removePreText.length()) {
						char * tmp = zstrdup((char *)config.ircnets[netno].removePreText.c_str());
						char * p2 = NULL;
						char * p = strtok_r(tmp, "|", &p2);
						while (p) {
							size_t len = strlen(p);
							if (!strnicmp(parms[1], p, len)) {
								//ib_printf("removePreText %s -> ", parms[1]);
								memmove(parms[1], parms[1] + len, strlen(parms[1] + len)+1);
								strtrim(parms[1], "\x09 ", TRIM_LEFT);
								//ib_printf("%s\n", parms[1]);
							}
							p = strtok_r(NULL, "|", &p2);
						}
						zfree(tmp);
					}

					if (config.ircnets[netno].IsChannelPrefix(parms[0][0])) {
						IAL_CHANNEL * Chan = ial.GetChan(parms[0]);
						//IAL_NICK * Nick =
						ial.GetNick(Chan,from,hostmask); // update/adds hostmask for user if not previously seen

						if (!stricmp(up->Nick, "Indy") && stristr(parms[1], "who's my little guy") != NULL) {
							up->send_chan(up, "I'm your little guy.");
						}

						IBM_USERTEXT ut;
						ut.userpres = up;
						ut.type = IBM_UTT_CHANNEL;
						ut.text = parms[1];
						if (SendMessage(-1,IB_ONTEXT,(char *)&ut,sizeof(ut))) {
							UnrefUser(up);
							continue;
						}

						char * p = (char *)FindMagicCommandPrefixes(parms[1]);

						if (IsCommandPrefix(parms[1][0]) || (p && p != parms[1])) {
							ib_printf(_("Possible Trigger: %s from %s\n"),parms[1],from);

							bool dobreak=false;
							for (int pln=0; pln < MAX_PLUGINS; pln++) {
								if (config.plugins[pln].hHandle && config.plugins[pln].plug) {
									if (config.plugins[pln].plug->on_text) {
										if (config.plugins[pln].plug->on_text(up,parms[1])) {
											dobreak=true;
											break;
										}
									}
								}
							}
							if (dobreak) {
								UnrefUser(up);
								continue;
							}

							char * com = parms[1];
							com++;
							if (!IsCommandPrefix(parms[1][0])) {
								com = p+2;
							}
							com = zstrdup(com);
							char * com_p = strchr(com, ' ');
							if (com_p) {
								com_p[0]=0;
								com_p++;
							}
							COMMAND * pCom = FindCommand(com);
							if (pCom && (pCom->mask & CM_ALLOW_IRC_PUBLIC)) {
								ib_printf(_("Match Command: %s\n"),pCom->command);
								if (pCom->proc_type == COMMAND_PROC) {
									ExecuteProc(pCom, com_p, up, CM_ALLOW_IRC_PUBLIC);
								} else if (command_is_allowed(pCom, up->Flags)) {
									sprintf(buf3,"%s\r\n",pCom->action);
									ProcText(buf3,sizeof(buf3));
									str_replaceA(buf3,sizeof(buf3),"%nick",from);
									str_replaceA(buf3,sizeof(buf3),"%chan",parms[0]);
									str_replaceA(buf3, sizeof(buf3),"%me",config.ircnets[netno].cur_nick.c_str());
									ProcParms(buf3, sizeof(buf3), parms[1]);
									bSend(config.ircnets[netno].sock,buf3,strlen(buf3), PRIORITY_INTERACTIVE);
								}
							}
							zfree(com);
						}
					} else {

						IBM_USERTEXT ut;
						ut.userpres = up;
						ut.type = IBM_UTT_PRIVMSG;
						ut.text = parms[1];
						if (SendMessage(-1,IB_ONTEXT,(char *)&ut,sizeof(ut))) {
							UnrefUser(up);
							continue;
						}

						uflag_to_string(up->Flags, buf3, sizeof(buf3));
						ib_printf(_("[irc-%d] [%s][PM] <%s> %s\n"),netno,buf3,from,parms[1]);
						if (config.base.log_chan.length() && config.ircnets[0].ready) {
							sprintf(buf2,"PRIVMSG %s :[IRC-%d][PM] <%s[%s]> %s\n",config.base.log_chan.c_str(),netno,from,buf3,parms[1]);
							bSend(config.ircnets[0].sock,buf2,strlen(buf2), PRIORITY_LOWEST);
						}

						if (!stricmp(parms[1],"\001VERSION\001")) {
							sprintf(buf2,"NOTICE %s :\001VERSION RadioBot %s%s/%s (C) 2003+ Drift Solutions [Build #" BUILD_STRING "] [Built on %s at %s]\001\r\n\0Editing the VERSION string is strictly forbidden by the RadioBot EULA",from,(GetBotVersion() & IRCBOT_VERSION_FLAG_LITE) ? "Lite ":"",GetBotVersionString(),PLATFORM,__DATE__,__TIME__);
							bSend(config.ircnets[netno].sock,buf2,strlen(buf2), PRIORITY_INTERACTIVE);
						}
						if (!stricmp(parms[1],"\001TIME\001")) {
							sprintf(buf2,"NOTICE %s :\001TIME It's Time To Tune In!\001\r\n",from);
							bSend(config.ircnets[netno].sock,buf2,strlen(buf2), PRIORITY_INTERACTIVE);
						}
						if (!strnicmp(parms[1],"\001PING ",6)) {
							sprintf(buf2,"NOTICE %s :%s\r\n",from,parms[1]);
							bSend(config.ircnets[netno].sock,buf2,strlen(buf2), PRIORITY_INTERACTIVE);
						}
						if (!stricmp(parms[1],"\001USERINFO\001")) {
							sprintf(buf2,"NOTICE %s :\001USERINFO I fight for the users!\001\r\n",from);
							bSend(config.ircnets[netno].sock,buf2,strlen(buf2), PRIORITY_INTERACTIVE);
						}
						if (!stricmp(parms[1],"\001FINGER\001")) {
							sprintf(buf2,"NOTICE %s :\001FINGER All work and no play makes %s a dull boy.\001\r\n",from,config.ircnets[0].cur_nick.c_str());
							bSend(config.ircnets[netno].sock,buf2,strlen(buf2), PRIORITY_INTERACTIVE);
						}
						if (!stricmp(parms[1],"\001XYZZY\001")) {
							sprintf(buf2,"NOTICE %s :\001XYZZY Nothing happens.\001\r\n",from);
							bSend(config.ircnets[netno].sock,buf2,strlen(buf2), PRIORITY_INTERACTIVE);
						}


						bool dobreak = false;
						for (int pln=0; pln < MAX_PLUGINS; pln++) {
							if (config.plugins[pln].hHandle && config.plugins[pln].plug) {
								if (config.plugins[pln].plug->on_text) {
									if (config.plugins[pln].plug->on_text(up,parms[1])) {
										dobreak = true;
										break;
									}
								}
							}
						}
						if (dobreak) {
							UnrefUser(up);
							continue;
						}

	#if defined(DEBUG)
						/*
						if (!stricmp(parms[1], "!memdump")) {
							sprintf(buf2, "PRIVMSG %s :Get ready for a surprise...\r\n", from);
							bSend(config.ircnets[netno].sock,buf2,strlen(buf2), PRIORITY_IMMEDIATE);
							MemLeakExit("memdump.txt");
							UnrefUser(up);//lol, don't know why I bother. This process is going to crash and burn
							continue;
						}
						*/
						if (!stricmp(parms[1], "!commanddump")) {
							sprintf(buf2, "PRIVMSG %s :Get ready for a surprise...\r\n", from);
							bSend(config.ircnets[netno].sock,buf2,strlen(buf2), PRIORITY_IMMEDIATE);
							DumpCommands();
							UnrefUser(up);
							continue;
						}
	#endif
						if (!stricmp(parms[1],"!ial-dump") && (up->Flags & UFLAG_MASTER)) {
							sprintf(buf2, "PRIVMSG %s :Dumping IAL...\r\n", from);
							bSend(config.ircnets[netno].sock,buf2,strlen(buf2));
							ial.Dump();
							UnrefUser(up);
							continue;
						}


						char * com = parms[1];
						if (IsCommandPrefix(parms[1][0])) {
							com++;
						}
						com = zstrdup(com);
						char * com_p = strchr(com, ' ');
						if (com_p) {
							com_p[0]=0;
							com_p++;
						}
						COMMAND * pCom = FindCommand(com);
						if (!pCom) {
							const char * p = FindMagicCommandPrefixes(parms[1]);
							if (p) {
								zfree(com);
								com = zstrdup(p+2);
								com_p = strchr(com, ' ');
								if (com_p) {
									com_p[0]=0;
									com_p++;
								}
								pCom = FindCommand(com);
							}
						}
						if (pCom && (pCom->mask & CM_ALLOW_IRC_PRIVATE)) {
							ib_printf(_("Match Command: %s\n"),pCom->command);
							if (pCom->proc_type == COMMAND_PROC) {
								ExecuteProc(pCom, com_p, up, CM_ALLOW_IRC_PRIVATE);
							} else if (command_is_allowed(pCom, up->Flags)) {
								sprintf(buf3,"%s\r\n",pCom->action);
								ProcText(buf3,sizeof(buf3));
								str_replaceA(buf3,sizeof(buf3),"%nick",from);
								str_replaceA(buf3, sizeof(buf3), "%me", config.ircnets[netno].cur_nick.c_str());
								ProcParms(buf3, sizeof(buf3), parms[1]);
								bSend(config.ircnets[netno].sock,buf3,strlen(buf3), PRIORITY_INTERACTIVE);
							}
						}
						zfree(com);

					}

					UnrefUser(up);
					continue;
				}

				if (!stricmp(cmd,"NOTICE") && parms[1] != NULL) { // a message
					USER_PRESENCE * up = alloc_irc_presence(config.ircnets[netno].sock, netno, from, hostmask, config.ircnets[netno].IsChannelPrefix(parms[0][0]) ? parms[0]:NULL);

					strip_control_codes((unsigned char *)parms[1]);

					bool dobreak = false;
					for (int pln=0; pln < MAX_PLUGINS; pln++) {
						if (config.plugins[pln].hHandle && config.plugins[pln].plug) {
							if (config.plugins[pln].plug->on_notice) {
								if (config.plugins[pln].plug->on_notice(up,parms[1])) {
									dobreak = true;
									break;
								}
							}
						}
					}
					if (dobreak) {
						UnrefUser(up);
						continue;
					}

					IBM_USERTEXT ut;
					ut.userpres = up;
					ut.text = parms[1];

					uflag_to_string(up->Flags, buf3, sizeof(buf3));
					if (config.ircnets[netno].IsChannelPrefix(parms[0][0])) {
						ib_printf(_("[irc-%d] [%s][%s] -%s- %s\n"),netno,buf3,parms[0],from,parms[1]);
						IAL_CHANNEL * Chan = ial.GetChan(parms[0]);
						ial.GetNick(Chan, from, hostmask);
						ut.type = IBM_UTT_CHANNEL;
						SendMessage(-1,IB_ONNOTICE,(char *)&ut,sizeof(ut));
					} else {
						ib_printf(_("[irc-%d] [%s][PM] -%s- %s\n"),netno,buf3,from,parms[1]);
						if (config.base.log_chan.length() && config.ircnets[0].ready) {
							sprintf(buf2,"PRIVMSG %s :[IRC-%d][PM] -%s[%s]- %s\n",config.base.log_chan.c_str(),netno,from,buf3,parms[1]);
							bSend(config.ircnets[0].sock,buf2,strlen(buf2), PRIORITY_LOWEST);
						}
						ut.type = IBM_UTT_PRIVMSG;
						SendMessage(-1,IB_ONNOTICE,(char *)&ut,sizeof(ut));
					}
					UnrefUser(up);
					continue;
				}

				if (!stricmp(cmd,"JOIN")) {

					IAL_CHANNEL * Chan = ial.GetChan(parms[0]);
					ial.GetNick(Chan, from, hostmask);

					if (stricmp(from,config.ircnets[netno].cur_nick.c_str())) {
						CHANNELS * chan = GetChannel(netno, parms[0]);
						if (chan) {
							if (chan->autoVoice) {
								snprintf(buf2, sizeof(buf2), "MODE %s +v %s\r\n", parms[0], from);
								bSend(config.ircnets[netno].sock,buf2,strlen(buf2), PRIORITY_INTERACTIVE);
							}
							if (config.doonjoin != false) {
								if (chan && chan->doonjoin) {
									if (LoadMessageChannel("OnJoin", netno, parms[0], buf2, sizeof(buf2))) {
										ProcText(buf2,sizeof(buf2));
										str_replaceA(buf2, sizeof(buf2),"%me",config.ircnets[netno].cur_nick.c_str());
										str_replaceA(buf2,sizeof(buf2),"%nick",from);
										str_replaceA(buf2,sizeof(buf2),"%chan",chan->channel.c_str());
										strcat(buf2, "\r\n");
										bSend(config.ircnets[netno].sock,buf2,strlen(buf2), PRIORITY_INTERACTIVE);
									} else {
										for (int i=0; i < 16; i++) {
											sprintf(buf3, "OnJoin%d", i);
											if (LoadMessageChannel(buf3, netno, parms[0], buf2, sizeof(buf2))) {
												ProcText(buf2,sizeof(buf2));
												str_replaceA(buf2, sizeof(buf2), "%me", config.ircnets[netno].cur_nick.c_str());
												str_replaceA(buf2,sizeof(buf2),"%nick",from);
												str_replaceA(buf2,sizeof(buf2),"%chan",chan->channel.c_str());
												strcat(buf2, "\r\n");
												bSend(config.ircnets[netno].sock,buf2,strlen(buf2), PRIORITY_INTERACTIVE);
											} else {
												break;
											}
										}
									}
								}
							}
						}
					} else {
						CHANNELS * chan = GetChannel(netno, parms[0]);
						if (chan) {
							chan->modes[0] = 0;
							chan->joined = true;
						}
						sprintf(buf2, "MODE %s\r\n", parms[0]);
						bSend(config.ircnets[netno].sock,buf2,strlen(buf2), PRIORITY_IMMEDIATE);
						sprintf(buf2, "WHO %s\r\n", parms[0]);
						bSend(config.ircnets[netno].sock,buf2,strlen(buf2), PRIORITY_IMMEDIATE);
					}

					USER_PRESENCE * up = alloc_irc_presence(config.ircnets[netno].sock, netno, from, hostmask, parms[0]);
					SendMessage(-1,IB_ONJOIN,(char *)up,sizeof(USER_PRESENCE));
					UnrefUser(up);
					continue;
				}

				if (!stricmp(cmd,"PART")) {
					if (!stricmp(config.ircnets[netno].cur_nick.c_str(), from)) {
						CHANNELS * chan = GetChannel(netno, parms[0]);
						if (chan) {
							chan->joined = false;
							chan->modes[0] = 0;
							chan->topic.clear();
						}
						ial.DeleteChan(parms[0]);
					} else {
						IAL_CHANNEL * Chan = ial.GetChan(parms[0]);
						IAL_NICK * Nick = ial.GetNick(Chan, from, hostmask, true);
						if (Nick) { ial.DeleteNick(Chan, Nick); }
					}

					USER_PRESENCE * up = alloc_irc_presence(config.ircnets[netno].sock, netno, from, hostmask, parms[0]);
					IBM_USERTEXT ut;
					memset(&ut,0,sizeof(ut));
					ut.userpres = up;
					ut.type = IBM_UTT_CHANNEL;
					ut.text = parms[1];
					SendMessage(-1,IB_ONPART,(char *)&ut,sizeof(ut));
					UnrefUser(up);
					continue;
				}

				if (!stricmp(cmd, "KICK")) {
					USER_PRESENCE * up = alloc_irc_presence(config.ircnets[netno].sock, netno, from, hostmask, parms[0]);

					char * hm = NULL;
					if (!stricmp(config.ircnets[netno].cur_nick.c_str(), parms[1])) {
						//bot was kicked from channel
						ib_printf(_("[irc-%d] You were kicked from channel %s by %s (reason: %s)\n"), netno, parms[0], from, parms[2] ? parms[2] : "None given");
						CHANNELS * chan = GetChannel(netno, parms[0]);
						if (chan) {
							chan->joined = false;
							chan->modes[0] = 0;
							chan->topic.clear();
						}
						ial.DeleteChan(parms[0]);
					} else {
						ib_printf(_("[irc-%d] %s was kicked from channel %s by %s (reason: %s)\n"), netno, parms[1], parms[0], from, parms[2] ? parms[2] : "None given");
						IAL_CHANNEL * Chan = ial.GetChan(parms[0]);

						//update kicker's hostmask
						IAL_NICK * Nick = ial.GetNick(Chan, from, hostmask);

						//removed kicked person from IAL for channel
						Nick = ial.GetNick(Chan, parms[1], NULL, true);
						if (Nick) {
							if (Nick->hostmask.length()) {
								hm = zstrdup(Nick->hostmask.c_str());
							}
							ial.DeleteNick(Chan, Nick);
						}
					}

					IBM_ON_KICK ut;
					memset(&ut,0,sizeof(ut));
					ut.reason = parms[2];
					ut.kicker = up;
					ut.kickee = parms[1];
					ut.kickee_hostmask = hm;
					SendMessage(-1,IB_ONKICK,(char *)&ut,sizeof(ut));
					zfreenn(hm);
					UnrefUser(up);
				}

				if (!stricmp(cmd,"TOPIC")) {
					if (parms[1] == NULL) { parms[1]=""; }
					CHANNELS * chan = GetChannel(netno, parms[0]);
					if (chan) {
						chan->topic = parms[1];
					}

					IAL_CHANNEL * Chan = ial.GetChan(parms[0]);
					ial.GetNick(Chan, from, hostmask);

					ib_printf(_("[irc-%d] Topic in %s changed by %s to '%s'\n"),netno,parms[0],from,parms[1]);

					USER_PRESENCE * up = alloc_irc_presence(config.ircnets[netno].sock, netno, from, hostmask, parms[0]);
					IBM_USERTEXT ut;
					memset(&ut,0,sizeof(ut));
					ut.userpres = up;
					ut.type = IBM_UTT_CHANNEL;
					ut.text = parms[1];
					SendMessage(-1,IB_ONTOPIC,(char *)&ut,sizeof(ut));
					UnrefUser(up);
					continue;
				}

				if (!stricmp(cmd,"INVITE")) {
					if (!strcmp(parms[0],config.ircnets[netno].cur_nick.c_str())) {
						bool auth = false;
						int32 chanind = -1;
						/// see if it is one of our channels
						for (int32 i=0; i < config.ircnets[netno].num_chans; i++) {
							if (!stricmp(config.ircnets[netno].channels[i].channel.c_str(), parms[1])) {
								/// it is one of our channels
								chanind = i;
								auth = true;
								break;
							}
						}
						if (!auth && uflag_have_any_of(GetUserFlags(hostmask), UFLAG_MASTER|UFLAG_OP|UFLAG_CHANADMIN)) {
							/// if an admin is doing the inviting, join the channel anyway
							auth = true;
						}
						if (auth) {
							ib_printf(_("[irc-%d] Invited to %s by %s, joining...\n"), netno, parms[1], from);
							if (chanind >= 0) {
								JoinChannel(netno, &config.ircnets[netno].channels[chanind]);
							} else {
								JoinChannel(netno, parms[1], NULL);
							}
						}
					}
				}

				if (!stricmp(cmd,"NICK")) {
					ial.ChangeNick(from,parms[0]);

					if (!stricmp(from, config.ircnets[netno].cur_nick.c_str())) {
						config.ircnets[netno].cur_nick = parms[0];
					}

					IBM_NICKCHANGE nc;
					nc.netno = netno;
					nc.old_nick = from;
					nc.new_nick = parms[0];
					nc.channel = NULL;
					SendMessage(-1,IB_ONNICK,(char *)&nc,sizeof(nc));

					for (int i=0; i < MAX_IRC_CHANNELS; i++) {
						IAL_CHANNEL * chan = ial.GetChan(config.ircnets[netno].channels[i].channel.c_str(), true);
						if (chan && ial.GetNick(chan, parms[0], NULL, true)) {
							nc.channel = zstrdup(config.ircnets[netno].channels[i].channel.c_str());
							SendMessage(-1,IB_ONNICK,(char *)&nc,sizeof(nc));
							zfree((void *)nc.channel);
						}
					}

					continue;
				}

				if (!stricmp(cmd,"QUIT")) {
					USER_PRESENCE * up = alloc_irc_presence(config.ircnets[netno].sock, netno, from, hostmask, NULL);
					IBM_USERTEXT ut;
					memset(&ut,0,sizeof(ut));
					ut.userpres = up;
					ut.type = IBM_UTT_PRIVMSG;
					ut.text = parms[0];
					SendMessage(-1,IB_ONQUIT,(char *)&ut,sizeof(ut));

					ut.type = IBM_UTT_CHANNEL;
					for (int i=0; i < MAX_IRC_CHANNELS; i++) {
						//IAL_CHANNEL * chan = ial.GetChan((char *)config.ircnets[netno].channels[i].channel.c_str(), true);
						if (ial.IsNickOnChan(config.ircnets[netno].channels[i].channel.c_str(),from)) {//chan && ial.GetNick(chan, from, true)
							USER_PRESENCE * up2 = alloc_irc_presence(config.ircnets[netno].sock, netno, from, hostmask, config.ircnets[netno].channels[i].channel.c_str());
							ut.userpres = up2;
							SendMessage(-1,IB_ONQUIT,(char *)&ut,sizeof(ut));
							UnrefUser(up2);
						}
					}

					ial.DeleteNickFromAll(from);
					UnrefUser(up);

					if (config.ircnets[netno].regainNick && !stricmp(config.ircnets[netno].base_nick.c_str(),from)) {
						sprintf(buf,"NICK %s\r\n", config.ircnets[netno].base_nick.c_str());
						bSend(config.ircnets[netno].sock,buf,0,PRIORITY_DEFAULT);
					}
					continue;
				}

				if (!stricmp(cmd,"MODE")) {
					USER_PRESENCE * up = alloc_irc_presence(config.ircnets[netno].sock, netno, from, hostmask, config.ircnets[netno].IsChannelPrefix(parms[0][0]) ? parms[0]:NULL);
					IBM_USERTEXT ut;
					memset(&ut,0,sizeof(ut));
					ut.userpres = up;
					snprintf(buf2, sizeof(buf2), "%s", parms[1]);
					for (int i=2; i < nparms; i++) {
						strcat(buf2, " ");
						strcat(buf2, parms[i]);
					}
					ut.text = buf2;
					if (config.ircnets[netno].IsChannelPrefix(parms[0][0])) {
						ut.type = IBM_UTT_CHANNEL;
					} else {
						ut.type = IBM_UTT_PRIVMSG;
					}
					SendMessage(-1,IB_ONMODE,(char *)&ut,sizeof(ut));

					if (!stricmp(parms[0], config.ircnets[netno].cur_nick.c_str())) {
						apply_irc_modes(config.ircnets[netno].my_mode, sizeof(config.ircnets[netno].my_mode), parms[1]);
						ib_printf(_("[irc-%d] %s sets bot's usermode: %s (+%s)\n"), netno, from, parms[1], config.ircnets[netno].my_mode);
					} else if (config.ircnets[netno].IsChannelPrefix(parms[0][0])) {
						ib_printf(_("[irc-%d] %s sets mode in %s: %s\n"), netno, from, parms[0], buf2);
						CHANNELS * chan = GetChannel(netno, parms[0]);
						if (chan) {
							apply_irc_channel_modes(netno, up, chan, parms, nparms, 1);
						}
						IAL_CHANNEL * Chan = ial.GetChan(parms[0]);
						ial.GetNick(Chan, from, hostmask);
						//ib_printf("[irc-%d] nparms: %d %s / %s / %s\n", netno, nparms, parms[0], parms[1], parms[2]);
					}
					UnrefUser(up);
					continue;
				}

#if defined(DEBUG)
				ib_printf("[irc-%d] unhandled cmd: %s\n", netno, cmd);
#endif
			}

			if (hostmask) { zfree(hostmask); hostmask=NULL; }
		} else {
			config.ircnets[netno].ready=false;
			ib_printf(_("Error connecting to: %s:%d (%s)\n"), config.ircnets[netno].host.c_str(), config.ircnets[netno].port, config.sockets->GetLastErrorString());
		}

		//ial.Dump();
		ial.Clear();
		ib_printf(_("[irc-%d] Disconnected from server...\n"),netno);

		config.ircnets[netno].ready = false;
		SendQ_ClearSockEntries(config.ircnets[netno].sock);
		time_t timeo = time(NULL) + 60;
		while (SendQ_HaveSockEntries(config.ircnets[netno].sock) && time(NULL) < timeo) {
			ib_printf(_("[irc-%d] Waiting for the rest the data on this IRC connection to be sent...\n"), netno);
			safe_sleep(1);
			SendQ_ClearSockEntries(config.ircnets[netno].sock);
		}
		config.sockets->Close(config.ircnets[netno].sock);
		config.ircnets[netno].sock = NULL;

		if (config.ircnets[netno].log_fp) {
			fprintf(config.ircnets[netno].log_fp, "[irc-%d] Disconnected from server...\r\n\r\n",netno);
			fclose(config.ircnets[netno].log_fp);
			config.ircnets[netno].log_fp = NULL;
		}

		if (!config.shutdown_now) {
			if (tries > 20) { tries = 20; }
			int wait = 30 * tries;
			ib_printf(_("[irc-%d] Waiting %d seconds to attempt reconnect... (try count: %d)\n"), netno, wait, tries);
			for (int sd_cnt=0; sd_cnt < wait && !config.shutdown_now; sd_cnt++) {
				safe_sleep(1);
			}
		}
	}

	TT_THREAD_END
}
#endif
