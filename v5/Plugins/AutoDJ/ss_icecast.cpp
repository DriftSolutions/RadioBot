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

class Icecast_Feeder: public AutoDJ_Feeder {
private:
	T_SOCKET * sock;
	API_SS ss;
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
		api->ss->GetSSInfo(0,&ss);
		if (ss.type != SS_TYPE_ICECAST) {
			api->ib_printf2(pluginnum,"AutoDJ -> Error: Sound Server is not Icecast!");
			return false;
		}
		sock = ad_config.sockets->Create();
		api->ib_printf2(pluginnum,"AutoDJ -> Connecting to: %s:%d ...\n",ad_config.Server.SourceIP,ad_config.Server.SourcePort ? ad_config.Server.SourcePort:ss.port);
		if (ad_config.sockets->ConnectWithTimeout(sock,ad_config.Server.SourceIP,ad_config.Server.SourcePort ? ad_config.Server.SourcePort:ss.port,10000)) {
			ad_config.sockets->SetSendTimeout(sock, 5000);
			ad_config.sockets->SetRecvTimeout(sock, 10000);

			char buf2[128], buf3[256];
				/*
				if (strchr(ad_config.Server.Pass, ':')) {
					sstrcpy(buf2,ad_config.Server.Pass);
				} else {
				*/
					snprintf(buf2,sizeof(buf2),"source:%s",ad_config.Server.Pass);
				//}
				base64_encode(buf2,strlen(buf2),buf3);
				snprintf(buf,sizeof(buf),"SOURCE %s HTTP/1.0\r\nAuthorization: Basic %s\r\n",ad_config.Server.Mount,buf3);
				ad_config.sockets->Send(sock,buf);
				snprintf(buf,sizeof(buf),"ice-name: %s\r\n",ad_config.Server.Name);
				ad_config.sockets->Send(sock,buf);
				snprintf(buf,sizeof(buf),"ice-description: %s\r\n",ad_config.Server.Desc);
				ad_config.sockets->Send(sock,buf);
				snprintf(buf,sizeof(buf),"ice-url: %s\r\n",ad_config.Server.URL);
				ad_config.sockets->Send(sock,buf);
				snprintf(buf,sizeof(buf),"ice-genre: %s\r\n",ad_config.Server.Genre);
				ad_config.sockets->Send(sock,buf);
				snprintf(buf,sizeof(buf),"ice-public: %d\r\n",ad_config.Server.Public);
				ad_config.sockets->Send(sock,buf);
				snprintf(buf,sizeof(buf),"ice-bitrate: %d\r\n",ad_config.Server.Bitrate);
				ad_config.sockets->Send(sock,buf);

				snprintf(buf,sizeof(buf),"ice-audio-info: ice-samplerate=%d;ice-bitrate=%d;ice-channels=%d;ice-url=%s\r\n",ad_config.Server.Sample,ad_config.Server.Bitrate,ad_config.Server.Channels,ad_config.Server.URL);
				ad_config.sockets->Send(sock,buf);

				ad_config.sockets->Send(sock,"User-Agent: RadioBot AutoDJ\r\n");
				snprintf(buf,sizeof(buf),"Content-Type: %s\r\n\r\n", ad_config.Server.MimeType);
				ad_config.sockets->Send(sock,buf);

				int n=0;
				if ((n = ad_config.sockets->Recv(sock,buf,sizeof(buf))) > 0) {
					buf[n]=0;
					if (strstr(buf,"200 OK")) {
						//int timeo=5000; // 5 seconds
						api->ib_printf2(pluginnum,"AutoDJ -> 200 OK, Connected as source...\n", buf);
						//ad_config.sockets->SetNonBlocking(sock, true);
					} else {
						if ((strstr(buf,"404") || strstr(buf,"403")) && strstr(buf,"in use")) {
							api->ib_printf2(pluginnum,"AutoDJ -> Mountpoint %s already has a source!\n", ad_config.Server.Mount);
						} else {
							api->ib_printf2(pluginnum,"AutoDJ -> Receipt from server: %s\n", buf);
						}
						ad_config.sockets->Close(sock);
						sock = NULL;
						return false;
					}
				} else {
					api->ib_printf2(pluginnum,"AutoDJ -> Error receiving data from server! (%s)\n", ad_config.sockets->GetLastErrorString());
					ad_config.sockets->Close(sock);
					sock = NULL;
					return false;
				}
		} else {
			api->ib_printf2(pluginnum,"AutoDJ -> Error connecting to server! (%s)\n", ad_config.sockets->GetLastErrorString());
			ad_config.sockets->Close(sock);
			sock = NULL;
			return false;
		}
		return true;
}

