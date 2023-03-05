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
#include <remote_protobuf.pb.h>
#if defined(WIN32)
	#if defined(DEBUG)
		#pragma comment(lib, "libprotobuf_d.lib")
	#else
		#pragma comment(lib, "libprotobuf.lib")
	#endif
#endif

template <class T>
T ProtobufFromBuffer(const char * buffer, uint32 length) {
	T pb;
	pb.ParseFromArray(buffer, length);
#if defined(DEBUG)
	ib_printf("Incoming protobuf: %s -> %s\n", typeid(T).name(), pb.DebugString());
#endif
	return pb;
}

Titus_Mutex sesMutex;
#if defined(IRCBOT_ENABLE_SS)
uint32 next_search_id = 0;

struct REMOTE_FIND {
	uint32 id;
	time_t timeo;
	USER_PRESENCE * ut;
	int max_results;
	find_finish_type find_finish;
	FIND_RESULTS * res;
};
typedef std::map<uint32, REMOTE_FIND> remoteFindList;
remoteFindList remoteFinds;

void remote_req_find_free(FIND_RESULTS * res) {
	for (int i=0; i < res->num_songs; i++) {
		zfree(res->songs[i].fn);
	}
	zfree(res);
}

void remote_req_timeout(bool all) {
	time_t tme = time(NULL);
	LockMutex(requestMutex);
	for (remoteFindList::iterator x = remoteFinds.begin(); x != remoteFinds.end();) {
		if (all || tme > x->second.timeo) {
			ib_printf("Remote -> Timing out @find search ID %u\n", x->second.id);
			x->second.find_finish(x->second.ut, NULL, x->second.max_results);
			remoteFinds.erase(x);
			x = remoteFinds.begin();
		} else {
			x++;
		}
	}
	RelMutex(requestMutex);
}

void remote_req_find(const char * fn, int max_results, int start, USER_PRESENCE * ut, find_finish_type find_finish) {

	REMOTE_FIND rf;
	memset(&rf, 0, sizeof(rf));
//	rf.res = ret;
	rf.max_results = max_results;
	rf.ut = ut;
	rf.find_finish = find_finish;
	rf.timeo = time(NULL) + 15;

	RemoteFindQuery q;
	q.set_query(fn);

	RefUser(ut);
	LockMutex(requestMutex);
	rf.id = next_search_id++;
	q.set_search_id(rf.id);
	string str = q.SerializeAsString();
	remoteFinds[rf.id] = rf;
	RelMutex(requestMutex);

	REMOTE_HEADER rh;
	rh.scmd = RCMD_FIND_QUERY;
	rh.datalen = str.length();
	SendRemoteReply(config.req_user->Sock, &rh, str.c_str());
}

bool remote_req_request_from_find(USER_PRESENCE * pres, FIND_RESULT * res, char * reply, size_t replySize) {
	///< Called when someone does a !request with a # that matches a \@find result
	return true;
}
bool remote_req_request(USER_PRESENCE * pres, const char * txt, char * reply, size_t replySize) {
	return true;
	///< Called when a !request is done with a filename or # after results have expired
}

struct REQUEST_INTERFACE remote_req_iface = {
	remote_req_find,
	remote_req_request_from_find,
	remote_req_request
};
#endif

void sendGenericError(T_SOCKET * sock, const char * buf) {
	REMOTE_HEADER rHead;
	rHead.scmd=RCMD_GENERIC_ERROR;
	rHead.datalen=strlen(buf);
	SendRemoteReply(sock, &rHead, buf);
}

void sendGenericMessage(T_SOCKET * sock, const char * buf) {
	REMOTE_HEADER rHead;
	rHead.scmd=RCMD_GENERIC_MSG;
	rHead.datalen=strlen(buf);
	SendRemoteReply(sock, &rHead, buf);
}

THREADTYPE remoteThreadStarter(VOID *lpData) {
	TT_THREAD_START

	config.remote.sock=NULL;
	while (config.remote.sock == NULL && !config.shutdown_now) {
		safe_sleep(3);

		ib_printf(_("Remote Control Starter: Attempting Socket Bind...\n"));
		config.remote.sock = config.sockets->Create();
		config.sockets->SetReuseAddr(config.remote.sock, true);
		bool bound = false;
		if (config.base.bind_ip.length()) {
			bound = config.sockets->BindToAddr(config.remote.sock, config.base.bind_ip.c_str(), config.remote.port);
		} else {
			bound = config.sockets->Bind(config.remote.sock, config.remote.port);
		}
		if (!bound) {
			ib_printf(_("Remote Control Starter: Error binding remote socket: %s!\n"), config.sockets->GetLastErrorString(config.remote.sock));
			config.sockets->Close(config.remote.sock);
			config.remote.sock=NULL;
			continue;
		}
		if (!config.sockets->Listen(config.remote.sock)) {
			ib_printf(_("Remote Control Starter: Error making remote socket listen(): %s!\n"), config.sockets->GetLastErrorString(config.remote.sock));
			config.sockets->Close(config.remote.sock);
			config.remote.sock=NULL;
			continue;
		}

		T_SOCKET * sock;
		int n = 0;
		while ((n = config.sockets->Select_Read(config.remote.sock, 1000)) >= 0 && !config.shutdown_now) {
			if (n == 1 && (sock = config.sockets->Accept(config.remote.sock)) != NULL) {
				TT_StartThread(remoteThread, sock, _("Remote Client Thread"));
			}
		}

		config.sockets->Close(config.remote.sock);
		config.remote.sock=NULL;
#if defined(IRCBOT_ENABLE_SS)
		remote_req_timeout(true);
#endif
	}

	ib_printf(_("Remote Control Starter: My Job is Done Here\n"));
	TT_THREAD_END
}

