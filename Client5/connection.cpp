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

#include "client.h"

VOID CALLBACK BotFree(HWND hwnd, UINT uMsg, ULONG_PTR dwData, LRESULT lResult) {
	if (uMsg == WM_USER) {
		zfree((void *)dwData);
	}
}
VOID CALLBACK BotFreeReply(HWND hwnd, UINT uMsg, ULONG_PTR dwData, LRESULT lResult) {
	if (uMsg == WM_USER) {
		BOT_REPLY * br = (BOT_REPLY *)dwData;
		zfreenn(br->data);
		zfree(br);
	}
}

int BotPrintf(void * uptr, const char * fmt, ...) {
	va_list va;
	va_start(va, fmt);
	char * buf = zvmprintf(fmt, va);
	va_end(va);
	//char * buf = zstrdup(fmt);
	SendMessageCallback(config.mWnd, WM_USER, 0xDEADBEEF, (LPARAM)buf, BotFree, (ULONG_PTR)buf);
	return 1;
}

int QueueBotReply(REMOTE_HEADER * head, const char * data) {
	BOT_REPLY * br = znew(BOT_REPLY);
	memset(br, 0, sizeof(BOT_REPLY));
	br->rHead = *head;
	if (head->datalen > 0) {
		br->data = (char *)zmalloc(head->datalen + 1);
		memcpy(br->data, data, head->datalen);
		br->data[head->datalen] = 0;
	} else {
		br->data = NULL;
	}
	SendMessageCallback(config.mWnd, WM_USER, 0xDEADBEBE, (LPARAM)br, BotFreeReply, (ULONG_PTR)br);
	return 1;
}

void SendPacket(REMOTE_HEADER * rHead, const char * data) {
	hostMutex.Lock();
	if (rc->IsReady()) {
		rc->SendPacket(rHead, data);
	}
	hostMutex.Release();
}

void SendPacket(REMOTE_COMMANDS_C2S cmd, uint32 datalen, const char * data) {
	REMOTE_HEADER r;
	r.ccmd = cmd;
	r.datalen = datalen;
	SendPacket(&r, data);
}

void SendPacket(REMOTE_COMMANDS_S2C cmd, uint32 datalen, const char * data) {
	REMOTE_HEADER r;
	r.scmd = cmd;
	r.datalen = datalen;
	SendPacket(&r, data);
}

TT_DEFINE_THREAD(ConxThread) {
	TT_THREAD_START
	int curind = -1;
	rc->SetOutput(BotPrintf);
	rc->SetClientName("ShoutIRC.com DJ Client " VERSION " (libremote/" LIBREMOTE_VERSION ")");
	REMOTE_HEADER rHead;
	time_t lastSU = 0;
	char buf[4096];
	while (!config.shutdown_now) {
		if (config.connect) {
			hostMutex.Lock();
			if (config.host_index != curind) {
				PostMessage(config.mWnd, WM_USER, 0xDEADBEEB, -1);
				rc->Disconnect();
			}
			curind = config.host_index;
			hostMutex.Release();

			if (!rc->IsReady()) {
				PostMessage(config.mWnd, WM_USER, 0xDEADBEEB, 1);
				if (rc->Connect(config.hosts[config.host_index].host, config.hosts[config.host_index].port, config.hosts[config.host_index].username, config.hosts[config.host_index].pass, config.hosts[config.host_index].mode)) {
					if (rc->IsIRCBotv5()) {
						PostMessage(config.mWnd, WM_USER, 0xDEADBEEB, 2);
						lastSU = 0;
					} else {
						BotPrintf(NULL, "This version of the DJ Client only supports RadioBot v5!");
						rc->Disconnect();
						PostMessage(config.mWnd, WM_USER, 0xDEADBEEB, 0);
						config.connect = false;
					}
				} else {
					PostMessage(config.mWnd, WM_USER, 0xDEADBEEB, 0);
					config.connect = false;
				}
			} else {
				if ((time(NULL) - lastSU) > 30) {
					lastSU = time(NULL);
					rHead.ccmd = RCMD_QUERY_STREAM;
					rHead.datalen = 0;
					SendPacket(&rHead, NULL);
				}
				if (rc->WaitForData(100) == 1) {
					if (rc->RecvPacket(&rHead, buf, sizeof(buf))) {
						QueueBotReply(&rHead, buf);
					} else {
						rc->Disconnect();
						PostMessage(config.mWnd, WM_USER, 0xDEADBEEB, 0);
						config.connect = false;
					}
				}
			}
		} else if (rc->IsReady()) {
			PostMessage(config.mWnd, WM_USER, 0xDEADBEEB, -1);
			rc->Disconnect();
		} else {
			safe_sleep(100, true);
		}
	}
	TT_THREAD_END
}
