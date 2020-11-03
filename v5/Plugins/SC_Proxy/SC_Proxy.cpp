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

#include "../../src/plugins.h"
#define LIBSSMT_DLL
#include "../../../Common/LibSSMT/libssmt.h"

BOTAPI_DEF * api = NULL;
int pluginnum = 0;
Titus_Sockets socks;
Titus_Mutex scpMutex;

struct PASSTHRU_CONFIG {
	bool shutdown_now;
	int lPort;
	char pass[128];
	char record_fn[MAX_PATH];

	SSMT_CTX * ssmt;
	SSMT_TAG * ssmt_tag;

	T_SOCKET * lBase, * lSource, * cSource;
	SOUND_SERVER ss;
};

PASSTHRU_CONFIG passthru_config;

bool check_pass(char * buf, char * user = NULL, size_t userSize=0) {
	bool ret = false;
	char * pass = strchr(buf, ':');
	if (pass) {
		*pass = 0;
		pass++;
		USER * u = api->user->GetUserByNick(buf);
		if (u) {
			if (user) { strlcpy(user, u->Nick, userSize); }
			if (u->Pass[0] && !strcmp(u->Pass, pass)) {
				ret = true;
			}
			UnrefUser(u);
		}
	}
	return ret;
}

bool authorize_source(T_SOCKET * sock, char * buf, size_t bufSize, char * user, size_t userSize) {
	bool ret = false;
	bool gotline = false;
	int n;
	time_t timeo = time(NULL)+30;
	while (!passthru_config.shutdown_now && timeo > time(NULL) && (n = socks.Select_Read(sock, 1000)) >= 0) {
		if (n > 0 && (n = socks.RecvLine(sock, buf, bufSize)) >= RL3_NOLINE) {
			if (n > 0) {
				gotline = true;
				if (check_pass(buf, user, userSize)) {
					api->ib_printf(_("SC Proxy -> Source connected from %s: %s\n"), sock->remote_ip, buf);
					ret = true;
				}

				if (!ret) {
					api->ib_printf(_("SC Proxy -> Invalid password from %s: '%s'\n"), sock->remote_ip, buf);
					socks.Send(sock, "invalid password\r\n");
				}
				break;
			}
		}
	}
	if (!ret && !gotline) {
		api->ib_printf(_("SC Proxy -> Error receiving password from source client!\n"));
	}
	return ret;
}

bool connect_source(T_SOCKET * sock, char * buf, size_t bufSize) {
	bool ret = false;
	bool gotline = false;
	int n;
	snprintf(buf, bufSize, "%s\r\n", passthru_config.pass);
	socks.Send(sock, buf);
	int line=0;
	time_t timeo = time(NULL)+30;
	while (!passthru_config.shutdown_now && timeo > time(NULL) && (n = socks.Select_Read(sock, 1000)) >= 0) {
		if (n > 0) {
			n = socks.RecvLine(sock, buf, bufSize);
			if (n > 0) {
				line++;
				if (line == 1) {
					gotline = true;
					if (!stricmp(buf, "OK2")) {
						api->ib_printf(_("SC Proxy -> Connected to SHOUTcast...\n"));
						ret = true;
					} else {
						api->ib_printf(_("SC Proxy -> Error connecting to SHOUTcast: %s\n"), buf);
					}
				}
			} else if (n == 0 || n == RL3_NOLINE) {
				break;
			} else if (n == RL3_CLOSED) {
				api->ib_printf(_("SC Proxy -> Error connecting to SHOUTcast: %s\n"), "server closed connection (source already connected?)");
				break;
			} else {
				api->ib_printf(_("SC Proxy -> Error connecting to SHOUTcast: %s\n"), socks.GetLastErrorString());
				break;
			}
		}
	}
	if (!ret && !gotline) {
		api->ib_printf(_("SC Proxy -> Error receiving response from SHOUTcast!\n"));
	}
	return ret;
}

