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

class Steamcast_Feeder: public AutoDJ_Feeder {
private:
	T_SOCKET * sock;
	bool icecast_mode;
	API_SS sc;
public:
	Steamcast_Feeder();
	~Steamcast_Feeder();
	virtual bool Connect();
	virtual bool IsConnected();
	virtual bool Send(unsigned char * buffer, int32 len);
	virtual bool SendTitleUpdate(const char * title, TITLE_DATA * td);
	virtual void Close();
};

Steamcast_Feeder::Steamcast_Feeder() {
	sock = NULL;
	icecast_mode = false;
}
Steamcast_Feeder::~Steamcast_Feeder() {
	if (sock) {
		Close();
	}
}

bool Steamcast_Feeder::IsConnected() {
	return sock ? true:false;
}

bool Steamcast_Feeder::Connect() {
	char buf[256];
	api->ss->GetSSInfo(0,&sc);
	//printf("AutoDJ -> First SC: %s:%d\n",sc.host,sc.port);
	sock = ad_config.sockets->Create();
	icecast_mode = strchr(ad_config.Server.Pass, ':') ? true:false;
	int port = sc.port;
	if (!icecast_mode) { port++; }
	if (ad_config.sockets->ConnectWithTimeout(sock,ad_config.Server.SourceIP,ad_config.Server.SourcePort ? ad_config.Server.SourcePort:port,10000)) {
		if (icecast_mode) {
			//sprintf(buf,"%s\r\n",ad_config.Server.Pass);
			//ad_config.sockets->Send(sock,buf);
			char buf2[128],buf3[128];
			sstrcpy(buf2,ad_config.Server.Pass);
			base64_encode(buf2,strlen(buf2),buf3);
			sprintf(buf,"SOURCE %s HTTP/1.0\r\nAuthorization: Basic %s\r\n",ad_config.Server.Mount,buf3);
			ad_config.sockets->Send(sock,buf);
		} else {
			sprintf(buf,"%s\r\n",ad_config.Server.Pass);
			ad_config.sockets->Send(sock,buf);
		}
		sprintf(buf,"icy-name:%s\r\n",ad_config.Server.Name);
		ad_config.sockets->Send(sock,buf);
		sprintf(buf,"icy-description:%s\r\n",ad_config.Server.Desc);
		ad_config.sockets->Send(sock,buf);
		sprintf(buf,"icy-url:%s\r\n",ad_config.Server.URL);
		ad_config.sockets->Send(sock,buf);
		sprintf(buf,"icy-genre:%s\r\n",ad_config.Server.Genre);
		ad_config.sockets->Send(sock,buf);
		sprintf(buf,"icy-br:%d\r\n",ad_config.Server.Bitrate);
		ad_config.sockets->Send(sock,buf);
		sprintf(buf,"icy-icq:%s\r\n",ad_config.Server.ICQ);
		ad_config.sockets->Send(sock,buf);
		sprintf(buf,"icy-irc:%s\r\n",ad_config.Server.IRC);
		ad_config.sockets->Send(sock,buf);
		sprintf(buf,"icy-aim:%s\r\n",ad_config.Server.AIM);
		ad_config.sockets->Send(sock,buf);
		sprintf(buf,"icy-pub:%d\r\n",ad_config.Server.Public);
		ad_config.sockets->Send(sock,buf);
		if (ad_config.Server.Reset) {
			ad_config.sockets->Send(sock,"icy-reset: 1\r\n");
		}
		ad_config.sockets->Send(sock,"User-Agent: RadioBot AutoDJ\r\n");
		sprintf(buf,"content-type:%s\r\n\r\n",ad_config.Server.MimeType);
		ad_config.sockets->Send(sock,buf);

		DRIFT_DIGITAL_SIGNATURE();

		int n=0;
		if ((n = ad_config.sockets->Recv(sock,buf,sizeof(buf))) > 0) {
			buf[n]=0;
			if (strstr(buf,"OK")) {
				//ad_config.sockets->SetNonBlocking(sock,true);
				ad_config.sockets->SetSendTimeout(sock, 10000);
				ad_config.sockets->SetRecvTimeout(sock, 10000);
			} else {
				api->ib_printf2(pluginnum,"AutoDJ (ss_steamcast) -> Error connecting to Steamcast: %s\n", buf);
				ad_config.sockets->Close(sock);
				sock = NULL;
				return false;
			}
		} else {
			ad_config.sockets->Close(sock);
			sock = NULL;
			return false;
		}
	} else {
		ad_config.sockets->Close(sock);
		sock = NULL;
		return false;
	}
	return true;
}

