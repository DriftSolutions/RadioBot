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

bool RecvFileCommon(DCCT_QUEUE * trans, T_SOCKET * sock, char * errbuf, size_t errsize) {
	char buf[1024 * 16];

	sstrcpy(buf, trans->fn);
	int n = 0;
	while (access(buf, 0) == 0 && n <= 1024) {
		n++;
		sprintf(buf, "%s.%03u", trans->fn, n);
	}

	if (n) {
		char buf2[4096];
		sprintf(buf2, "NOTICE %s :File %s already exists, changed incoming filename to: %s\r\n", trans->nick, nopath(trans->fn), nopath(buf));
		api->irc->SendIRC(trans->netno, buf2, strlen(buf2));
		api->ib_printf(_("DCC Transfer -> File %s already exists, changed incoming filename to: %s\n"), nopath(trans->fn), nopath(buf));
	}

	FILE * fp = fopen(buf, "wb");
	if (fp == NULL) {
		strlcpy(errbuf, "Error opening file for output", errsize);
		return false;
	}

	trans->pos = 0;
	union {
		uint32 un;
		uint64 un64;
	};
	un64 = 0;
	trans->status = DS_TRANSFERRING;
	sockets->SetSendTimeout(sock,  30000);
	sockets->SetKeepAlive(sock);
	time_t timeo = time(NULL) + 240;
	while (!dcc_config.shutdown_now) {
		if (sockets->Select_Read(sock, 1000) == 1) {
			n = sockets->Recv(sock, buf, sizeof(buf));
			if (n <= 0) {
				api->ib_printf(_("DCC Transfer -> Socket error while receiving file from %s\n"), trans->nick);
				break;
			}
			if (fwrite(buf, n, 1, fp) != 1) {
				api->ib_printf(_("DCC Transfer -> Error writing data while receiving file from %s (%s)\n"), trans->nick, strerror(errno));
				break;
			}
			timeo = time(NULL) + 240;
			trans->pos += n;
			if (trans->size > UINT_MAX) {
				un64 = Get_UBE64(trans->pos);
				sockets->Send(sock, (char *)&un64, sizeof(un64));
			} else {
				un = htonl(uint32(trans->pos));
				sockets->Send(sock, (char *)&un, sizeof(un));
			}
			if (ftell64(fp) >= trans->size) {
				// file complete
				break;
			}
		} else if (timeo < time(NULL)) {
			api->ib_printf(_("DCC Transfer -> Timed out while receiving file from %s\n"), trans->nick);
			break;
		}
	}

	fclose(fp);

	if (trans->pos >= trans->size) {
		sprintf(buf, "NOTICE %s :I have successfully received your file, thank you...\r\n", trans->nick);
		api->irc->SendIRC(trans->netno, buf, strlen(buf));

		snprintf(buf, sizeof(buf), _("DCC Transfer -> Successfully got file from %s (" I64FMT "/" I64FMT ")"), trans->nick, trans->pos, trans->size);
		api->ib_printf("%s\n", buf);
		api->irc->LogToChan(buf);
		return true;
	} else {
		snprintf(errbuf, errsize, "File Incomplete! (" I64FMT "/" I64FMT ")", trans->pos, trans->size);
		return false;
	}
}


