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

#include "dcc.h"
#ifdef DEBUG
#pragma comment(lib, "physfs_d.lib")
#else
#pragma comment(lib, "physfs.lib")
#endif

Titus_Sockets3 * sockets = new Titus_Sockets3();

BOTAPI_DEF *api = NULL;
int pluginnum; // the number we were assigned
Titus_Mutex sendMutex;

int DCC_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type);

DCCT_CONFIG dcc_config;

std::vector<DCCT_CHAT *> chatList;
std::vector<DCCT_QUEUE *> recvList;
std::vector<DCCT_CHAT *> fservList;

#if PHYSFS_HAS_STAT == 0
int PHYSFS_stat(const char *fname, PHYSFS_Stat * st) {
	memset(st, 0, sizeof(PHYSFS_Stat));
	if (PHYSFS_exists(fname) != 0) {
		st->modtime = PHYSFS_getLastModTime(fname);
		if (PHYSFS_isDirectory(fname)) {
			st->filetype = PHYSFS_FILETYPE_DIRECTORY;
		} else if (PHYSFS_isSymbolicLink(fname)) {
			st->filetype = PHYSFS_FILETYPE_SYMLINK;
		} else {
			st->filetype = PHYSFS_FILETYPE_REGULAR;
		}
		st->readonly = 1;
		PHYSFS_file * fp = PHYSFS_openRead(fname);
		if (fp != NULL) {
			st->filesize = PHYSFS_fileLength(fp);
			PHYSFS_close(fp);
		}
		return 0;
	}
	return -1;
}
#endif

int PHYSFS_myStat(const char *fname, PHYSFS_MyStat * st) {
	if (PHYSFS_stat(fname, &st->st) != 0) { return -1; }
	snprintf(st->real_fn, sizeof(st->real_fn), "%s%s%s", PHYSFS_getRealDir(fname), PHYSFS_getDirSeparator(), fname);
	str_replace(st->real_fn, sizeof(st->real_fn), "/", PHYSFS_getDirSeparator());
	return 0;
}

struct STATUS_TXT {
	DCCT_STATUS s;
	const char * msg;
};

STATUS_TXT status_text[] = {
	{ DS_DROP, "Closing chat" },
	{ DS_CONNECTING, "Connecting" },
	{ DS_WAITING, "Waiting for incoming connection" },
	{ DS_CONNECTED, "Connected" },
	{ DS_AUTHENTICATED, "In Chat" },
	{ DS_TRANSFERRING, "Transferring" },
	{ (DCCT_STATUS)-1, "Unknown status!" }
};

const char * GetStatusText(DCCT_STATUS s) {
	for (int i = 0; ; i++) {
		if (status_text[i].s == s || status_text[i].s == -1) {
			return status_text[i].msg;
		}
	}
};