bool Icecast_Feeder::Send(unsigned char * buf, int len) {
	if (sock == NULL) { return false; }
	if (len == 0) { return true; }

	int sret = ad_config.sockets->Send(sock,(char *)buf,len);
	if (sret == -1) {
		api->ib_printf2(pluginnum,"AutoDJ (ss_icecast) -> Socket error in feeder! (errno: %d, desc: %s)\n",ad_config.sockets->GetLastError(),ad_config.sockets->GetLastErrorString());
		return false;
	}
	if (sret == 0) {
		api->ib_printf2(pluginnum,"AutoDJ (ss_icecast) -> Server closed socket! (errno: %d, desc: %s)\n",ad_config.sockets->GetLastError(),ad_config.sockets->GetLastErrorString());
		return false;
	}
	return true;
}

bool Icecast_Feeder::SendTitleUpdate(const char * title, TITLE_DATA * td) {

	DRIFT_DIGITAL_SIGNATURE();

	ostringstream surl;
	char url[2048], url2[512];

	urlencode(ad_config.Server.Mount, url, sizeof(url));
	if (url[0] == 0) {
		api->ib_printf("AutoDJ (ss_icecast) -> No 'Mount' specified in AutoDJ/Server! Icecast requires a mountpoint.\n");
		return false;
	}

	surl << "GET /admin/metadata?mode=updinfo&mount=" << url << "&charset=utf8&song=" << title;
	if (td != NULL && td->title[0]) {
		if (td->artist[0]) {
			surl << "&artist=" << urlencode(td->artist, url, sizeof(url));
		}
		surl << "&title=" << urlencode(td->title, url, sizeof(url));
	}
	surl << " HTTP/1.0\r\n";

	snprintf(url,sizeof(url),"source:%s",ad_config.Server.Pass);
	base64_encode(url, strlen(url), url2);
	snprintf(url,sizeof(url),"Authorization: Basic %s\r\n",url2);
	surl << url;

	surl << "Connection: close\r\nUser-Agent: RadioBot/Mozilla\r\n\r\n";
	T_SOCKET * sockupd = ad_config.sockets->Create();
	bool ret = true;
	if (sockupd) {
		api->ib_printf2(pluginnum,"AutoDJ -> Connecting to SS to send update...\n");
		if (sockupd && ad_config.sockets->ConnectWithTimeout(sockupd, ss.host, ss.port, 30000)) {
			ad_config.sockets->SetNonBlocking(sockupd, true);
			ad_config.sockets->Send(sockupd,surl.str().c_str(),surl.str().length());
			if (ad_config.sockets->Select_Read(sockupd, 5000) == 1) {
				memset(url, 0, sizeof(url));
				int n = ad_config.sockets->RecvLine(sockupd, url, sizeof(url) - 1);
				if (n > 0) {
					strtrim(url);
					if (stristr(url, "200 OK")) {
						string reply;
						while (n > 0 && ad_config.sockets->Select_Read(sockupd, 10000) == 1) {
							n = ad_config.sockets->Recv(sockupd, url, sizeof(url) - 1);
							if (n > 0) {
								url[n] = 0;
								reply += url;
							}
						}
						if (reply.length()) {
							if (stristr(reply.c_str(), "Metadata update successful")) {
								api->ib_printf2(pluginnum, "AutoDJ -> Update sent!\n", url);
							} else if (stristr(reply.c_str(), "will not accept URL")) {
								api->ib_printf2(pluginnum, "AutoDJ -> Warning: This mountpoint/format does not accept metadata updates!\n");
							} else {
								api->ib_printf2(pluginnum, "AutoDJ -> Reply: %s\n", reply.c_str());
							}
						} else {
							api->ib_printf2(pluginnum, "AutoDJ -> Update sent!\n", url);
						}
					} else {
						api->ib_printf2(pluginnum, "AutoDJ -> Reply: %s\n", url);
						ret = false;
					}
				}
			}
		} else {
			api->ib_printf2(pluginnum,"AutoDJ -> Error sending title update, could not connect to server...\n");
			ret = false;
		}
		ad_config.sockets->Close(sockupd);
	} else {
		api->ib_printf2(pluginnum,"AutoDJ -> Error creating socket to send title update!\n");
		ret = false;
	}
	return ret;
}

void Icecast_Feeder::Close() {
	if (sock != NULL) {
		ad_config.sockets->Close(sock);
		sock = NULL;
	}
}

AutoDJ_Feeder * ic_create() { return new Icecast_Feeder; }
void ic_destroy(AutoDJ_Feeder * feeder) {
	delete dynamic_cast<Icecast_Feeder *>(feeder);
}