bool forward_headers(T_SOCKET * sock, T_SOCKET * dest, char * buf, size_t bufSize) {
	bool ret = false;
	bool gotline = false;
	int n;
	time_t timeo = time(NULL)+30;
	while (!passthru_config.shutdown_now && timeo > time(NULL) && (n = socks.Select_Read(sock, 1000)) >= 0) {
		if (n > 0 && (n = socks.RecvLine(sock, buf, bufSize)) >= RL3_NOLINE) {
			if (n > 0) {
				gotline = true;
				api->ib_printf("SC Proxy -> Header: %s\n", buf);
				strlcat(buf, "\r\n", bufSize);
				socks.Send(dest, buf);
			} else if (n == 0) {
				gotline = true;
				ret = true;
				socks.Send(dest, "\r\n");
				break;
			}
		}
	}
	if (!ret && !gotline) {
		api->ib_printf(_("SC Proxy -> Error receiving headers from source client!\n"));
	}
	return ret;
}

TT_DEFINE_THREAD(SourceThread) {
	TT_THREAD_START
	T_SOCKET * sock = (T_SOCKET *)tt->parm;
	char buf[4096];
	char user[128];

	if (authorize_source(sock, buf, sizeof(buf), user, sizeof(user))) {
		T_SOCKET * srv = socks.Create();
		if (socks.ConnectWithTimeout(srv, passthru_config.ss.host, passthru_config.ss.port+1, 10000)) {
			if (connect_source(srv, buf, sizeof(buf))) {
				socks.Send(sock, "OK2\r\nicy-caps:11\r\n\r\n");
				if (forward_headers(sock, srv, buf, sizeof(buf))) {
					time_t lastRecv = time(NULL);
					socks.SetRecvTimeout(sock, 30000);
					socks.SetSendTimeout(srv, 30000);
					socks.SetKeepAlive(sock);
					socks.SetKeepAlive(srv);
					int n;

					if (passthru_config.record_fn[0]) {
						sstrcpy(buf, passthru_config.record_fn);
						str_replaceA(buf, sizeof(buf), "%nick%", user);
						char buf2[128];
						struct tm lt;
						time_t tme = time(NULL);
						localtime_r(&tme, &lt);
						strftime(buf2, sizeof(buf2), "%m_%d_%Y", &lt);
						str_replaceA(buf, sizeof(buf), "%date%", buf2);
						strftime(buf2, sizeof(buf2), "%H_%M_%S", &lt);
						str_replaceA(buf, sizeof(buf), "%time%", buf2);
						scpMutex.Lock();
						if ((passthru_config.ssmt = LibSSMT_OpenFile(buf, "wb")) != NULL) {
							api->ib_printf(_("SC Proxy -> Opened '%s' for recording...\n"), buf, strerror(errno));
						} else {
							api->ib_printf(_("SC Proxy -> Error opening '%s' for recording... (%s)\n"), buf, strerror(errno));
						}
						scpMutex.Release();
					}

					while (!passthru_config.shutdown_now && (time(NULL)-lastRecv) < 30 && (n = socks.Select_Read(sock, 1000)) >= 0) {
						if (n == 1) {
							n = socks.Recv(sock, buf, sizeof(buf));
							if (n > 0) {
								lastRecv = time(NULL);
								if (passthru_config.ssmt) {
									scpMutex.Lock();
									bool dowrite = true;
									if (passthru_config.ssmt_tag) {
										int i;
										for (i=0; i < n-4; i++) {
											uint32 head = Get_UBE32(*((uint32 *)&buf[i]));
											/*          sync word                      mpeg version 1/2/2.5     make sure layer 3          check invalid bitrate       check invalid samplerate */
											if (((head & 0xffe00000) == 0xffe00000) && (((head>>19)&3) != 1) && (((head>>17)&3) == 1) && (((head>>12)&0xf) != 0xf) && (((head>>10)&0x3) != 0x3)) {
												//got MP3 frame
												dowrite = false;
												break;
											}
										}
										if (!dowrite) {
											LibSSMT_WriteFile(passthru_config.ssmt, buf, i);
											SSMT_TAG * tag = passthru_config.ssmt_tag;
											if (LibSSMT_WriteTag(passthru_config.ssmt, tag)) {
												api->ib_printf(_("SC Proxy -> Wrote SSMT tag to recording file.\n"));
											} else {
												api->ib_printf(_("SC Proxy -> Error writing SSMT tag to recording file! (%s)\n"), strerror(errno));
											}
											LibSSMT_FreeTag(tag);
											passthru_config.ssmt_tag = NULL;
											LibSSMT_WriteFile(passthru_config.ssmt, &buf[i], n-i);
										}
									}
									if (dowrite && !LibSSMT_WriteFile(passthru_config.ssmt, buf, n)) {
										api->ib_printf(_("SC Proxy -> Error writing to recording file, closing... (%s)\n"), strerror(errno));
										LibSSMT_CloseFile(passthru_config.ssmt);
										passthru_config.ssmt = NULL;
									}
									scpMutex.Release();
								}
								if (socks.Send(srv, buf, n) != n) {
									api->ib_printf(_("SC Proxy -> Socket error sending to SHOUTcast: %s\n"), socks.GetLastErrorString());
									break;
								}
							} else if (n == 0) {
								api->ib_printf(_("SC Proxy -> Source closed connection.\n"));
								break;
							} else {
								api->ib_printf(_("SC Proxy -> Socket error in source socket: %s\n"), socks.GetLastErrorString());
								break;
							}
						}
					}

					scpMutex.Lock();
					if (passthru_config.ssmt) {
						LibSSMT_CloseFile(passthru_config.ssmt);
						passthru_config.ssmt = NULL;
						api->ib_printf(_("SC Proxy -> Closed recording file.\n"));
					}
					scpMutex.Release();
				}
			} else {
				socks.Send(sock, "error connecting to SHOUTcast\r\n");
			}
		} else {
			socks.Send(sock, "error connecting to SHOUTcast\r\n");
		}
		socks.Close(srv);
	} else {
	}

	socks.Close(sock);
	LockMutex(scpMutex);
	passthru_config.cSource = NULL;
	RelMutex(scpMutex);
	TT_THREAD_END
}

