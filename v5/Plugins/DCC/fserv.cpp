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

void DoPWD(T_SOCKET * sock, DCCT_CHAT * trans) {
	string str = _("Current directory is: /");
	str += trans->cwd;
	CommandOutput_DCC(sock, "", (char *)str.c_str());
}

int WaitXSecondsForSock(T_SOCKET * sock, int secs) {
	while (secs > 0 && !dcc_config.shutdown_now) {
		int n = sockets->Select_Read(sock, 1000);
		if (n != 0) { return n; }
		secs--;
	}
	return 0;
}

char * bytes(uint64 num, char * buf) {
	#define KB 1024
	#define MB 1048576
	#define GB 1073741824LL
	#define TB 1099511627776LL
	if (num >= TB) {
		uint64 w = num / TB;
		num -= (w * TB);
		double x = num;
		x /= TB;
		x += w;
		sprintf(buf, "%.2fTB", x);
		return buf;
	}
	if (num >= GB) {
		uint64 w = num / GB;
		num -= (w * GB);
		double x = num;
		x /= GB;
		x += w;
		sprintf(buf, "%.2fGB", x);
		return buf;
	}
	if (num >= MB) {
		double x = num;
		x /= MB;
		sprintf(buf, "%.2fMB", x);
		return buf;
	}
	if (num >= KB) {
		double x = num;
		x /= KB;
		sprintf(buf, "%.2fKB", x);
		return buf;
	}

	sprintf(buf, "" U64FMT "B", num);
	return buf;
}