void LoadConfig() {
	memset(&dcc_config, 0, sizeof(dcc_config));
	dcc_config.enable_chat = true;
	dcc_config.first_port  = 1025;
	dcc_config.last_port  = 65535;
	dcc_config.maxFServs = 4;
	dcc_config.maxSends = dcc_config.maxRecvs = 2;
	strcpy(dcc_config.fserv_trigger, "fserv");
	strcpy(dcc_config.get_trigger, "get");
	snprintf(dcc_config.recv_path, sizeof(dcc_config.recv_path), "%sincoming" PATH_SEPS "", api->GetBasePath());
	snprintf(dcc_config.get_path, sizeof(dcc_config.get_path), "%sincoming" PATH_SEPS "", api->GetBasePath());
	snprintf(dcc_config.fserv_path, sizeof(dcc_config.fserv_path), "%sincoming" PATH_SEPS "", api->GetBasePath());
	DS_CONFIG_SECTION * Scan = api->config->GetConfigSection(NULL, "DCC");
	if (Scan) {
		dcc_config.enable_fserv = api->config->GetConfigSectionLong(Scan, "AllowFServ") > 0 ? true:false;
		dcc_config.enable_recv = api->config->GetConfigSectionLong(Scan, "AllowIncoming") > 0 ? true:false;
		dcc_config.enable_get = api->config->GetConfigSectionLong(Scan, "AllowGet") > 0 ? true:false;
		dcc_config.enable_chat = api->config->GetConfigSectionLong(Scan, "AllowChat") > 0 ? true:false;
		dcc_config.AllowSymLinks = api->config->GetConfigSectionLong(Scan, "AllowSymLinks") > 0 ? true:false;
		dcc_config.enable_autodj = api->config->GetConfigSectionLong(Scan, "EnableAutoDJ") > 0 ? true:false;

		long lVal = api->config->GetConfigSectionLong(Scan, "FirstPort");
		if (lVal > 0 && lVal <= 65535) {
			dcc_config.first_port = lVal;
		}
		lVal = api->config->GetConfigSectionLong(Scan, "LastPort");
		if (lVal > 0 && lVal >= dcc_config.first_port && lVal <= 65535) {
			dcc_config.last_port = lVal;
		}
		char * val = api->config->GetConfigSectionValue(Scan,"IncomingPath");
		if (val) {
			sstrcpy(dcc_config.recv_path,val);
			if (dcc_config.recv_path[strlen(dcc_config.recv_path)-1] != PATH_SEP) {
				strlcat(dcc_config.recv_path,  PATH_SEPS , sizeof(dcc_config.recv_path));
			}
		}
		val = api->config->GetConfigSectionValue(Scan,"GetPath");
		if (val) {
			sstrcpy(dcc_config.get_path,val);
			if (dcc_config.get_path[strlen(dcc_config.get_path)-1] != PATH_SEP) {
				strlcat(dcc_config.get_path,  PATH_SEPS , sizeof(dcc_config.get_path));
			}
		}
		val = api->config->GetConfigSectionValue(Scan,"FServPath");
		if (val) {
			sstrcpy(dcc_config.fserv_path,val);
			if (dcc_config.fserv_path[strlen(dcc_config.fserv_path)-1] != PATH_SEP) {
				strlcat(dcc_config.fserv_path,  PATH_SEPS , sizeof(dcc_config.fserv_path));
			}
		}
		val = api->config->GetConfigSectionValue(Scan,"FServTrigger");
		if (val) {
			sstrcpy(dcc_config.fserv_trigger,val);
		}
		val = api->config->GetConfigSectionValue(Scan,"GetTrigger");
		if (val) {
			sstrcpy(dcc_config.get_trigger,val);
		}
		val = api->config->GetConfigSectionValue(Scan,"RealIP");
		if (val) {
			sstrcpy(dcc_config.local_ip, val);
		}
	} else {
		api->ib_printf(_("DCC Support -> No 'DCC' block found in config, using pure defaults...\n"));
	}
}