THREADTYPE RecvThread(void *lpData) {
	TT_THREAD_START
	DCCT_QUEUE *trans = (DCCT_QUEUE*)tt->parm;
	api->ib_printf(_("DCC Transfer -> Beginning RecvThread for file from %s (%s:%d)\n"), trans->nick, trans->ip, trans->port);

	char buf[1024];
	char errbuf[512] = { 0 };

	T_SOCKET * sock = sockets->Create();
	if (sock) {
		if (sockets->ConnectWithTimeout(sock,trans->ip,trans->port, 30000)) {
			RecvFileCommon(trans, sock, errbuf, sizeof(errbuf));
		} else {
			sstrcpy(errbuf, _("Connection Refused"));
		}
		sockets->Close(sock);
	} else {
		sstrcpy(errbuf, _("Error creating socket"));
	}

	if (errbuf[0]) {
		snprintf(buf, sizeof(buf), "NOTICE %s :DCC Failed! (%s)\r\n", trans->nick, errbuf);
		api->irc->SendIRC(trans->netno, buf, strlen(buf));
		snprintf(buf, sizeof(buf), _("DCC Transfer -> Failed to get file from %s (%s)"), trans->nick, errbuf);
		api->ib_printf("%s\n", buf);
		api->irc->LogToChan(buf);
	}

	sendMutex.Lock();
	for (std::vector<DCCT_QUEUE *>::iterator x = recvList.begin(); x != recvList.end(); x++) {
		if (*x == trans) {
			recvList.erase(x);
			break;
		}
	}
	sendMutex.Release();

	zfree(trans);
	TT_THREAD_END
}

THREADTYPE RecvPasvThread(void *lpData) {
	TT_THREAD_START
	DCCT_QUEUE *trans = (DCCT_QUEUE*)tt->parm;
	api->ib_printf(_("DCC Transfer -> Beginning RecvPasvThread for file from %s (%s)\n"), trans->nick, trans->ip);

	char buf[1024];
	char errbuf[512] = { 0 };

	T_SOCKET * lSock = BindRandomPort();
	while (lSock == NULL && !dcc_config.shutdown_now) {
		printf("[IRC-%d] Waiting for free DCC socket to receive file from <%s>...\n", trans->netno, trans->nick);
		safe_sleep(2);
		lSock = BindRandomPort();
	}
	if (lSock) {
		trans->port = lSock->local_port;
		sstrcpy(trans->ip, strlen(dcc_config.local_ip) ? dcc_config.local_ip : api->GetIP());
		struct in_addr x;
		inet_pton(AF_INET, trans->ip, &x);
		uint32 longip = ntohl(x.s_addr);
		snprintf(buf, sizeof(buf), "PRIVMSG %s :\001DCC SEND \"%s\" %u %d " I64FMT " %d\001\r\n", trans->nick, nopath(trans->fn), longip, trans->port, trans->size, trans->token);
		api->irc->SendIRC_Priority(trans->netno, buf, strlen(buf), PRIORITY_IMMEDIATE);

#if defined(DEBUG)
		api->ib_printf("%s", buf);
#endif

		api->ib_printf(_("DCC Transfer -> Waiting on %s:%d for file from %s\n"), trans->ip, trans->port, trans->nick);

		int tries = 60;
		bool accepted = false;
		while (!accepted && tries >= 0) {
			if (sockets->Select_Read(lSock, 1000) == 1) {
				T_SOCKET * sock = sockets->Accept(lSock);
				if (sock != NULL) {
					accepted = true;
					RecvFileCommon(trans, sock, errbuf, sizeof(errbuf));
					sockets->Close(sock);
				} else {
					sstrcpy(errbuf, _("Error accepting incoming connection!"));
				}
			}
		}
		if (!accepted) {
			sstrcpy(errbuf, _("Timeout waiting for client connection"));
		}
		sockets->Close(lSock);
	} else {
		sstrcpy(errbuf, _("Error creating listening socket"));
	}

	if (errbuf[0]) {
		snprintf(buf, sizeof(buf), "NOTICE %s :DCC Failed! (%s)\r\n", trans->nick, errbuf);
		api->irc->SendIRC(trans->netno, buf, strlen(buf));
		snprintf(buf, sizeof(buf), _("DCC Transfer -> Failed to get file from %s (%s)"), trans->nick, errbuf);
		api->ib_printf("%s\n", buf);
		api->irc->LogToChan(buf);
	}

	sendMutex.Lock();
	for (std::vector<DCCT_QUEUE *>::iterator x = recvList.begin(); x != recvList.end(); x++) {
		if (*x == trans) {
			recvList.erase(x);
			break;
		}
	}
	sendMutex.Release();

	zfree(trans);
	TT_THREAD_END
}