typedef std::map<string, string, ci_less> parmList;
TT_DEFINE_THREAD(HTTPThread) {
	TT_THREAD_START
	T_SOCKET * sock = (T_SOCKET *)tt->parm;

	parmList parms;
	std::vector<string> request;
	char buf[1024];
//	char buf2[1024];
	int resp = 0;
	int n;
	int line=0;
	time_t timeo = time(NULL)+30;
	while (!passthru_config.shutdown_now && timeo > time(NULL) && (n = socks.Select_Read(sock, 1000)) >= 0) {
		if (n > 0 && (n = socks.RecvLine(sock, buf, sizeof(buf))) >= RL3_NOLINE) {
			if (n > 0) {
				line++;
				if (line == 1) {
					if (!strnicmp(buf, "GET ", 4)) {
						char * p = &buf[4];
						if (!strnicmp(p, "/admin.cgi?", 11)) {
							resp = 401;
							p += 11;
							p=p;
							char * q = p + strlen(p) - 9;
							if (stricmp(q, " HTTP/1.0") || stricmp(q, " HTTP/1.1")) {
								*q = 0;
							}
							StrTokenizer st(p, '&');
							for (unsigned long i=1; i <= st.NumTok(); i++) {
								char * c = st.GetSingleTok(i);
								q = strchr(c, '=');
								if (q) {
									*q = 0;
									q++;
									parms[c] = q;
									if (!stricmp(c, "pass")) {
										resp = check_pass(q) ? 200:401;
									}
								}
								st.FreeString(c);
							}
							//if (resp == resp = 200;
						} else {
							resp = 404;
						}
					} else {
						resp = 500;
					}
				} else {
					request.push_back(buf);
				}
				api->ib_printf("SC Proxy -> Header: %s\n", buf);
			} else if (n == 0) {
				break;
			}
		}
	}
	if (resp == 401) {
		socks.Send(sock, "HTTP/1.0 401 Unauthorized\r\nConnection: close\r\n\r\nInvalid password.");
	} else if (resp == 404) {
		socks.Send(sock, "HTTP/1.0 404 Not found\r\nConnection: close\r\n\r\nFile not found.");
	} else if (resp == 500) {
		socks.Send(sock, "HTTP/1.0 500 Unsupported method\r\nConnection: close\r\n\r\nUnsupported method.");
	} else if (resp == 200) {
		socks.Send(sock, "HTTP/1.0 200 OK\r\n\r\nTitle updated.");
	}
	safe_sleep(2);
	socks.Close(sock);

	if (resp == 200) {
		scpMutex.Lock();
		if (passthru_config.ssmt) {
			if (passthru_config.ssmt_tag == NULL) {
				SSMT_TAG * tag = LibSSMT_NewTag();
				for (parmList::const_iterator x = parms.begin(); x != parms.end(); x++) {
					if (!stricmp(x->first.c_str(), "song")) {
						char * tmp = api->curl->unescape(x->second.c_str(), x->second.length());
						strtrim(tmp);
						LibSSMT_AddItem(tag, SSMT_STIT, tmp);
						api->curl->free(tmp);
					} else if (!stricmp(x->first.c_str(), "url")) {
						char * tmp = api->curl->unescape(x->second.c_str(), x->second.length());
						strtrim(tmp);
						LibSSMT_AddItem(tag, SSMT_SURL, tmp);
						api->curl->free(tmp);
					}
				}
				if (tag->num_items > 0) {
					passthru_config.ssmt_tag = tag;
				} else {
					LibSSMT_FreeTag(tag);
				}
			}
		}
		scpMutex.Release();
		sock = socks.Create();
		if (socks.ConnectWithTimeout(sock, passthru_config.ss.host, passthru_config.ss.port, 10000)) {
			socks.SetSendTimeout(sock, 10000);
			stringstream sstr;
			sstr << "GET /admin.cgi?";
			for (parmList::const_iterator x = parms.begin(); x != parms.end(); x++) {
				if (x != parms.begin()) {
					sstr << "&";
				}
				if (!stricmp(x->first.c_str(), "pass")) {
					sstr << x->first << "=" << passthru_config.pass;
				} else {
					sstr << x->first << "=" << x->second;
				}
			}
			sstr << " HTTP/1.0\r\n";
			for (std::vector<string>::const_iterator y = request.begin(); y != request.end(); y++) {
				sstr << *y << "\r\n";
			}
			sstr << "\r\n";
			socks.Send(sock, sstr.str().c_str());
			safe_sleep(3);
		}
		socks.Close(sock);
	}

	TT_THREAD_END
}