int init(int num, BOTAPI_DEF * BotAPI) {
	pluginnum=num;
	api=BotAPI;
	if (api->irc == NULL) {
		api->ib_printf2(pluginnum,_("DCC Support -> This version of RadioBot is not supported!\n"));
		return 0;
	}
	LoadConfig();

	char buf[1024];

	if (dcc_config.enable_get) {
		//int32 minlevel, uint32 mask, CommandProcType proc, char * desc
		COMMAND_ACL acl;
		api->commands->FillACL(&acl, 0, UFLAG_DCC_XFER|UFLAG_DCC_ADMIN, 0);
		api->commands->RegisterCommand_Proc(pluginnum, dcc_config.get_trigger, &acl, CM_ALLOW_IRC_PUBLIC|CM_ALLOW_IRC_PRIVATE, DCC_Commands, _("This command lets you request a file from the bot"));
	}

	if (dcc_config.enable_fserv) {
#if defined(WIN32)
		sprintf(buf, "%sradiobot.exe", api->GetBasePath());
#else
		sprintf(buf, "%sradiobot", api->GetBasePath());
#endif
		if (PHYSFS_init(buf) != 0) {
			api->ib_printf(_("DCC Support -> VFS initialized...\n"));
			StrTokenizer st(dcc_config.fserv_path, ';');
			for (unsigned long i=1; i <= st.NumTok(); i++) {
				char * p = zstrdup(st.stdGetSingleTok(i).c_str());
				api->ib_printf(_("DCC Support -> Adding path: %s\n"), p);
				if (PHYSFS_mount(p, "", 1) == 0) {
					api->ib_printf(_("DCC Support -> Error adding directory: %s\n"), PHYSFS_getLastError());
				}
				if (i == 1) {
					if (PHYSFS_setWriteDir(p) == 0) {
						api->ib_printf(_("DCC Support -> Error setting write directory: %s\n"), PHYSFS_getLastError());
					}
				}
				zfree(p);
			}
			PHYSFS_permitSymbolicLinks(dcc_config.AllowSymLinks ? 1:0);
			COMMAND_ACL acl;
			api->commands->FillACL(&acl, 0, UFLAG_DCC_FSERV|UFLAG_DCC_ADMIN, 0);
			api->commands->RegisterCommand_Proc(pluginnum, dcc_config.fserv_trigger, &acl, CM_ALLOW_IRC_PUBLIC|CM_ALLOW_IRC_PRIVATE, DCC_Commands, _("This command will open an fserv session with you"));
		} else {
			api->ib_printf(_("DCC Support -> Error initializing VFS, FServ disabled!\n"));
			dcc_config.enable_fserv = false;
		}
	}

	if (dcc_config.enable_chat) {
		COMMAND_ACL acl;
		api->commands->FillACL(&acl, 0, UFLAG_DCC_CHAT|UFLAG_DCC_ADMIN, 0);
		api->commands->RegisterCommand_Proc(pluginnum, "chatme", &acl, CM_ALLOW_IRC_PUBLIC|CM_ALLOW_IRC_PRIVATE, DCC_Commands, _("This command lets you request a DCC chat from the bot (useful if you don't have your DCC ports setup properly)"));
	}

	COMMAND_ACL acl;
	api->commands->FillACL(&acl, 0, UFLAG_DCC_ADMIN, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "dccstatus", &acl, CM_ALLOW_ALL, DCC_Commands, _("This command lets you view the current DCC status"));
	TT_StartThread(SendThread, NULL, _("DCC SendThread"), pluginnum);
	return 1;
}

void quit() {
	api->ib_printf(_("DCC Support -> Shutting down...\n"));
	api->commands->UnregisterCommandsByPlugin(pluginnum);
	dcc_config.shutdown_now=true;
	time_t timeo = time(NULL) + 30;
	while (TT_NumThreadsWithID(pluginnum) && time(NULL) < timeo) {
		api->ib_printf(_("DCC Support -> Waiting on (%d) threads to die...\n"), TT_NumThreadsWithID(pluginnum));
		api->safe_sleep_seconds(1);
	}
	delete sockets;
	sockets = NULL;
	if (dcc_config.enable_fserv) {
		PHYSFS_deinit();
		api->ib_printf(_("DCC Support -> VFS shut down.\n"));
	}
	api->ib_printf(_("DCC Support -> Shut down.\n"));
}