REMOTE_SESSION * RegisterSession(T_SOCKET * sock) {
	LockMutex(sesMutex);
	REMOTE_SESSION * newses = (REMOTE_SESSION *)zmalloc(sizeof(REMOTE_SESSION));
	memset(newses, 0, sizeof(REMOTE_SESSION));
	newses->sock = sock;
	if (config.sesFirst == NULL) {
		config.sesFirst = config.sesLast = newses;
	} else {
		config.sesLast->Next = newses;
		newses->Prev = config.sesLast;
		config.sesLast = newses;
	}
	RelMutex(sesMutex);
	return newses;
}

void DeRegisterSession(REMOTE_SESSION * ses) {
	LockMutex(sesMutex);

	if (config.sesFirst == ses && config.sesLast == ses) {
		config.sesFirst = config.sesLast = NULL;
	} else if (ses == config.sesFirst) {
		config.sesFirst=config.sesFirst->Next;
		config.sesFirst->Prev=NULL;
	} else if (ses == config.sesLast) {
		config.sesLast=config.sesLast->Prev;
		config.sesLast->Next=NULL;
	} else {
		ses->Next->Prev=ses->Prev;
		ses->Prev->Next=ses->Next;
	}

	zfree(ses);
	RelMutex(sesMutex);
}

static const char remote_desc[] = "Remote";