TT_DEFINE_THREAD(PassthruThread) {
	TT_THREAD_START
	TITUS_SOCKET_LIST tfd;
	while (!passthru_config.shutdown_now) {
		TFD_ZERO(&tfd);
		TFD_SET(&tfd, passthru_config.lBase);
		TFD_SET(&tfd, passthru_config.lSource);
		if (socks.Select_Read_List(&tfd, 1000) > 0) {
			if (TFD_ISSET(&tfd, passthru_config.lBase)) {
				T_SOCKET * sock = socks.Accept(passthru_config.lBase);
				if (sock) {
					TT_StartThread(HTTPThread, sock, "SC HTTP Thread", pluginnum);
				}
			}
			if (TFD_ISSET(&tfd, passthru_config.lSource)) {
				T_SOCKET * sock = socks.Accept(passthru_config.lSource);
				LockMutex(scpMutex);
				if (passthru_config.cSource == NULL) {
					passthru_config.cSource = sock;
					RelMutex(scpMutex);
					TT_StartThread(SourceThread, sock, "SC Source Thread", pluginnum);
				} else {
					RelMutex(scpMutex);
					socks.Send(sock, "source already connected\r\n");
					socks.Close(sock);
				}
			}
		}
	}
	TT_THREAD_END
}

int Notes_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
	/*
	if (!stricmp(command, "remember") && (!parms || !strlen(parms))) {
		ut->std_reply(ut, _("Usage: remember text"));
		return 1;
	}

	if (!stricmp(command, "remember")) {
		AddNote(ut->User->Nick, ut->User->Nick, _("Remember Note"), parms);
		ut->std_reply(ut, _("I will remember that for you"));
		return 1;
	}
	*/

	return 0;
}