int privmsgproc(USER_PRESENCE * ut, const char * msg) {
	if (msg[0] == 1) {
		char buf[1024];
		const char *p = msg+1;
		api->ib_printf(_("CTCP/DCC: %s\n"),msg);

		//Normal DCC: PRIVMSG User2 :\001DCC SEND filename ipaddress port filesize\001
		//Passive DCC: PRIVMSG User2 :\001DCC SEND filename ipaddress 0 filesize token\001
		if (!strncmp(p, "DCC SEND ", 9) && dcc_config.enable_recv && (ut->Flags & (UFLAG_DCC_XFER | UFLAG_DCC_ADMIN))) {
			p += 9;
			sstrcpy(buf, p);
			strtrim(buf, "\001", TRIM_RIGHT);
			if (buf[0] == '"') { // convert filename in quotes to underscores
				memmove(buf, ((char *)&buf) + 1, strlen(buf));
				char * q;
				for (q = buf; *q != '\"' && *q != 0; q++) {
					if (*q == ' ') {
						*q = '_';
					}
				}
				memmove(q, q + 1, strlen(q));
			}

			StrTokenizer st(buf, ' ');
			if (st.NumTok() >= 4) {
				sendMutex.Lock();
				if (recvList.size() < dcc_config.maxRecvs) {
					DCCT_QUEUE * Scan = (DCCT_QUEUE*)zmalloc(sizeof(DCCT_QUEUE));
					memset(Scan, 0, sizeof(DCCT_QUEUE));
					sstrcpy(Scan->nick, ut->Nick);
					Scan->netno = ut->NetNo;
					Scan->timestamp = time(NULL);

					sstrcpy(buf, st.stdGetSingleTok(1).c_str());
					if (strstr(buf, "..") || strchr(buf, '~') || strchr(buf, '/') || strchr(buf, '\\') || strchr(buf, ':')) {
						ut->send_notice(ut, _("That filename contains illegal characters!"));
						zfree(Scan);
						sendMutex.Release();
						return 0;
					}

					sprintf(Scan->fn, "%s%s", dcc_config.recv_path, buf);
					in_addr ia;
					ia.s_addr = ntohl(atoul(st.stdGetSingleTok(2).c_str()));
					inet_ntop(AF_INET, &ia, Scan->ip, sizeof(Scan->ip));
					Scan->port = atoul(st.stdGetSingleTok(3).c_str());
					Scan->size = atoi64(st.stdGetSingleTok(4).c_str());
					if (Scan->port == 0 && st.NumTok() >= 5) {
						Scan->token = atoi(st.stdGetSingleTok(5).c_str());
						Scan->status = DS_WAITING;
					} else {
						Scan->status = DS_CONNECTING;
					}

					if (Scan->port == 0 && Scan->token == 0) {
						ut->send_notice(ut, _("Passive request but no token given!"));
						zfree(Scan);
						sendMutex.Release();
						return 0;
					}

					recvList.push_back(Scan);
					sendMutex.Release();
					if (Scan->port > 0) {
						TT_StartThread(RecvThread, (void *)Scan, _("DCC RecvThread"), pluginnum);
					} else {
						TT_StartThread(RecvPasvThread, (void *)Scan, _("DCC RecvPasvThread"), pluginnum);
					}
				} else {
					sendMutex.Release();
					ut->send_notice(ut, _("I already have too many incoming files, please try again later"));
				}
			}
			return 1;
		}

		if (!strncmp(p,"DCC RESUME",10) && dcc_config.enable_recv && (ut->Flags & (UFLAG_DCC_XFER|UFLAG_DCC_ADMIN))) {
			//if (SendQueue == NULL) {
			//	return 1;
			//}
			//PRIVMSG User1 :DCC RESUME filename port position

			sstrcpy(buf,p);
			buf[strlen(buf) - 1]=0;
			char *p2=NULL;
			char *p=strtok_r(buf," ",&p2); // DCC
			if (p == NULL) { return 0; }
			p=strtok_r(NULL," ",&p2); // RESUME
			if (p == NULL) { return 0; }
			p=strtok_r(NULL," ",&p2); // filename
			if (p != NULL) {
				p=strtok_r(NULL," ",&p2); // port
				if (p) {
					int port = atoi(p);
					p=strtok_r(NULL," ",&p2); // position
					if (p) {
						int64 pos = atoi64(p);
						sendMutex.Lock();
						DCCT_QUEUE *Scan = curSend;
						if (!Scan || stricmp(Scan->nick,ut->Nick) || Scan->port != port) {
							Scan = SendQueue;
						}
						while (Scan != NULL && (stricmp(Scan->nick,ut->Nick) || Scan->port != port)) {
							Scan=Scan->Next;
						}
						if (Scan != NULL) { // found send
							Scan->pos=pos;
							snprintf(buf,sizeof(buf),"\001DCC ACCEPT \"file.ext\" %d " I64FMT "\001\n",port,pos);
							ut->send_msg(ut, buf);
						}
						sendMutex.Release();
					}
				}
			}
			return 1;
		}

		if (!strnicmp(p,"DCC CHAT chat ",14) && dcc_config.enable_chat && ut->User && (ut->Flags & (UFLAG_DCC_CHAT|UFLAG_DCC_ADMIN))) {
			//DCC CHAT chat 1154593213 8902
			if (!strlen(ut->User->Pass)) {
				ut->send_notice(ut, _("ERROR: You need to set a password with !chpass before you can DCC Chat me"));
				return 1;
			}

			DCCT_CHAT * tmp = (DCCT_CHAT *)zmalloc(sizeof(DCCT_CHAT));
			p += 14;
			sstrcpy(buf,p);
			buf[strlen(buf) - 1]=0;
			tmp->User = ut->User;
			tmp->Pres = ut;
			tmp->timestamp = time(NULL);

			char * p2 = NULL;
			p = strtok_r(buf," ",&p2);
			if (p != NULL) {
				in_addr ia;
				ia.s_addr = ntohl(strtoul(p, NULL, 10));
				inet_ntop(AF_INET, &ia, tmp->ip, sizeof(tmp->ip));
				p = strtok_r(NULL," ",&p2); // port
				if (p != NULL) {
					tmp->port = atol(p);
					RefUser(ut);
					TT_StartThread(ChatThread_Incoming,(void *)tmp, _("DCC Chat Thread"), pluginnum);
				} else {
					zfree(tmp);
				}
			} else {
				zfree(tmp);
			}
			return 1;
		}
	}
	return 0;
}

