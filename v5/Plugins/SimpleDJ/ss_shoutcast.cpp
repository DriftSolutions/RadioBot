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

class SC_Feeder: public SimpleDJ_Feeder {
private:
	T_SOCKET * sock;
public:
	SC_Feeder();
	~SC_Feeder();
	virtual bool Connect();
	virtual bool IsConnected();
	virtual bool Send(unsigned char * buffer, int32 len);
	virtual bool SendTitleUpdate(const char * title, TITLE_DATA * td);
	virtual void Close();
};

SimpleDJ_Feeder::SimpleDJ_Feeder() {
}
SimpleDJ_Feeder::~SimpleDJ_Feeder() {
}

SC_Feeder::SC_Feeder() {
	sock = NULL;
}
SC_Feeder::~SC_Feeder() {
	if (sock) {
		Close();
	}
}

bool SC_Feeder::IsConnected() {
	return sock ? true:false;
}

bool SC_Feeder::Connect() {
	char buf[256];
	api->ss->GetSSInfo(num,&ss);
	//printf("SimpleDJ -> First SC: %s:%d\n",sc.host,sc.port);
	sock = sockets->Create();
	if (sockets->ConnectWithTimeout(sock,ss.host,ss.port+1,10000)) {
		sprintf(buf,"%s\r\n",sd_config.Servers[num].Pass);
		sockets->Send(sock,buf);
		sprintf(buf,"icy-name:%s\r\n",sd_config.Servers[num].Name);
		sockets->Send(sock,buf);
		sprintf(buf,"icy-description:%s\r\n",sd_config.Servers[num].Desc);
		sockets->Send(sock,buf);
		sprintf(buf,"icy-url:%s\r\n",sd_config.Servers[num].URL);
		sockets->Send(sock,buf);
		sprintf(buf,"icy-genre:%s\r\n",sd_config.Servers[num].Genre);
		sockets->Send(sock,buf);
		sprintf(buf,"icy-br:%d\r\n",sd_config.Servers[num].Bitrate);
		sockets->Send(sock,buf);
		sprintf(buf,"icy-icq:%s\r\n",sd_config.Servers[num].ICQ);
		sockets->Send(sock,buf);
		sprintf(buf,"icy-irc:%s\r\n",sd_config.Servers[num].IRC);
		sockets->Send(sock,buf);
		sprintf(buf,"icy-aim:%s\r\n",sd_config.Servers[num].AIM);
		sockets->Send(sock,buf);
		sprintf(buf,"icy-pub:%d\r\n",sd_config.Servers[num].Public);
		sockets->Send(sock,buf);
		if (sd_config.Servers[num].Reset) {
			sockets->Send(sock,"icy-reset: 1\r\n");
		}
		sockets->Send(sock,"User-Agent: RadioBot SimpleDJ\r\n");
		sprintf(buf,"content-type:%s\r\n\r\n",sd_config.Servers[num].MimeType);
		sockets->Send(sock,buf);

		DRIFT_DIGITAL_SIGNATURE();

		int n=0;
		if ((n = sockets->Recv(sock,buf,sizeof(buf))) > 0) {
			buf[n]=0;
			if (strstr(buf,"OK")) {
				//sockets->SetNonBlocking(sock,true);
				sockets->SetSendTimeout(sock, 10000);
				sockets->SetRecvTimeout(sock, 10000);
			} else {
				api->ib_printf(_("SimpleDJ [stream-%02d] -> Error connecting to SHOUTCast: %s\n"), num, buf);
				sockets->Close(sock);
				sock = NULL;
				return false;
			}
		} else {
			sockets->Close(sock);
			sock = NULL;
			return false;
		}
	} else {
		sockets->Close(sock);
		sock = NULL;
		return false;
	}
	return true;
}

bool SC_Feeder::Send(unsigned char * buffer, int32 len) {
	if (sock == NULL) { return false; }
	if (len == 0) { return true; }

	int sret = sockets->Send(sock,(char *)buffer,len);
	if (sret == -1) {
		api->ib_printf(_("SimpleDJ [stream-%02d] -> Socket error in feeder! (errno: %d, desc: %s)\n"), num,sockets->GetLastError(),sockets->GetLastErrorString());
		return false;
	}
	if (sret == 0) {
		api->ib_printf(_("SimpleDJ [stream-%02d] -> Server closed socket! (errno: %d, desc: %s)\n"), num,sockets->GetLastError(),sockets->GetLastErrorString());
		return false;
	}
	return true;
}

bool SC_Feeder::SendTitleUpdate(const char * title, TITLE_DATA * td) {
	char url[1024];
	sprintf(url,"GET /admin.cgi?pass=%s&mode=updinfo&song=",sd_config.Servers[num].Pass);

	DRIFT_DIGITAL_SIGNATURE();

	strcat(url, title);
	strcat(url," HTTP/1.0\r\n");

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

void SC_Feeder::Close() {
	if (sock != NULL) {
		sockets->Close(sock);
		sock = NULL;
	}
}

SimpleDJ_Feeder * sc_create() { return new SC_Feeder; }
void sc_destroy(SimpleDJ_Feeder * feeder) {
	delete dynamic_cast<SC_Feeder *>(feeder);
}