bool BindSocket(T_SOCKET ** oSock, int port) {
	T_SOCKET * sock = socks.Create();
	if (sock == NULL) {
		api->ib_printf(_("SC Proxy -> Error creating socket! (%s)\n"), socks.GetLastErrorString());
		return false;
	}
	socks.SetReuseAddr(sock, true);
	if (!socks.Bind(sock, port)) {
		api->ib_printf(_("SC Proxy -> Error binding socket on port %d! (%s)\n"), port, socks.GetLastErrorString());
		socks.Close(sock);
		return false;
	}
	if (!socks.Listen(sock)) {
		api->ib_printf(_("SC Proxy -> Error listen()ing socket on port %d! (%s)\n"), port, socks.GetLastErrorString());
		socks.Close(sock);
		return false;
	}
	*oSock = sock;
	return true;
}

int init(int num, BOTAPI_DEF * BotAPI) {
	api = (BOTAPI_DEF *)BotAPI;
	pluginnum = num;
	memset(&passthru_config, 0, sizeof(passthru_config));

	if (api->ss == NULL) {
		api->ib_printf2(pluginnum,_("SC Proxy -> This version of RadioBot is not supported!\n"));
		return 0;
	}

	api->ss->GetSSInfo(0, &passthru_config.ss);
	if (passthru_config.ss.type != SS_TYPE_SHOUTCAST && passthru_config.ss.type != SS_TYPE_SHOUTCAST2) {
		api->ib_printf(_("SC Proxy -> I only support SHOUTcast at this time...\n"));
		return 0;
	}
	if (passthru_config.ss.type == SS_TYPE_SHOUTCAST2) {
		api->ib_printf(_("SC Proxy -> FYI: I only support SHOUTcast v2 with legacy source/compatibility enabled at this time...\n"));
	}

	passthru_config.lPort = 8000;
	DS_CONFIG_SECTION * sec = api->config->GetConfigSection(NULL, "SC_Proxy");
	if (sec) {
		long x = api->config->GetConfigSectionLong(sec, "Port");
		if (x > 0 && x < 65535) {
			passthru_config.lPort = x;
		}
		api->config->GetConfigSectionValueBuf(sec, "SourcePass", passthru_config.pass, sizeof(passthru_config.pass));
		api->config->GetConfigSectionValueBuf(sec, "RecordFN", passthru_config.record_fn, sizeof(passthru_config.record_fn));
	}

	if (passthru_config.pass[0] == 0) {
		api->ib_printf(_("SC Proxy -> No 'SourcePass' defined in your 'SC_Proxy' section, unloading...\n"));
		return 0;
	}

	if (!BindSocket(&passthru_config.lBase, passthru_config.lPort)) {
		return 0;
	}
	if (!BindSocket(&passthru_config.lSource, passthru_config.lPort+1)) {
		socks.Close(passthru_config.lBase);
		return 0;
	}
	/*
	COMMAND_ACL acl;
	api->commands->FillACL(&acl, 0, UFLAG_DJ|UFLAG_HOP|UFLAG_OP|UFLAG_MASTER, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "notes",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE, Notes_Commands, _("This will let you manage your notes, or send new ones"));
	api->commands->RegisterCommand_Proc(pluginnum, "remember", &acl, CM_ALLOW_ALL, Notes_Commands, _("This will record a note to yourself based on what you type after it"));
	*/
	TT_StartThread(PassthruThread, NULL, "SC Proxy Thread", pluginnum);
	return 1;
}

void quit() {
	api->ib_printf(_("SC Proxy -> Shutting down...\n"));
	passthru_config.shutdown_now = true;
	while (TT_NumThreadsWithID(pluginnum) > 0) {
		api->ib_printf(_("SC Proxy -> Waiting for threads to exit...\n"));
		safe_sleep(1);
	}
	socks.Close(passthru_config.lBase);
	socks.Close(passthru_config.lSource);
	//api->commands->UnregisterCommandByName( "remember" );
	//api->commands->UnregisterCommandByName( "notes" );
	api->ib_printf(_("SC Proxy -> Shut down.\n"));
}

int message(unsigned int msg, char * data, int datalen) {
	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{3D237F15-33D7-435B-8A79-ADA309AE6DC1}",
	"SC_Proxy",
	"SHOUTcast Proxy Plugin 1.0",
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