bool Steamcast_Feeder::Send(unsigned char * buffer, int32 len) {
	if (sock == NULL) { return false; }
	if (len == 0) { return true; }

	int sret = ad_config.sockets->Send(sock,(char *)buffer,len);
	if (sret == -1) {
		api->ib_printf2(pluginnum,"AutoDJ (ss_steamcast) -> Socket error in feeder! (errno: %d, desc: %s)\n",ad_config.sockets->GetLastError(),ad_config.sockets->GetLastErrorString());
		return false;
	}
	if (sret == 0) {
		api->ib_printf2(pluginnum,"AutoDJ (ss_steamcast) -> Server closed socket! (errno: %d, desc: %s)\n",ad_config.sockets->GetLastError(),ad_config.sockets->GetLastErrorString());
		return false;
	}
	return true;
}

bool Steamcast_Feeder::SendTitleUpdate(const char * title, TITLE_DATA * td) {
	char url[1024];

	if (!icecast_mode) {
		sprintf(url,"GET /admin.cgi?pass=%s&mode=updinfo&song=",ad_config.Server.Pass);
	} else {
		sprintf(url,"GET /admin/metadata?mode=updinfo&mount=%s&song=",ad_config.Server.Mount);
	}

	DRIFT_DIGITAL_SIGNATURE();

	strcat(url,title);
	strcat(url," HTTP/1.0\r\n");

	if (icecast_mode) {
		char url2[512],url3[512];
		sstrcpy(url2, ad_config.Server.Pass);
		base64_encode(url2,strlen(url2),url3);
		sprintf(url2,"Authorization: Basic %s\r\n",url3);
		strcat(url,url2);
	}

	strcat(url,"Connection: close\r\nUser-Agent: RadioBot/Mozilla\r\n\r\n");
	T_SOCKET * sockupd = ad_config.sockets->Create();
	api->ib_printf2(pluginnum,"AutoDJ -> Connecting to SS to send update...\n");
	bool ret = true;
	if (sockupd && ad_config.sockets->ConnectWithTimeout(sockupd,sc.host,sc.port,30000)) {
		ad_config.sockets->SetNonBlocking(sockupd, true);
		ad_config.sockets->Send(sockupd,url,strlen(url));
		if (ad_config.sockets->Select_Read(sockupd, 5000) == 1) {
			memset(url, 0, sizeof(url));
			int n = ad_config.sockets->RecvLine(sockupd, url, sizeof(url) - 1);
			if (n > 0) {
				strtrim(url);
				api->ib_printf2(pluginnum, "AutoDJ -> Reply: %s\n", url);
			}
		}
	} else {
		api->ib_printf2(pluginnum,"AutoDJ -> Error sending title update, could not connect to server...\n");
		ret = false;
	}
	if (sockupd) { ad_config.sockets->Close(sockupd); }
	return ret;
}

void Steamcast_Feeder::Close() {
	if (sock != NULL) {
		ad_config.sockets->Close(sock);
		sock = NULL;
	}
}

AutoDJ_Feeder * steamcast_create() { return new Steamcast_Feeder; }
void steamcast_destroy(AutoDJ_Feeder * feeder) {
	delete dynamic_cast<Steamcast_Feeder *>(feeder);
}
