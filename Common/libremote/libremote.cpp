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


#include "libremote.h"

RemoteClient::RemoteClient() {
	hMutex = new Titus_Mutex();
	socks = new Titus_Sockets();
	host = user = pass = NULL;
	sock = NULL;
	port = 10001;
	strcpy(client_name, "libremote/" LIBREMOTE_VERSION "");
	rprintf = (rprintftype)printf;
	ready = ssl = false;
	ircbotv5 = false;
	uflags = 0;
	uptr = NULL;
	socks->Silent(true);
	socks->EnableSSL("client.pem", TS3_SSL_METHOD_TLS);
}
RemoteClient::~RemoteClient() {
	this->Disconnect();
	delete socks;
	socks = NULL;
	delete hMutex;
	hMutex = NULL;
}

bool RemoteClient::Connect(const char * phost, int32 pport, const char * puser, const char * ppass, int use_ssl) {
	Disconnect();
	if (!puser || !ppass || !phost) { return false; }
	bool ret = false;
	AutoMutexPtr(hMutex);

	zfreenn(host);
	zfreenn(user);
	zfreenn(pass);
	host = zstrdup(phost);
	user = zstrdup(puser);
	pass = zstrdup(ppass);
	port = pport ? pport:10001;
	//ssl = (use_ssl < 2) ? true:false;
	ssl_enabled = false;
	ulevel = 5;
	ircbotv5 = false;
	uflags = 0;


	sock = socks->Create();
	if (socks->ConnectWithTimeout(sock, host, port, 5000)) {
		socks->SetRecvTimeout(sock, 30000);
		socks->SetSendTimeout(sock, 15000);
		char buf[1024];
		REMOTE_HEADER rHead;
		memset(&rHead, 0, sizeof(rHead));
		if (use_ssl < 2) {
			if (socks->IsEnabled(TS3_FLAG_SSL)) {
				rHead.ccmd = RCMD_ENABLE_SSL;
				rprintf(uptr, "Attempting to enable SSL...\n");
				if (SendPacketAwaitResponse(&rHead, buf, sizeof(buf))) {
					if (rHead.scmd == RCMD_ENABLE_SSL_ACK) {
						if (socks->SwitchToSSL_Client(sock)) {
							ssl_enabled = true;
							rprintf(uptr, "SSL Enabled!\n");
						} else {
							rprintf(uptr, "Error: %s\n", socks->GetLastErrorString());
						}
					}
				}
			}
			if (!ssl_enabled && use_ssl == 1) {
				rprintf(uptr, "Error: Could not connect with SSL!\n");
				Disconnect();
				return false;
			}
		}
		rHead.ccmd = RCMD_LOGIN;
		sprintf(buf,"%s\xFE%s\xFE%c\xFE%s", user, pass, PROTO_VERSION, client_name);
		rHead.datalen = uint32(strlen(buf));
		if (SendPacketAwaitResponse(&rHead, buf, sizeof(buf))) {
			if (rHead.scmd == RCMD_LOGIN_OK) {
				rprintf(uptr, "Logged in successfully!\n");
				ulevel = (buf[0] & 0xFF);
				if (ulevel == 0) {
					if (rHead.datalen >= 5) {
						//RadioBot v5 here
						ircbotv5 = true;
						uflags = *((uint32 *)&buf[1]);
					} else {
						rprintf(uptr, "RadioBot v5 but no user flags sent!\n");
						Disconnect();
						return false;
					}
				}
				ready = ret = true;
			} else if (rHead.scmd == RCMD_LOGIN_FAILED) {
				rprintf(uptr, "Error logging in to RadioBot! %s\n", buf);
			} else {
				rprintf(uptr, "Error logging in to RadioBot! Check your username/password...\n");
			}
		}
	} else {
		if (socks->GetLastError()) {
			rprintf(uptr, "Error connecting to RadioBot: %s\n", socks->GetLastErrorString());
		} else {
			rprintf(uptr, "Error connecting to RadioBot: Connection refused\n");
		}
	}

	if (!ret) {
		Disconnect();
	}
	return ret;
}

bool RemoteClient::IsReady() {
	return (sock != NULL && ready);
}

void RemoteClient::Disconnect() {
	AutoMutexPtr(hMutex);
	ready = false;
	ssl_enabled = false;
	if (host) { zfree(host); host=NULL; }
	if (user) { zfree(user); user=NULL; }
	if (pass) { zfree(pass); pass=NULL; }
	if (sock) {
		socks->Close(sock);
		sock = NULL;
	}
}

