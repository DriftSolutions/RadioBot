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

DCCT_QUEUE *SendQueue=NULL, *curSend=NULL;

void AddSend(DCCT_QUEUE * dNew) {
	sendMutex.Lock();
	DCCT_QUEUE * Scan = SendQueue;
	if (Scan == NULL) {
		SendQueue = dNew;
	} else {
		while (Scan->Next != NULL) {
			Scan=Scan->Next;
		}
		Scan->Next=dNew;
	}
	sendMutex.Release();
}

THREADTYPE SendThread(VOID *lpData) {
	TT_THREAD_START
	char buf[256],buf2[4096];
	while (!dcc_config.shutdown_now) {
		sendMutex.Lock();
		if (SendQueue != NULL) {
			curSend=SendQueue;
			SendQueue=SendQueue->Next;
			sendMutex.Release();

			T_SOCKET * sock = BindRandomPort();
			while (sock == NULL && !dcc_config.shutdown_now) {
				printf("[IRC-%d] Waiting for free DCC socket to offer send to <%s>...\n",curSend->netno,curSend->nick);
				safe_sleep(2);
				sock = BindRandomPort();
			}

			if (sock != NULL) {
				curSend->port = sock->local_port;
				sstrcpy(curSend->ip, strlen(dcc_config.local_ip) ? dcc_config.local_ip:api->GetIP());

				//PRIVMSG User2 :DCC SEND filename ipaddress port filesize
				char *fn = nopath(curSend->fn);
				curSend->timestamp=time(NULL);
				//uint32 longip = ntohl(inet_addr(curSend->ip));
				struct in_addr x;
				inet_pton(AF_INET, curSend->ip, &x);
				uint32 longip = ntohl(x.s_addr);
				snprintf(buf, sizeof(buf), "PRIVMSG %s :\001DCC SEND \"%s\" %u %d " I64FMT "\001\r\n", curSend->nick, fn, longip, curSend->port, curSend->size);
				api->ib_printf(_("Send: %s"),buf);
				api->irc->SendIRC_Priority(curSend->netno, buf, strlen(buf), PRIORITY_INTERACTIVE);

				T_SOCKET * cSock;
				//sockaddr addr;
				//socklen_t szAddr = sizeof(addr);
				int n=0;

				if (WaitXSecondsForSock(sock, 90) == 1 && !dcc_config.shutdown_now) { // wait up to 90 seconds for a connection, then abort
					if ((cSock = sockets->Accept(sock)) != NULL) {
						sockets->Close(sock);
						sock = NULL;
						FILE * fp = fopen(curSend->fn, "rb");
						if (fp) {
							int bread = sizeof(buf2);
							int64 left = curSend->size - curSend->pos;
							fseek64(fp, curSend->pos, SEEK_SET);
							while (left > 0 && !dcc_config.shutdown_now) {
								if (left < bread) { bread = left; }
								if (fread(buf2,bread,1,fp) != 1 || (n = sockets->Send(cSock,buf2,bread)) <= 0) {
									break;
								}
								left -= bread;

								if (sockets->Select_Read(cSock, uint32(0)) == 1) {
									//clear out any acks received
									sockets->Recv(cSock, buf, sizeof(buf));
								}
								//api->safe_sleep_milli(100);
							}
						}
						api->safe_sleep_seconds(5);
						sockets->Close(cSock);
					}
					api->safe_sleep_milli(150);
				}

				if (sock) {
					sockets->Close(sock);
				}
			}

			sendMutex.Lock();
			zfree(curSend);
			curSend=NULL;
			sendMutex.Release();
		} else {
			sendMutex.Release();
		}
		api->safe_sleep_seconds(1);
	}
	TT_THREAD_END
}