int DCC_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
	char buf[1024],buf2[1024];
	if (!stricmp(command, dcc_config.get_trigger) && dcc_config.enable_get && (!parms || !strlen(parms))) {
		//char buf[256];
		sprintf(buf, _("Usage: %s filename.ext"), dcc_config.get_trigger);
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command, dcc_config.get_trigger) && dcc_config.enable_get) {
		const char *p = parms;

		if (strstr(p,"..")) {
			ut->std_reply(ut, _("I think not."));
			sprintf(buf,_("%%bold%%color4[POSSIBLE HACK ATTEMPT]%%bold Warning: .. in filename from %s on [IRC-%d]: %s\r\n"),ut->Nick,ut->NetNo,p);
			api->irc->LogToChan(buf);
			return 1;
		}

		if (strstr(p,"/")) {
			ut->std_reply(ut, "I think not.");
			sprintf(buf,_("%%bold%%color4[POSSIBLE HACK ATTEMPT]%%bold Warning: / in filename from %s on [IRC-%d]: %s\r\n"),ut->Nick,ut->NetNo,p);
			api->irc->LogToChan(buf);
			return 1;
		}

		if (strstr(p,"\\")) {
			ut->std_reply(ut, "I think not.");
			sprintf(buf,_("%%bold%%color4[POSSIBLE HACK ATTEMPT]%%bold Warning: \\ in filename from %s on [IRC-%d]: %s\r\n"),ut->Nick,ut->NetNo,p);
			api->irc->LogToChan(buf);
			return 1;
		}

		if (strstr(p,"~")) {
			ut->std_reply(ut, "I think not.");
			sprintf(buf,_("%%bold%%color4[POSSIBLE HACK ATTEMPT]%%bold Warning: ~ in filename from %s on [IRC-%d]: %s\r\n"),ut->Nick,ut->NetNo,p);
			api->irc->LogToChan(buf);
			return 1;
		}

		sprintf(buf2,"%s%s",dcc_config.get_path,p);
		FILE * fp = fopen(buf2,"rb");
		if (!fp) {
#if !defined(IRCBOT_LITE)
			if (dcc_config.enable_autodj && api->SendMessage(-1, AUTODJ_IS_LOADED, NULL, 0)) {
				sstrcpy(buf2, p);
				if (api->SendMessage(-1, AUTODJ_GET_FILE_PATH, buf2, sizeof(buf2))) {
					fp = fopen(buf2, "rb");
				}
			}
#endif
			if (!fp) {
				ut->std_reply(ut, _("Error, that file is not in my serving directory!"));
				return 1;
			}
		}

		DCCT_QUEUE *dNew = (DCCT_QUEUE *)zmalloc(sizeof(DCCT_QUEUE));
		memset(dNew, 0, sizeof(DCCT_QUEUE));

		fseek64(fp, 0, SEEK_END);
		dNew->size = ftell64(fp);
		fclose(fp);

		dNew->netno = ut->NetNo;
		sstrcpy(dNew->nick, ut->Nick);
		sstrcpy(dNew->fn, buf2);
		ut->std_reply(ut, _("Your file has been put into the SendQueue..."));

		AddSend(dNew);
		return 1;
	}

	if (!stricmp(command, dcc_config.fserv_trigger) && dcc_config.enable_fserv) {
		sendMutex.Lock();
		if (fservList.size() <= dcc_config.maxFServs) {
			T_SOCKET * lSock = BindRandomPort();
			if (lSock == NULL) {
				ut->std_reply(ut, _("All of my DCC ports are in use, please try again later..."));
				return 1;
			}

			DCCT_CHAT *dNew = (DCCT_CHAT *)zmalloc(sizeof(DCCT_CHAT));
			memset(dNew, 0, sizeof(DCCT_CHAT));

			dNew->Pres = ut;
			dNew->User = ut->User;
			RefUser(ut);
			dNew->timestamp = time(NULL);
			dNew->lSock = lSock;

			fservList.push_back(dNew);
			TT_StartThread(FServThread, dNew, _("FServ Thread"), pluginnum);
		} else {
			ut->std_reply(ut, _("Sorry, there are already the maximum number of FServ sessions in progress. Please try again later..."));
		}
		sendMutex.Release();

		return 1;
	}

	if (!stricmp(command, "dccstatus")) {
		ut->std_reply(ut, _("DCC Status"));
		sendMutex.Lock();
		if (dcc_config.enable_chat) {
			sprintf(buf, _("Active DCC Chats: %d"), chatList.size());
			ut->std_reply(ut, buf);
			for (chatListType::const_iterator x = chatList.begin(); x != chatList.end(); x++) {
				api->user->uflag_to_string((*x)->Pres->Flags, buf, sizeof(buf));
				sprintf(buf, _(" %s[%s]: %s, %s:%d, connected for %s"), (*x)->Pres->Nick, buf, (*x)->status == DS_AUTHENTICATED ? _("Logged In"):_("Waiting for login"), (*x)->ip, (*x)->port, api->textfunc->duration(time(NULL) - (*x)->timestamp, buf2));
				ut->std_reply(ut, buf);
			}
		} else {
			ut->std_reply(ut, _("DCC Chats: Disabled"));
		}
		if (dcc_config.enable_fserv) {
			sprintf(buf, _("DCC FServ Sessions: %d/%d"), fservList.size(), dcc_config.maxFServs);
			ut->std_reply(ut, buf);
			for (chatListType::const_iterator x = fservList.begin(); x != fservList.end(); x++) {
				api->user->uflag_to_string((*x)->Pres->Flags, buf, sizeof(buf));
				sprintf(buf, _(" %s[%s]: %s, %s:%d, cwd: /%s, connected/ing for %s"), (*x)->Pres->Nick, buf, (*x)->status >= DS_CONNECTED ? _("Active"):_("Waiting for connection"), (*x)->ip, (*x)->port, (*x)->cwd, api->textfunc->duration(time(NULL) - (*x)->timestamp, buf2));
				ut->std_reply(ut, buf);
			}
		} else {
			ut->std_reply(ut, _("DCC FServ: Disabled"));
		}
		if (dcc_config.enable_recv) {
			sprintf(buf, _("DCC Receives (incoming): %d/%d"), recvList.size(), dcc_config.maxRecvs);
			ut->std_reply(ut, buf);
			for (recvListType::const_iterator x = recvList.begin(); x != recvList.end(); x++) {
				double per = (*x)->pos;
				per /= (*x)->size;
				per *= 100;
				if (per > 100) { per = 100; }
				sprintf(buf, " %s: %s, %s:%d, file: %s, " I64FMT "/" I64FMT " (%.02f%%) received", (*x)->nick, GetStatusText((*x)->status), (*x)->ip, (*x)->port, nopath((*x)->fn), (*x)->pos, (*x)->size, per);
				ut->std_reply(ut, buf);
			}
		} else {
			ut->std_reply(ut, _("DCC Receive (incoming): Disabled"));
		}
		sendMutex.Release();
	}

	if (!stricmp(command, "chatme") && dcc_config.enable_chat && ut->User) {
		if (!strlen(ut->User->Pass)) {
			ut->std_reply(ut, _("ERROR: You need to set a password with !chpass before you can DCC Chat me"));
			return 1;
		}

		T_SOCKET * lSock = BindRandomPort();
		if (lSock == NULL) {
			ut->std_reply(ut, _("All of my DCC ports are in use, please try again later..."));
			return 1;
		}

		DCCT_CHAT * tmp = (DCCT_CHAT *)zmalloc(sizeof(DCCT_CHAT));
		tmp->User = ut->User;
		tmp->Pres = ut;
		tmp->timestamp = time(NULL);
		tmp->lSock = lSock;

		RefUser(ut);
		TT_StartThread(ChatThread_Outgoing,(void *)tmp, _("DCC Chat Thread"), pluginnum);
		return 1;
	}

	return 0;
}