bool RemoteClient::SendPacket(REMOTE_HEADER * rHead, const char * buf) {
	if (rHead->datalen > MAX_REMOTE_PACKET_SIZE) {
		rprintf(uptr, "Error sending data to RadioBot: packet too large\n");
		return false;
	}
	AutoMutexPtr(hMutex);
	if (sock == NULL) {
		rprintf(uptr, "We are not connected to RadioBot!\n");
		return false;
	}
	if (socks->Select_Write(sock, 15000) != 1) {
		rprintf(uptr, "Timeout sending data to RadioBot: %s\n", socks->GetLastErrorString());
		return false;
	}
	bool ret = false;
	if (socks->Send(sock, (char *)rHead, sizeof(REMOTE_HEADER)) == sizeof(REMOTE_HEADER)) {
		if (rHead->datalen) {
			if (socks->Send(sock, buf, rHead->datalen) == (int)rHead->datalen) {
				ret = true;
			} else {
				rprintf(uptr, "Error sending data to RadioBot: %s\n", socks->GetLastErrorString());
			}
		} else {
			ret = true;
		}
	} else {
		rprintf(uptr, "Error sending data to RadioBot: %s\n", socks->GetLastErrorString());
	}
	return ret;
}

bool RemoteClient::RecvPacket(REMOTE_HEADER * rHead, char * buf, uint32 bufSize) {
	if (!sock) { return false; }
	bool ret = false;
	AutoMutexPtr(hMutex);
	if (buf && bufSize) {
		memset(buf, 0, bufSize);
	}
	if (socks->Select_Read(sock, 30000) != 1) {
		rprintf(uptr, "Timeout receiving data from RadioBot: %s\n", socks->GetLastErrorString());
		return false;
	}
	if (socks->Recv(sock, (char *)rHead, sizeof(REMOTE_HEADER)) == sizeof(REMOTE_HEADER)) {
		if (rHead->datalen) {
			if (buf != NULL && rHead->datalen <= bufSize) {
				int left = rHead->datalen;
				int ind = 0;
				while (left > 0) {
					int n = socks->Recv(sock,buf+ind,left);
					if (n <= 0) { break; }
					left -= n;
					ind += n;
				}
				if (left == 0) {
					ret= true;
				} else {
					rprintf(uptr, "Error receiving data from RadioBot: %s\n", socks->GetLastErrorString());
				}
			} else {
				rprintf(uptr, "Error receiving data from RadioBot: supplied buffer is too small\n");
			}
		} else {
			ret = true;
		}
	} else {
		rprintf(uptr, "Error receiving data from RadioBot: %s\n", socks->GetLastErrorString());
	}
	return ret;
}

bool RemoteClient::SendPacketAwaitResponse(REMOTE_HEADER * rHead, char * buf, uint32 bufSize) {
	bool ret = false;
	hMutex->Lock();
	if (SendPacket(rHead, buf)) {
		if (socks->Select_Read(this->sock, 30000) == 1 && RecvPacket(rHead, buf, bufSize)) {
			ret = true;
		}
	}
	hMutex->Release();
	return ret;
}

void RemoteClient::SetClientName(const char * new_name) {
	strlcpy(client_name, new_name, sizeof(client_name));
}

int RemoteClient::WaitForData(timeval * timeo) {
	AutoMutexPtr(hMutex);
	if (!sock || !ready) {
		return -1;
	}
	return socks->Select_Read(sock, timeo);
}

int RemoteClient::WaitForData(int32 timeout_ms) {
	AutoMutexPtr(hMutex);
	if (!sock || !ready) {
		return -1;
	}
	return socks->Select_Read(sock, timeout_ms);
}

bool RemoteClient::ConnectedWithSSL() {
	return (sock && ready && ssl_enabled) ? true:false;
}

void RemoteClient::SetOutput(rprintftype pOutput) {
	hMutex->Lock();
	rprintf = pOutput;
	hMutex->Release();
}

void RemoteClient::SetUserPtr(void * uuptr) {
	uptr = uuptr;
}

int RemoteClient::GetUserLevel() {
	return ulevel;
}

uint32 RemoteClient::GetUserFlags() {
	return uflags;
}

bool RemoteClient::IsIRCBotv5() {
	return ircbotv5;
}