#if !defined(DEBUG)
void remote_up_ref(USER_PRESENCE * u) {
#else
void remote_up_ref(USER_PRESENCE * u, const char * fn, int line) {
	ib_printf("Remote_UserPres_AddRef(): %s : %s:%d\n", u->User->Nick, fn, line);
#endif
	LockMutex(sesMutex);
	RefUser(u->User);
	u->ref_cnt++;
	RelMutex(sesMutex);
}

#if !defined(DEBUG)
void remote_up_unref(USER_PRESENCE * u) {
#else
void remote_up_unref(USER_PRESENCE * u, const char * fn, int line) {
	ib_printf("Remote_UserPres_DelRef(): %s : %s:%d\n", u->Nick, fn, line);
#endif
	LockMutex(sesMutex);
	UnrefUser(u->User);
	u->ref_cnt--;
	#if defined(DEBUG)
	ib_printf("Remote_UserPres_DelRef(): %s : New Refcount %d\n", u->Nick,u->ref_cnt);
	#endif
	if (u->ref_cnt == 0) {
		zfree((void *)u->Nick);
		zfree((void *)u->Hostmask);
		zfree(u);
	}
	RelMutex(sesMutex);
}

bool send_remote_pm(USER_PRESENCE * ut, const char * str) {
	REMOTE_HEADER rHead;
	rHead.scmd = RCMD_GENERIC_MSG;
	rHead.datalen = strlen(str);
	SendRemoteReply(ut->Sock, &rHead, (char *)str);
	return true;
}

USER_PRESENCE * alloc_remote_presence(USER * user, T_SOCKET * sock) {
	USER_PRESENCE * ret = znew(USER_PRESENCE);
	memset(ret, 0, sizeof(USER_PRESENCE));

	ret->User = user;
	ret->Sock = sock;
	ret->Nick = zstrdup(ret->User->Nick);
	ret->Hostmask = zstrdup(ret->User->Hostmasks[0]);
	ret->NetNo = -1;
	ret->Flags = ret->User->Flags;

	ret->send_chan_notice = send_remote_pm;
	ret->send_chan = send_remote_pm;
	ret->send_msg = send_remote_pm;
	ret->send_notice = send_remote_pm;
	ret->std_reply = send_remote_pm;
	ret->Desc = remote_desc;

	ret->ref = remote_up_ref;
	ret->unref = remote_up_unref;

	RefUser(ret);
	return ret;
};

#define HTTP_GET 0x20544547
#define HTTP_POST 0x54534F50

struct REMOTE_UPLOAD {
	bool used;
	char fn[MAX_PATH];
	char hash[41];
	FILE * fp;
	uint32 size;
};
#define MAX_UPLOADS 2

THREADTYPE remoteThread(VOID *lpData) {
	TT_THREAD_START

	//ib_printf(_("Remote Control Thread Started: %d\n"),voidptr2int(tr->parm));
	T_SOCKET * sock = (T_SOCKET *)tt->parm;
	ib_printf(_("Remote -> Incoming connection from %s:%d ...\n"), sock->remote_ip, sock->remote_port);

	REMOTE_HEADER rHead;
	REMOTE_UPLOAD uploads[2];
	char buf[MAX_REMOTE_PACKET_SIZE+1], cli_version=0x10;
	REMOTE_SESSION * ses = RegisterSession(sock);
	USER * User=NULL;
	USER_PRESENCE * UserPres = NULL;
	int n = 0;
	memset(&uploads, 0, sizeof(uploads));

	config.sockets->SetSendTimeout(sock, 3000);
	config.sockets->SetRecvTimeout(sock, 30000);

	while ((n = config.sockets->Select_Read(sock, 1000)) >= 0 && !config.shutdown_now) {
		if (n == 1) {
			if ((n = config.sockets->Recv(sock,(char *)&rHead,sizeof(rHead))) == sizeof(rHead)) {
#ifndef LITTLE_ENDIAN
				rHead.cmd = (REMOTE_COMMANDS)Get_ULE32(rHead.cmd);
				rHead.datalen = Get_ULE32(rHead.datalen);
#endif
				if (rHead.ccmd != RCMD_UPLOAD_DATA) {
					ib_printf(_("Remote -> Incoming Packet: Command: %04X, Data Length: %08X\n"),rHead.ccmd,rHead.datalen);
				}
				if (rHead.ccmd == HTTP_GET || rHead.ccmd == HTTP_POST) {
					while ((n = config.sockets->Select_Read(sock, 2000)) > 0) {
						config.sockets->Recv(sock,buf,sizeof(buf));
					}
					ib_printf(_("Remote -> HTTP Request Detected, sending error reply...\n"));
					snprintf(buf, sizeof(buf), "HTTP/1.0 200 OK\r\nConnection: close\r\n\r\n<h1>ShoutIRC Bot %s</h1>This is not a web server port, it is for use only by clients supporting the <a href=\"http://wiki.shoutirc.com/index.php/Remote_Commands\">Remote Protocol</a>!", GetBotVersionString());
					config.sockets->Send(sock, buf);
					break;
				}
				if (rHead.datalen > MAX_REMOTE_PACKET_SIZE || rHead.ccmd > 0xFF) {
					ib_printf(_("Remote -> Client tried to send too large of packet, dropping connection...\n"));
					break;
				}

				memset(buf,0,sizeof(buf));
				if (rHead.datalen > 0) {
					int left = rHead.datalen;
					int ind = 0;
					while (left > 0) {
						n = config.sockets->Recv(sock,buf+ind,left);
						if (n <= 0) { break; }
						left -= n;
						ind += n;
					}
					if (left > 0) {
						ib_printf(_("Remote -> Error receiving packet, dropping connection...\n"));
						break;
					}
					buf[rHead.datalen]=0;
				}
				if ( (!User || !(User->Flags & UFLAG_REMOTE)) && (rHead.ccmd != RCMD_LOGIN && rHead.ccmd != RCMD_ENABLE_SSL && rHead.ccmd != RCMD_GET_VERSION) ) {
					continue;
				}

				if (rHead.ccmd != RCMD_LOGIN && rHead.ccmd != RCMD_ENABLE_SSL && UserPres) {
					bool plugin_did = false;
					IBM_REMOTE rm;
					rm.cliversion = cli_version;
					rm.data = buf;
					rm.head = &rHead;
					rm.userpres = UserPres;
					if (SendMessage(-1, IB_REMOTE, (char *)&rm, sizeof(rm)) != 0) {
						continue;
					}
					for (int pln=0; pln < MAX_PLUGINS; pln++) {
						if (config.plugins[pln].plug && config.plugins[pln].plug->remote) {
							if (config.plugins[pln].plug->remote(UserPres,cli_version,&rHead,buf) != 0) {
								plugin_did = true;
								break;
							}
						}
					}
					if (plugin_did) {
						continue;
					}
				}

				switch(rHead.ccmd) {
					case RCMD_LOGIN:{
							char *p,*p2=NULL;
							char user[256]={0}, pass[256]={0};

							p=strtok_r(buf,"\xFE",&p2);
							if (p) { sstrcpy(user,p); }
							p=strtok_r(NULL,"\xFE",&p2);
							if (p) { sstrcpy(pass,p); }
							p=strtok_r(NULL,"\xFE",&p2);
							if (p) {
								cli_version = p[0];
							} else {
								cli_version = 0x10;
							}
							ses->cli_version=cli_version;
							p=strtok_r(NULL,"\xFE",&p2);
							if (p) {
								ib_printf(_("Remote -> Login request from %s (%s) using %s with protocol 0x%02X\n"),user,pass,p,cli_version);
							} else {
								ib_printf(_("Remote -> Login request from %s (%s) with protocol 0x%02X\n"),user,pass,cli_version);
							}

							bool allow = false;
							User = GetUserByNick(user);
							if (User && !strcmp(User->Pass, pass) && strlen(User->Pass) && (User->Flags & UFLAG_REMOTE)) {
								ses->user = User;
								UserPres = alloc_remote_presence(User, sock);
//								iUser.nick = User->Nick;
	//							iUser.flags = User->Flags;
								allow=true;
							}
							if (allow) {
								char buf2[64];
								uflag_to_string(User->Flags, buf2, sizeof(buf2));
								ib_printf(_("Remote -> Client Logged In: %s (%s), Client Version: %.2X\n"),ses->user->Nick,buf2,cli_version);
								rHead.scmd = RCMD_LOGIN_OK;
								rHead.datalen=5;
								config.sockets->Send(sock,(char *)&rHead,sizeof(rHead));
								buf[0] = 0;//User->Level;
								*((uint32 *)(&buf[1])) = User->Flags;
								config.sockets->Send(sock,buf,5);
							} else {
								if (User) { UnrefUser(User); User = NULL; }
								rHead.scmd = RCMD_LOGIN_FAILED;
								ib_printf(_("Remote -> Invalid Login!\n"));
								strcpy((char *)&buf+sizeof(rHead),_("That login was incorrect!"));
								rHead.datalen=strlen(_("That login was incorrect!"));
								memcpy(&buf,&rHead,sizeof(rHead));
								config.sockets->Send(sock,buf,sizeof(rHead)+rHead.datalen);
							}
						}
						break;

					case RCMD_ENABLE_SSL:
						if (config.base.ssl_cert.length() && config.sockets->IsEnabled(TS3_FLAG_SSL)) {
							rHead.scmd = RCMD_ENABLE_SSL_ACK;
							rHead.datalen = 0;
							config.sockets->Send(sock,(char *)&rHead,sizeof(rHead));
							config.sockets->SwitchToSSL_Server(sock);
							//X509 * x = config.sockets->GetSSL_Cert(sock);
							//x=x;
						} else {
							rHead.scmd = RCMD_GENERIC_MSG;
							strcpy((char *)&buf+sizeof(rHead),_("SSL Not Enabled"));
							rHead.datalen=strlen(_("SSL Not Enabled"))+1;
							memcpy(&buf,&rHead,sizeof(rHead));
							config.sockets->Send(sock,buf,sizeof(rHead)+rHead.datalen);
						}
						break;

					case RCMD_GET_VERSION:
						rHead.scmd = RCMD_IRCBOT_VERSION;
						*((uint32 *)&buf[sizeof(rHead)]) = GetBotVersion();
						rHead.datalen=4;
						memcpy(&buf,&rHead,sizeof(rHead));
						config.sockets->Send(sock,buf,sizeof(rHead)+rHead.datalen);
						break;

#if defined(IRCBOT_ENABLE_SS)
					case RCMD_QUERY_STREAM:
						STREAM_INFO *si;
						si = (STREAM_INFO *)zmalloc(sizeof(STREAM_INFO));
						rHead.scmd=RCMD_STREAM_INFO;
						rHead.datalen=sizeof(STREAM_INFO);
						strncpy(si->title,config.stats.cursong,64);
						if (!config.stats.online || config.stats.cursong[0] == 0) {
							strcpy(si->title, _("Stream Inactive"));
						}
						char * tmp;
						tmp = GetCurrentDJDisp();
						strncpy(si->dj,tmp,64);
						zfree(tmp);
						si->listeners = Get_ULE32(config.stats.listeners);
						si->max = Get_ULE32(config.stats.maxusers);
						si->peak = Get_ULE32(config.stats.peak);

						SendRemoteReply(sock, &rHead, (const char *)si);
						//config.sockets->Send(sock,(char *)&rHead,sizeof(rHead));
						//config.sockets->Send(sock,(char *)si,sizeof(STREAM_INFO));
						zfree(si);
						break;

					case RCMD_GET_SOUND_SERVER:
						if (rHead.datalen >= 1) {
							uint8 num = *((uint8 *)buf);
							rHead.scmd = RCMD_SOUND_SERVER;
							rHead.datalen = sizeof(SOUND_SERVER);
							memcpy(&buf, &rHead, sizeof(rHead));
							if (GetSSInfo(num, (SOUND_SERVER *)&buf[sizeof(rHead)])) {
								config.sockets->Send(sock, buf, sizeof(rHead) + rHead.datalen);
								continue;
							}
							rHead.scmd = RCMD_GENERIC_MSG;
							strcpy((char *)&buf + sizeof(rHead), _("Invalid server number included!"));
							rHead.datalen = strlen(_("Invalid server number included!")) + 1;
							memcpy(&buf, &rHead, sizeof(rHead));
							config.sockets->Send(sock, buf, sizeof(rHead) + rHead.datalen);
						} else {
							rHead.scmd = RCMD_GENERIC_MSG;
							strcpy((char *)&buf + sizeof(rHead), _("No server number included!"));
							rHead.datalen = strlen(_("No server number included!")) + 1;
							memcpy(&buf, &rHead, sizeof(rHead));
							config.sockets->Send(sock, buf, sizeof(rHead) + rHead.datalen);
						}
						break;

					case RCMD_REQ_LOGOUT: // DJ logout
						LockMutex(requestMutex);
						if ((config.req_user && config.req_user->User == User) || uflag_have_any_of(User->Flags, UFLAG_MASTER|UFLAG_OP)) {
							RequestsOff();
							rHead.scmd=RCMD_REQ_LOGOUT_ACK;
							rHead.datalen=0;
							config.sockets->Send(sock,(char *)&rHead,sizeof(rHead));
						} else {
							sstrcpy(buf,_("Error: you are not the current DJ!"));
							rHead.scmd=RCMD_GENERIC_ERROR;
							rHead.datalen=strlen(buf);
							config.sockets->Send(sock,(char *)&rHead,sizeof(rHead));
							config.sockets->Send(sock,buf,strlen(buf));
						}
						RelMutex(requestMutex);
						break;

					case RCMD_REQ_LOGIN: // DJ login
						if (uflag_have_any_of(User->Flags, UFLAG_DJ)) {
							REQUEST_INTERFACE * iface = NULL;
							if (rHead.datalen >= 1) {
								if (buf[0] & 0x01) {
									iface = &remote_req_iface;
								}
							}
							RequestsOn(UserPres, REQ_MODE_REMOTE, iface);
							rHead.scmd=RCMD_REQ_LOGIN_ACK;
							rHead.datalen=0;
						} else {
							sstrcpy(buf, _("Error: You do not have access to the Request System!"));
							rHead.scmd=RCMD_GENERIC_ERROR;
							rHead.datalen=strlen(buf)+1;
						}
						SendRemoteReply(sock,&rHead,buf);
						break;

					case RCMD_REQ_CURRENT: // get current DJ
						LockMutex(requestMutex);
						if (HAVE_DJ(config.base.req_mode)) {
							if (config.req_user) {
								sstrcpy(buf, config.req_user->User->Nick);
							} else {
#if defined(IRCBOT_ENABLE_IRC)
								sstrcpy(buf, config.base.irc_nick.c_str());
#else
								sstrcpy(buf, "AutoDJ");
#endif
							}
							rHead.scmd=RCMD_GENERIC_MSG;
							rHead.datalen=strlen(buf);
							config.sockets->Send(sock,(char *)&rHead,sizeof(rHead));
							config.sockets->Send(sock,buf,strlen(buf));
						} else {
							sstrcpy(buf,_("Error: there is no current DJ!"));
							rHead.scmd=RCMD_GENERIC_ERROR;
							rHead.datalen=strlen(buf);
							config.sockets->Send(sock,(char *)&rHead,sizeof(rHead));
							config.sockets->Send(sock,buf,strlen(buf));
						}
						RelMutex(requestMutex);
						break;

					case RCMD_SEND_REQ: // Send Request Broadcast
#if defined(IRCBOT_ENABLE_SS) && defined(IRCBOT_ENABLE_IRC)
						rHead.scmd=RCMD_GENERIC_MSG;
						LockMutex(requestMutex);
						if (config.base.req_mode != REQ_MODE_OFF && config.req_user && config.req_user == UserPres && strlen(buf)) {
							char buf2[2048];
							if (!LoadMessage("ReqToChans", buf2, sizeof(buf2))) {
								rHead.scmd=RCMD_GENERIC_ERROR;
								sstrcpy(buf,_("Error loading request message!"));
							} else {
								if (config.dospam) {
									str_replaceA(buf2,sizeof(buf2),"%request",buf);
									ProcText(buf2,sizeof(buf2));
									BroadcastMsg(UserPres, buf2);
								}
								sstrcpy(buf,_("Request Message Sent!"));
							}
						} else {
							sstrcpy(buf,_("You are not the current DJ, or No Dedication Specified!"));
						}
						RelMutex(requestMutex);
#else
						rHead.scmd=RCMD_GENERIC_ERROR;
						sstrcpy(buf,_("This function isn't supported by the standalone AutoDJ"));
#endif
						rHead.datalen=strlen(buf);
						config.sockets->Send(sock,(char *)&rHead,sizeof(rHead));
						config.sockets->Send(sock,buf,strlen(buf));
						break;
					case RCMD_REQ: // Add A Request
						rHead.scmd=RCMD_GENERIC_MSG;
						if (!strempty(buf)) {
							LockMutex(requestMutex);
							if (!DeliverRequest(UserPres, buf, buf, sizeof(buf))) {
								rHead.scmd = RCMD_GENERIC_ERROR;
							}
							ProcText(buf,sizeof(buf));
							str_replaceA(buf, sizeof(buf), "%nick", User->Nick);
							RelMutex(requestMutex);
						} else {
							rHead.scmd = RCMD_GENERIC_ERROR;
							sstrcpy(buf,_("No request specified!"));
						}
						rHead.datalen=strlen(buf);
						SendRemoteReply(sock, &rHead, buf);
						break;
					case RCMD_SEND_DED: // Send Dedicated To
#if defined(IRCBOT_ENABLE_SS) && defined(IRCBOT_ENABLE_IRC)
						rHead.scmd=RCMD_GENERIC_MSG;
						LockMutex(requestMutex);
						if (config.base.req_mode != REQ_MODE_OFF && config.req_user == UserPres && strlen(buf)) {
							char buf2[2048];
							if (!LoadMessage("DedToChans", buf2, sizeof(buf2))) {
								rHead.scmd=RCMD_GENERIC_ERROR;
								sstrcpy(buf,_("Error loading request message!"));
							} else {
								if (config.dospam) {
									str_replaceA(buf2,sizeof(buf2),"%request",buf);
									ProcText(buf2,sizeof(buf2));
									BroadcastMsg(UserPres, buf2);
								}
								sstrcpy(buf,_("Dedicated To Message Sent!"));
							}
						} else {
							sstrcpy(buf,_("You are not the current DJ, or No Dedication Specified!"));
						}
						RelMutex(requestMutex);
#else
						rHead.scmd=RCMD_GENERIC_ERROR;
						sstrcpy(buf,_("This function isn't supported by the standalone AutoDJ"));
#endif
						rHead.datalen=strlen(buf);
						config.sockets->Send(sock,(char *)&rHead,sizeof(rHead));
						config.sockets->Send(sock,buf,strlen(buf));
						break;

					case RCMD_FIND_RESULTS:{
							LockMutex(requestMutex);
							if (config.base.req_mode != REQ_MODE_OFF && config.req_user == UserPres && rHead.datalen > 0) {
								RemoteFindResult fr = ProtobufFromBuffer<RemoteFindResult>(buf, rHead.datalen);
								if (fr.IsInitialized()) {
									REMOTE_FIND * f = NULL;
									remoteFindList::iterator x = remoteFinds.find(fr.search_id());
									if (x != remoteFinds.end()) {
										f = &x->second;
									}
									if (f != NULL) {
										f->res = znew(FIND_RESULTS);
										memset(f->res, 0, sizeof(FIND_RESULTS));
										f->res->free = remote_req_find_free;
										f->res->num_songs = (f->max_results < fr.files_size()) ? f->max_results:fr.files_size();
										if (fr.files_size() > f->max_results) {
											f->res->have_more = true;
										}
										for (int i=0; i < f->res->num_songs; i++) {
											f->res->songs[i].fn = zstrdup(fr.files(i).file().c_str());
											f->res->songs[i].id = fr.files(i).file_id();
										}

										f->find_finish(f->ut, f->res, f->max_results);
										UnrefUser(f->ut);
										remoteFinds.erase(x);
									}
								} else {
									ib_printf("Remote -> Error parsing RemoteFindResult! (debug: %s)\n", fr.DebugString().c_str());
								}
								RelMutex(requestMutex);
							} else {
								RelMutex(requestMutex);
								sstrcpy(buf,_("You are not the current DJ!"));
								rHead.scmd=RCMD_GENERIC_MSG;
								rHead.datalen=strlen(buf);
								SendRemoteReply(sock, &rHead, buf);
							}
						}
						break;
		#endif

					case RCMD_DOSPAM: // DoSpam
#if defined(IRCBOT_ENABLE_SS) && defined(IRCBOT_ENABLE_IRC)
						if (uflag_have_any_of(User->Flags, UFLAG_MASTER|UFLAG_OP|UFLAG_HOP)) {
							config.dospam = config.dospam ? false:true;
							sprintf(buf,_("Spamming is now: %s"),config.dospam ? _("On"):_("Off"));
						} else {
							sstrcpy(buf,_("Sorry, you are not a high enough level to do that!"));
						}
						rHead.scmd=RCMD_GENERIC_MSG;
#else
						rHead.scmd=RCMD_GENERIC_ERROR;
						sstrcpy(buf,_("This function isn't supported by the standalone AutoDJ"));
#endif
						rHead.datalen=strlen(buf);
						SendRemoteReply(sock, &rHead, buf);
						break;

					case RCMD_DIE: // Die
						if (uflag_have_any_of(User->Flags, UFLAG_MASTER|UFLAG_DIE)) {
							std::ostringstream str;
#if defined(IRCBOT_ENABLE_IRC)
							sprintf(buf, _("Shut down ordered by %s on Remote Link"), User->Nick);
							config.base.quitmsg = str.str();
#endif
							config.shutdown_now=true;
							sstrcpy(buf,_("Shutting down now..."));
						} else {
							sstrcpy(buf,_("Sorry, you are not a high enough level to do that!"));
						}
						rHead.scmd=RCMD_GENERIC_MSG;
						rHead.datalen=strlen(buf);
						config.sockets->Send(sock,(char *)&rHead,sizeof(rHead));
						config.sockets->Send(sock,buf,strlen(buf));
						break;

					case RCMD_BROADCAST_MSG: // Broadcast Message
#if defined(IRCBOT_ENABLE_IRC)
						if (uflag_have_any_of(User->Flags, UFLAG_MASTER|UFLAG_OP|UFLAG_HOP)) {
							if (config.dospam && strlen(buf) > 0) {
								ProcText(buf,sizeof(buf));
								BroadcastMsg(UserPres, buf);
								sstrcpy(buf,_("Broadcast Sent!"));
							} else {
								sstrcpy(buf,_("Spamming is currently off, or no message specified!"));
							}
						} else {
							sstrcpy(buf,_("Sorry, you are not a high enough level to do that!"));
						}
						rHead.scmd=RCMD_GENERIC_MSG;
#else
						rHead.scmd=RCMD_GENERIC_ERROR;
						sstrcpy(buf,_("This function isn't supported by the standalone AutoDJ"));
#endif
						rHead.datalen=strlen(buf);
						config.sockets->Send(sock,(char *)&rHead,sizeof(rHead));
						config.sockets->Send(sock,buf,strlen(buf));
						break;

					case RCMD_RESTART: // restart
						if (uflag_have_any_of(User->Flags, UFLAG_MASTER|UFLAG_DIE)) {
							std::ostringstream str;
#if defined(IRCBOT_ENABLE_IRC)
							sprintf(buf, _("Restart ordered by %s on Remote Link"), User->Nick);
							config.base.quitmsg = str.str();
#endif
							config.shutdown_reboot=true;
							config.shutdown_now=true;
							sstrcpy(buf,_("Restarting now..."));
						} else {
							sstrcpy(buf,_("Sorry, you are not a high enough level to do that!"));
						}
						rHead.scmd=RCMD_GENERIC_MSG;
						rHead.datalen=strlen(buf);
						config.sockets->Send(sock,(char *)&rHead,sizeof(rHead));
						config.sockets->Send(sock,buf,strlen(buf));
						break;

					case RCMD_REHASH: // rehash
						if (uflag_have_any_of(User->Flags, UFLAG_MASTER|UFLAG_OP)) {
							snprintf(buf,sizeof(buf),_("Reloading %s ..."), IRCBOT_TEXT);
							config.rehash = true;
						} else {
							sstrcpy(buf,_("Sorry, you are not a high enough level to do that!"));
						}
						rHead.scmd=RCMD_GENERIC_MSG;
						rHead.datalen=strlen(buf);
						config.sockets->Send(sock,(char *)&rHead,sizeof(rHead));
						config.sockets->Send(sock,buf,strlen(buf));
						break;

					case RCMD_RCONS_OPEN: // open remote console
					case RCMD_RCONS_CLOSE: // close remote console
						sstrcpy(buf,_("That feature only existed in IRCBot v2, please use the DCC console instead"));
						rHead.scmd=RCMD_GENERIC_MSG;
						rHead.datalen=strlen(buf);
						config.sockets->Send(sock,(char *)&rHead,sizeof(rHead));
						config.sockets->Send(sock,buf,strlen(buf));
						break;

					case RCMD_SRC_COUNTDOWN: // Count me In
					case RCMD_SRC_FORCE_OFF:
					case RCMD_SRC_FORCE_ON:
					case RCMD_SRC_NEXT:
					case RCMD_SRC_RELOAD:
					case RCMD_SRC_RELAY:
					case RCMD_SRC_GET_SONG:
					case RCMD_SRC_GET_SONG_INFO:
					case RCMD_SRC_RATE_SONG:
					case RCMD_SRC_STATUS:
					case RCMD_SRC_GET_NAME:
		#ifndef IRCBOT_LITE
						sstrcpy(buf,_("No Source Plugin Active or Current Source Does Not Support That Command"));
		#else
						sstrcpy(buf,_("IRCBot Lite does not support source commands!"));
		#endif
						rHead.scmd=RCMD_GENERIC_ERROR;
						rHead.datalen=strlen(buf);
						config.sockets->Send(sock,(char *)&rHead,sizeof(rHead));
						config.sockets->Send(sock,buf,strlen(buf));
						break;

					case RCMD_GETUSERINFO:
						if (User->Flags & UFLAG_USER_RECORDS) {
							USER * pUser = NULL;
							if (strchr(buf, '!') || strchr(buf, '@')) {
								pUser = GetUser(buf);
							} else {
								pUser = GetUserByNick(buf);
							}
							if (pUser) {
								IBM_USEREXTENDED * ue = (IBM_USEREXTENDED *)&buf;
								UserToExtended(pUser, ue);
								rHead.scmd=RCMD_USERINFO;
								rHead.datalen=sizeof(IBM_USEREXTENDED);
								UnrefUser(pUser);
							} else {
								rHead.scmd=RCMD_USERNOTFOUND;
								rHead.datalen=0;
							}
						} else {
							sstrcpy(buf, _("Sorry, you do not have the user records flag set on your account (+u)!"));
							rHead.scmd=RCMD_GENERIC_MSG;
							rHead.datalen=strlen(buf);
						}
						config.sockets->Send(sock,(char *)&rHead,sizeof(rHead));
						if (rHead.datalen) {
							config.sockets->Send(sock,buf,rHead.datalen);
						}
						break;

					case RCMD_UPLOAD_START:{
							if (User->Flags & UFLAG_DCC_XFER) {
								if (config.remote.uploads_enabled && strlen(config.remote.upload_path)) {
									uint8 ind = 255;
									for (int i=0; i < MAX_UPLOADS; i++) {
										if (!uploads[i].used) {
											ind = i;
											break;
										}
									}
									if (ind != 255) {
										if (access(config.remote.upload_path, 0) != 0) {
											dsl_mkdir(config.remote.upload_path, 0750);
										}
										//sstrcat(
										REMOTE_UPLOAD_START * rus = (REMOTE_UPLOAD_START *)buf;
										if (rHead.datalen >= sizeof(REMOTE_UPLOAD_START)) {
											if (strchr(rus->filename, '/') == NULL && strchr(rus->filename, '\\') == NULL) {
												if (rus->size > 0) {
													char buf2[MAX_PATH];
													snprintf(buf2, sizeof(buf2), "%s%c%s", config.remote.upload_path, PATH_SEP, User->Nick);
													if (access(buf2, 0) != 0) {
														dsl_mkdir(buf2, 0750);
													}
													sstrcat(buf2, PATH_SEPS);
													sstrcat(buf2, rus->filename);
													FILE * fp = fopen(buf2, "wb");
													if (fp != NULL) {
														uploads[ind].used = true;
														uploads[ind].size = rus->size;
														uploads[ind].fp = fp;
														sstrcpy(uploads[ind].fn, buf2);
														memcpy(uploads[ind].hash, rus->hash, sizeof(rus->hash));
														rHead.scmd = RCMD_UPLOAD_OK;
														rHead.datalen = 1;
														buf[0] = ind;
														SendRemoteReply(sock, &rHead, buf);
													} else {
														ib_printf(_("%s: Error opening file '%s' for remote file upload from %s: %s\n"), IRCBOT_NAME, buf2, User->Nick, strerror(errno));
														sstrcpy(buf, "Error opening file for write!");
														sendGenericError(sock, buf);
													}
												} else {
													sstrcpy(buf, "File size must be greater than 0");
													sendGenericError(sock, buf);
												}
											} else {
												sstrcpy(buf, "Filename cannot contain a path (/ or \\)");
												sendGenericError(sock, buf);
											}
										} else {
											sstrcpy(buf, "Not enough data sent.");
											sendGenericError(sock, buf);
										}
									} else {
										sstrcpy(buf, "All of your upload slots are already in use.");
										sendGenericError(sock, buf);
									}
								} else {
									sstrcpy(buf, "Uploads not enabled or no upload path set.");
									sendGenericError(sock, buf);
								}
							} else {
								sstrcpy(buf, "You need the +g flag to upload files.");
								sendGenericError(sock, buf);
							}
						}
						break;

					case RCMD_UPLOAD_DATA:{
							REMOTE_UPLOAD_DATA * rud = (REMOTE_UPLOAD_DATA *)buf;
							rHead.scmd = RCMD_UPLOAD_DATA_ACK;
							if (rHead.datalen > sizeof(REMOTE_UPLOAD_DATA)) {
								if (rud->xferid < MAX_UPLOADS && uploads[rud->xferid].used) {
									if (rud->offset < uploads[rud->xferid].size) {
										fseek(uploads[rud->xferid].fp, rud->offset, SEEK_SET);
										if (fwrite(&buf[sizeof(REMOTE_UPLOAD_DATA)], rHead.datalen - sizeof(REMOTE_UPLOAD_DATA), 1, uploads[rud->xferid].fp) == 1) {
											rHead.datalen = sizeof(REMOTE_UPLOAD_DATA);
											SendRemoteReply(sock, &rHead, buf);
										} else {
											rud->offset = 0xFFFFFFFF;
											strcpy(&buf[sizeof(REMOTE_UPLOAD_DATA)], "Error writing file data!");
											rHead.datalen = sizeof(REMOTE_UPLOAD_DATA) + strlen(&buf[sizeof(REMOTE_UPLOAD_DATA)]) + 1;
											SendRemoteReply(sock, &rHead, buf);
										}
									} else {
										rud->offset = 0xFFFFFFFF;
										strcpy(&buf[sizeof(REMOTE_UPLOAD_DATA)], "Invalid file offset!");
										rHead.datalen = sizeof(REMOTE_UPLOAD_DATA) + strlen(&buf[sizeof(REMOTE_UPLOAD_DATA)]) + 1;
										SendRemoteReply(sock, &rHead, buf);
									}
								} else {
									rud->offset = 0xFFFFFFFF;
									strcpy(&buf[sizeof(REMOTE_UPLOAD_DATA)], "Invalid upload ID!");
									rHead.datalen = sizeof(REMOTE_UPLOAD_DATA) + strlen(&buf[sizeof(REMOTE_UPLOAD_DATA)]) + 1;
									SendRemoteReply(sock, &rHead, buf);
								}
							} else {
								rud->offset = 0xFFFFFFFF;
								strcpy(&buf[sizeof(REMOTE_UPLOAD_DATA)], "Not enough data sent");
								rHead.datalen = sizeof(REMOTE_UPLOAD_DATA) + strlen(&buf[sizeof(REMOTE_UPLOAD_DATA)]) + 1;
								SendRemoteReply(sock, &rHead, buf);
							}
						}
						break;

					case RCMD_UPLOAD_DONE:{
							if (rHead.datalen > 0) {
								uint8 ind = (uint8)buf[0];
								if (ind < MAX_UPLOADS && uploads[ind].used) {
									fclose(uploads[ind].fp);
									uploads[ind].fp = NULL;
									rHead.scmd = RCMD_UPLOAD_DONE_ACK;
									if (hashfile("sha1", uploads[ind].fn, buf, sizeof(buf)) && !stricmp(buf, uploads[ind].hash)) {
										rHead.datalen = 1;
										buf[0] = 0;
										SendRemoteReply(sock, &rHead, buf);
									} else {
										buf[0] = 1;
										strcpy(&buf[1], "Hash does not match!");
										rHead.datalen = 2 + strlen(&buf[1]);
										SendRemoteReply(sock, &rHead, buf);
										unlink(uploads[ind].fn);
									}
									memset(&uploads[ind], 0, sizeof(uploads[ind]));
								} else {
									sstrcpy(buf, "Invalid upload ID");
									sendGenericError(sock, buf);
								}
							} else {
								sstrcpy(buf, "Not enough data sent.");
								sendGenericError(sock, buf);
							}
						}
						break;

					default:
						rHead.scmd = RCMD_GENERIC_ERROR;
						sstrcpy(buf,_("Error, unknown command code!"));
						rHead.datalen=strlen(buf);
						config.sockets->Send(sock,(char *)&rHead,sizeof(rHead));
						config.sockets->Send(sock,buf,strlen(buf));
						break;
				}
			} else {
				//printf("break: %d\n", n);
				break;
			}
		}
	}

	//printf("select_read: %d\n", n);
	//sock=sock;

#if defined(IRCBOT_ENABLE_SS)
	LockMutex(requestMutex);
	if (config.base.req_mode == REQ_MODE_REMOTE && config.req_user && config.req_user == UserPres) {
		RequestsOff();
	}
	RelMutex(requestMutex);
#endif

	if (UserPres) { UnrefUser(UserPres); }
	if (User) { UnrefUser(User); }

	DeRegisterSession(ses);
	config.sockets->Close(sock);

	for (int i=0; i < MAX_UPLOADS; i++) {
		if (uploads[i].used) {
			ib_printf(_("%s: Upload in progress '%s' aborted.\n"), IRCBOT_NAME, nopath(uploads[i].fn));
			fclose(uploads[i].fp);
			unlink(uploads[i].fn);
			uploads[i].used = false;
		}
	}

	ib_printf(_("Remote Control Thread Ended: %d\n"),voidptr2int(tt->parm));
	TT_THREAD_END
}

void SendRemoteReply(T_SOCKET * sock, REMOTE_HEADER * rHead, const char * data) {
	LockMutex(sesMutex);
	if (config.sockets->IsKnownSocket(sock)) {
		if (rHead->datalen > 0 && data != NULL) {
			int len = sizeof(REMOTE_HEADER) + rHead->datalen;
			char * buf = (char *)zmalloc(len);
			memcpy(buf, rHead, sizeof(REMOTE_HEADER));
			memcpy(buf + sizeof(REMOTE_HEADER), data, rHead->datalen);
			config.sockets->Send(sock,(char *)buf,len);
			zfree(buf);
		} else {
			config.sockets->Send(sock,(char *)rHead,sizeof(REMOTE_HEADER));
		}
	}
	RelMutex(sesMutex);
}