THREADTYPE FServThread(VOID *lpData) {
	TT_THREAD_START
	DCCT_CHAT * trans = (DCCT_CHAT*)tt->parm;
	char buf[1024],buf2[128],local_ip[128];
	trans->status = DS_CONNECTING;

	sstrcpy(local_ip, strlen(dcc_config.local_ip) ? dcc_config.local_ip:api->GetIP());

	api->user->uflag_to_string(trans->Pres->Flags, buf2, sizeof(buf2));
	snprintf(buf,sizeof(buf),_("[IRC-%d] Offering DCC FServ to <%s[%s]>..."),trans->Pres->NetNo,trans->Pres->Nick,buf2);
	api->irc->LogToChan(buf);
	api->ib_printf("%s\n",buf);

	snprintf(buf, sizeof(buf), "NOTICE %s :DCC Fserve (%s)\r\n", trans->Pres->Nick, local_ip);
	api->irc->SendIRC(trans->Pres->NetNo, buf, strlen(buf));
//	uint32 longip = ntohl(inet_addr(local_ip));
	struct in_addr xx;
	inet_pton(AF_INET, local_ip, &xx);
	uint32 longip = ntohl(xx.s_addr);
	snprintf(buf, sizeof(buf), "PRIVMSG %s :\001DCC CHAT chat %u %d\001\r\n", trans->Pres->Nick, longip, trans->lSock->local_port);
	api->irc->SendIRC(trans->Pres->NetNo, buf, strlen(buf));

	if (WaitXSecondsForSock(trans->lSock, 60*3) == 1) {
		T_SOCKET * sock = sockets->Accept(trans->lSock);
		if (sock) {

			api->user->uflag_to_string(trans->Pres->Flags, buf2, sizeof(buf2));
			sprintf(buf,_("[IRC-%d] <%s[%s]> connected to FServ..."),trans->Pres->NetNo,trans->Pres->Nick,buf2);
			api->irc->LogToChan(buf);
			api->ib_printf(_("[IRC-%d] <%s[%s]> connected to FServ...\n"),trans->Pres->NetNo,trans->Pres->Nick,buf2);
			sockets->Close(trans->lSock);
			trans->lSock = NULL;

			sstrcpy(trans->ip, sock->remote_ip);
			trans->port = sock->remote_port;
			trans->status = DS_CONNECTED;

			time_t tme=0;
			time(&tme);
			tm tml;
			localtime_r(&tme, &tml);
			char ib_time[64];
			strftime(ib_time, sizeof(ib_time), _("%I:%M:%S %p"), &tml);

			CommandOutput_DCC(sock,"","");
			CommandOutput_DCC(sock,""," ## #            #  ### ##   ##     ##       #  ");
			CommandOutput_DCC(sock,"","#   ### ### # # ###  #  # # #       # # ### ### ");
			CommandOutput_DCC(sock,""," #  # # # # # #  #   #  ##  #       ##  # #  #  ");
			CommandOutput_DCC(sock,"","  # # # ### ###  ##  #  # # #       # # ###  ## ");
			CommandOutput_DCC(sock,"","##                  ### # #  ##     ##          ");
			CommandOutput_DCC(sock,"","");

			api->user->uflag_to_string(trans->Pres->Flags, buf2, sizeof(buf2));
			sprintf(buf,_("Welcome %s, your user flags are '%s'!\r\nMy local time is: %s\r\n"),trans->Pres->Nick,buf2,ib_time);
			CommandOutput_DCC(sock,"",buf);
			CommandOutput_DCC(sock,"",_("Type \002commands\002 for a command list"));

			int n=0;
			while (trans->status >= DS_CONNECTED && (n = sockets->RecvLine(sock, buf, sizeof(buf))) >= RL3_NOLINE && !dcc_config.shutdown_now) {
				if (n == RL3_NOLINE) {
					safe_sleep(100, true);
					continue;
				}

				StrTokenizer st(buf, ' ');
				string cmd = st.stdGetSingleTok(1);
				if (!stricmp(cmd.c_str(), "commands")) {
					CommandOutput_DCC(sock,"", _("FServ Commands"));
					CommandOutput_DCC(sock,"", _("cd [dir] - Change directory, if no param it will print your current directory."));
					CommandOutput_DCC(sock,"", _("\tNote: You can use cd .. to go up a directory or cd / to return to the root directory"));
					CommandOutput_DCC(sock,"", _("pwd - Print current directory"));
					CommandOutput_DCC(sock,"", _("ls [pattern] - Print directory listing, optionally matching wildcard pattern"));
					CommandOutput_DCC(sock,"", _("dir - Alias of ls"));
					CommandOutput_DCC(sock,"", _("get filename - Request a file be sent to you"));
					if (trans->Pres->Flags & UFLAG_DCC_ADMIN) {
						CommandOutput_DCC(sock,"", _("You also have access to the following administrative commands:"));
						CommandOutput_DCC(sock,"", _("mkdir dir - Create a new directory"));
					}
				} else if (!stricmp(cmd.c_str(), "cd")) {
					if (st.NumTok() > 1) {
						char * nd = st.GetTok(2,st.NumTok());
						if (!strcmp(nd, "..")) {
							if (strlen(trans->cwd)) {
								char * p = strrchr(trans->cwd, '/');
								if (p) {
									//2 or more levels from root
									p[0]=0;
								} else {
									//one dir from root
									trans->cwd[0]=0;
								}
							}
							DoPWD(sock, trans);
						} else if (!strcmp(nd, "/")) {
							trans->cwd[0]=0;
							DoPWD(sock, trans);
						} else if (strstr(nd, "..") || strstr(nd, "/") || strstr(nd, "\\") || strstr(nd, "~")) {
							CommandOutput_DCC(sock,"", _("You can not have .. / \\ or ~ in your path name."));
						} else {
							string test = trans->cwd;
							if (strlen(trans->cwd)) {
								test += "/";
							}
							test += nd;
							if (nd[0] == '/') {
								if (nd[strlen(nd)-1] == '/') {
									nd[strlen(nd)-1] = 0;
								}
								test = (nd+1);
							}
							if (PHYSFS_isDirectory(test.c_str())) {
								sstrcpy(trans->cwd, test.c_str());
								DoPWD(sock, trans);
							} else {
								CommandOutput_DCC(sock,"", _("That is not a valid directory!"));
							}
						}
						st.FreeString(nd);
					} else {
						DoPWD(sock, trans);
					}
				} else if (!stricmp(cmd.c_str(), "pwd")) {
					DoPWD(sock, trans);
				} else if (!stricmp(cmd.c_str(), "mkdir") && (trans->Pres->Flags & UFLAG_DCC_ADMIN)) {
					if (st.NumTok() > 1) {
						char * nd = st.GetTok(2,st.NumTok());
						string newdir = trans->cwd;
						if (strlen(trans->cwd)) {
							newdir += "/";;
						}
						newdir += nd;
						if (nd[0] == '/') {
							if (nd[strlen(nd)-1] == '/') {
								nd[strlen(nd)-1] = 0;
							}
							newdir = (nd+1);
						}
						if (PHYSFS_mkdir(newdir.c_str())) {
							CommandOutput_DCC(sock,"", _("Directory created."));
						} else {
							sprintf(buf, _("Error creating directory: %s"), PHYSFS_getLastError());
							CommandOutput_DCC(sock,"", buf);
						}
						st.FreeString(nd);
					} else {
						CommandOutput_DCC(sock,"", _("Usage: mkdir newdirname"));
					}
				} else if (!stricmp(cmd.c_str(), "ls") || !stricmp(cmd.c_str(), "dir")) {
					char * pat = NULL;
					if (st.NumTok() > 1) {
						pat = st.GetTok(2, st.NumTok());
					}
					sprintf(buf, _("Directory listing for /%s:"), trans->cwd);
					CommandOutput_DCC(sock,"", buf);
					uint32 num_files=0, num_dirs=0;
					char ** rc = PHYSFS_enumerateFiles(trans->cwd);
					for (char ** i = rc; *i != NULL; i++) {
						if (pat == NULL || wildicmp(pat, *i)) {
							string ffn = trans->cwd;
							ffn += "/";
							ffn += *i;
							sstrcpy(buf, *i);
							strlcat(buf, " ", sizeof(buf));
							while (strlen(buf) < 60) { strcat(buf, " "); }
							PHYSFS_MyStat st;
							if (PHYSFS_myStat(ffn.c_str(), &st) == 0) {
								if (st.st.filetype == PHYSFS_FILETYPE_DIRECTORY) {
									strlcat(buf, _("<DIR>"), sizeof(buf));
									num_dirs++;
								} else {
									strlcat(buf, bytes(st.st.filesize, buf2), sizeof(buf));
									num_files++;
								}
							} else {
								snprintf(buf, sizeof(buf), _("Error getting information on file: %s"), *i);
							}
							CommandOutput_DCC(sock,"", buf);
						}
					}
					PHYSFS_freeList(rc);
					sprintf(buf, _("End of list. (%u files, %u directories)"), num_files, num_dirs);
					CommandOutput_DCC(sock,"", buf);
					if (pat) { st.FreeString(pat); }
				} else if (!stricmp(cmd.c_str(), "get")) {
					char * nd = st.GetTok(2,st.NumTok());
					if (strstr(nd, "..") || strstr(nd, "/") || strstr(nd, "\\") || strstr(nd, "~")) {
						CommandOutput_DCC(sock,"", _("You can not have .. / \\ or ~ in your filename."));
					} else if (st.NumTok() < 2) {
						CommandOutput_DCC(sock,"", _("Usage: get filename"));
					} else {
						string ffn = trans->cwd;
						ffn += "/";
						ffn += st.stdGetTok(2, st.NumTok());
						PHYSFS_MyStat st;
						if (PHYSFS_myStat(ffn.c_str(), &st) == 0 && (st.st.filetype != PHYSFS_FILETYPE_DIRECTORY) && strlen(st.real_fn)) {
							DCCT_QUEUE *dNew = (DCCT_QUEUE *)zmalloc(sizeof(DCCT_QUEUE));
							memset(dNew, 0, sizeof(DCCT_QUEUE));

							dNew->size = st.st.filesize;
							dNew->netno = trans->Pres->NetNo;
							sstrcpy(dNew->nick, trans->Pres->Nick);
							sstrcpy(dNew->fn, st.real_fn);
							CommandOutput_DCC(sock,"", _("Your file has been put into the SendQueue..."));

							AddSend(dNew);
						} else {
							CommandOutput_DCC(sock,"", _("Invalid filename."));
						}
					}
					st.FreeString(nd);
				} else if (!stricmp(cmd.c_str(), "bye") || !stricmp(cmd.c_str(), "exit") || !stricmp(cmd.c_str(), "quit")) {
					break;
				} else {
					CommandOutput_DCC(sock,"",_("Unrecoginized command."));
				}
			}

			sockets->Close(sock);
		} else {
			api->ib_printf(_("Error accepting connection for %s to connect to FServ!\n"), trans->Pres->Nick);
		}
	} else {
		api->ib_printf(_("Timeout waiting for %s to connect to FServ!\n"), trans->Pres->Nick);
	}
	if (trans->lSock) { sockets->Close(trans->lSock); trans->lSock = NULL; }

	sendMutex.Lock();
	std::vector<DCCT_CHAT *>::iterator x = fservList.begin();
	for (; x != fservList.end(); x++) {
		if (*x == trans) {
			fservList.erase(x);
			break;
		}
	}
	sendMutex.Release();

	UnrefUser(trans->Pres);
	zfree(trans);
	TT_THREAD_END
}
