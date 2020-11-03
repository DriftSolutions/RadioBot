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


#include "autodj.h"

class Icecast_Feeder: public SimpleDJ_Feeder {
private:
	T_SOCKET * sock;
public:
	Icecast_Feeder();
	virtual ~Icecast_Feeder();
	virtual bool Connect();
	virtual bool IsConnected();
	virtual bool Send(unsigned char * buffer, int32 len);
	virtual bool SendTitleUpdate(const char * title, TITLE_DATA * td);
	virtual void Close();
};

Icecast_Feeder::Icecast_Feeder() {
	sock = NULL;
}
Icecast_Feeder::~Icecast_Feeder() {
	if (sock) {
		Close();
	}
}

bool Icecast_Feeder::IsConnected() {
	return sock ? true:false;
}

bool Icecast_Feeder::Connect() {
		char buf[1024];
		api->ss->GetSSInfo(num,&ss);
		api->ib_printf(_("SimpleDJ [stream-%02d] -> Connecting to icecast server: %s:%d\n"),num,ss.host,ss.port);
		if (ss.type != SS_TYPE_ICECAST) {
			api->ib_printf(_("SimpleDJ [stream-%02d] -> Error: Sound Server is not Icecast!\n"),num);
			return false;
		}
		api->ss->GetSSInfo(num,&ss);
		sock = sockets->Create();
		if (sockets->ConnectWithTimeout(sock,ss.host,ss.port,10000)) {
				char buf2[128],buf3[128];
				sprintf(buf2,"source:%s",sd_config.Servers[num].Pass);
				base64_encode(buf2, strlen(buf2), buf3);
				sprintf(buf,"SOURCE %s HTTP/1.0\r\nAuthorization: Basic %s\r\n",sd_config.Servers[num].Mount,buf3);
				sockets->Send(sock,buf);
				sprintf(buf,"ice-name: %s\r\n",sd_config.Servers[num].Name);
				sockets->Send(sock,buf);
				sprintf(buf,"ice-description: %s\r\n",sd_config.Servers[num].Desc);
				sockets->Send(sock,buf);
				sprintf(buf,"ice-url: %s\r\n",sd_config.Servers[num].URL);
				sockets->Send(sock,buf);
				sprintf(buf,"ice-genre: %s\r\n",sd_config.Servers[num].Genre);
				sockets->Send(sock,buf);
				sprintf(buf,"ice-public: %d\r\n",sd_config.Servers[num].Public);
				sockets->Send(sock,buf);

				sprintf(buf,"ice-audio-info: ice-samplerate=%d;ice-bitrate=%d;ice-channels=%d;ice-url=%s\r\n",sd_config.Servers[num].Sample,sd_config.Servers[num].Bitrate,sd_config.Servers[num].Channels,sd_config.Servers[num].URL);
				sockets->Send(sock,buf);

				sockets->Send(sock,"User-Agent: RadioBot SimpleDJ\r\n");
				sprintf(buf, "Content-Type: %s\r\n\r\n", sd_config.Servers[num].MimeType);
				sockets->Send(sock,buf);

				int n=0;
				if ((n = sockets->Recv(sock,buf,sizeof(buf))) > 0) {
					buf[n]=0;
					if (strstr(buf,"200 OK")) {
						//int timeo=5000; // 5 seconds
						api->ib_printf(_("SimpleDJ [stream-%02d] -> 200 OK, Connected as source...\n"), num, buf);
						sockets->SetSendTimeout(sock, 5000);
						sockets->SetRecvTimeout(sock, 5000);
						//sockets->SetNonBlocking(sock, true);
					} else {
						if ((strstr(buf,"404") || strstr(buf,"403")) && strstr(buf,"in use")) {
							api->ib_printf(_("SimpleDJ [stream-%02d] -> Mountpoint %s already has a source!\n"), num, sd_config.Servers[num].Mount);
						} else {
							api->ib_printf(_("SimpleDJ [stream-%02d] -> Receipt from server: %s\n"), num, buf);
						}
						sockets->Close(sock);
						sock = NULL;
						return false;
					}
				} else {
					api->ib_printf(_("SimpleDJ [stream-%02d] -> Error receiving data from server! (%s)\n"), num, sockets->GetLastErrorString());
					sockets->Close(sock);
					sock = NULL;
					return false;
				}
		} else {
			api->ib_printf(_("SimpleDJ [stream-%02d] -> Error connecting to server! (%s)\n"), num, sockets->GetLastErrorString());
			sockets->Close(sock);
			sock = NULL;
			return false;
		}
		return true;
}

bool Icecast_Feeder::Send(unsigned char * buf, int len) {
	if (sock == NULL) { return false; }
	if (len == 0) { return true; }

	int sret = sockets->Send(sock,(char *)buf,len);
	if (sret == -1) {
		api->ib_printf(_("SimpleDJ [stream-%02d] -> Socket error in feeder! (errno: %d, desc: %s)\n"),num,sockets->GetLastError(),sockets->GetLastErrorString());
		return false;
	}
	if (sret == 0) {
		api->ib_printf(_("SimpleDJ [stream-%02d] -> Server closed socket! (errno: %d, desc: %s)\n"),num,sockets->GetLastError(),sockets->GetLastErrorString());
		return false;
	}
	return true;
}

bool Icecast_Feeder::SendTitleUpdate(const char * title, TITLE_DATA * td) {
	char url[1024];
	sprintf(url,"GET /admin/metadata?mode=updinfo&mount=%s&song=",sd_config.Servers[num].Mount);

	DRIFT_DIGITAL_SIGNATURE();

	strcat(url,title);
	strcat(url," HTTP/1.0\r\n");

	char url2[512],url3[512];
	sprintf(url2,"source:%s",sd_config.Servers[num].Pass);
	base64_encode(url2, strlen(url2), url3);
	sprintf(url2,"Authorization: Basic %s\r\n",url3);
	strcat(url,url2);

	strcat(url,"Connection: close\r\nUser-Agent: RadioBot/Mozilla\r\n\r\n");
	T_SOCKET * sockupd = sockets->Create();
	api->ib_printf(_("SimpleDJ [stream-%02d] -> Connecting to SS to send update...\n"), num);
	bool ret = true;
	if (sockupd && sockets->ConnectWithTimeout(sockupd,ss.host,ss.port,10000)) {
		sockets->SetNonBlocking(sockupd,true);
		api->ib_printf(_("SimpleDJ [stream-%02d] -> Sending title update... (%s)\n"), num, title);
		sockets->Send(sockupd,url,strlen(url));
		api->safe_sleep_seconds(1);
	} else {
		api->ib_printf(_("SimpleDJ [stream-%02d] -> Error sending title update, could not connect to server...\n"), num);
		ret = false;
	}
	if (sockupd) { sockets->Close(sockupd); }
	return ret;
}

void Icecast_Feeder::Close() {
	if (sock != NULL) {
		sockets->Close(sock);
		sock = NULL;
	}
}

SimpleDJ_Feeder * ic_create() { return new Icecast_Feeder; }
void ic_destroy(SimpleDJ_Feeder * feeder) {
	delete dynamic_cast<Icecast_Feeder *>(feeder);
}