int message_proc(unsigned int msg, char * data, int datalen) {
	if (msg == IB_LOG) {
		sendMutex.Lock();
		for (std::vector<DCCT_CHAT *>::iterator x = chatList.begin(); x != chatList.end(); x++) {
			if ((*x)->status == DS_AUTHENTICATED && ((*x)->Pres->Flags & UFLAG_MASTER)) {
				time_t tme=0;
				time(&tme);
				tm tml;
				localtime_r(&tme, &tml);
				char ib_time[64];
				strftime(ib_time, sizeof(ib_time), _("<%m/%d/%y@%I:%M:%S%p>"), &tml);

				/*
				int n = strlen(ib_time);
				memmove(buf+n+1, buf, strlen(buf)+1);
				strcpy(buf, ib_time);
				buf[strlen(ib_time)]=' ';
				fprintf(config.base.log_fp, "%s", buf)
				*/
				std::string buf = ib_time;
				buf += " ";
				buf += data;
				sockets->Send((*x)->sock, (char *)buf.c_str());
			}
		}
		sendMutex.Release();
	}

	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{CDC70FD8-0588-4f6b-8B33-A3C00A4A4104}",
	"DCC Support",
	"DCC Support (Chat/Transfer) Plugin 1.0",
	"Drift Solutions",
	init,
	quit,
	message_proc,
	privmsgproc,
	NULL,
	NULL,
	NULL,
};

PLUGIN_EXPORT_FULL

T_SOCKET * BindRandomPort() {
	AutoMutex(sendMutex);
	T_SOCKET * sock = sockets->Create();
	int port = clamp<int>(dcc_config.last_random_port + 1, dcc_config.first_port, dcc_config.last_port);

	int loops = 5;
	//sockets->SetReuseAddr(sock, true);
	while (!sockets->Bind(sock, port)) {
		port++;
		if (port > dcc_config.last_port || port > 65535) {
			port = dcc_config.first_port;
			loops--;
			if (loops <= 0) {
				sockets->Close(sock);
				return NULL;
			}
		}
	}

	while (!sockets->Listen(sock)) {
		api->safe_sleep_seconds(1);
	}

	sock->local_port = dcc_config.last_random_port = port;
	return sock;
}

/*
#ifndef HANDLE
	#define HANDLE void *
#endif

typedef struct {
	HANDLE hHandle;
	PLUGIN_PUBLIC * plug;
	char parm[256];
	char name[256];
} PLUGIN;
*/

