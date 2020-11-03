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

class SC_Feeder: public AutoDJ_Feeder {
private:
	T_SOCKET * sock;
	API_SS sc;
public:
	SC_Feeder();
	~SC_Feeder();
	virtual bool Connect();
	virtual bool IsConnected();
	virtual bool Send(unsigned char * buffer, int32 len);
	virtual bool SendTitleUpdate(const char * title, TITLE_DATA * td);
	virtual void Close();
};

AutoDJ_Feeder::AutoDJ_Feeder() {
}
AutoDJ_Feeder::~AutoDJ_Feeder() {
}
bool AutoDJ_Feeder::SendTitleUpdate(const char * title, TITLE_DATA * td) { return false; }

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
	api->ss->GetSSInfo(0,&sc);
	//printf("AutoDJ -> First SC: %s:%d\n",sc.host,sc.port);
	sock = ad_config.sockets->Create();
	//if (api->GetB
	if (ad_config.sockets->ConnectWithTimeout(sock,ad_config.Server.SourceIP,ad_config.Server.SourcePort ? ad_config.Server.SourcePort:sc.port+1,10000)) {
		ad_config.sockets->SetSendTimeout(sock, 10000);
		ad_config.sockets->SetRecvTimeout(sock, 10000);

		sprintf(buf,"%s\r\n",ad_config.Server.Pass);
		ad_config.sockets->Send(sock,buf);
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
			} else {
				api->ib_printf2(pluginnum,"AutoDJ (ss_shoutcast) -> Error connecting to SHOUTCast: %s\n", buf);
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
		api->ib_printf2(pluginnum,"AutoDJ (ss_shoutcast) -> Error connecting to SHOUTCast: connection attempt timed out (is the server running?)\n");
		ad_config.sockets->Close(sock);
		sock = NULL;
		return false;
	}
	return true;
}

bool SC_Feeder::Send(unsigned char * buffer, int32 len) {
	if (sock == NULL) { return false; }
	if (len == 0) { return true; }

	int sret = 0;
	int tries = 0;
	while (tries++ < 3) {
		sret = ad_config.sockets->Send(sock,(char *)buffer,len);
		if (sret == -1 && (ad_config.sockets->GetLastError() == EAGAIN || ad_config.sockets->GetLastError() == EWOULDBLOCK)) {
			api->ib_printf2(pluginnum, "AutoDJ (ss_shoutcast) -> Timeout sending data to SHOUTCast: try %d ...\n", tries);
			safe_sleep(500, true);
			continue;
		}
		break;
	}
	if (sret == -1) {
		api->ib_printf2(pluginnum,"AutoDJ (ss_shoutcast) -> Socket error in feeder! (errno: %d, desc: %s)\n",ad_config.sockets->GetLastError(),ad_config.sockets->GetLastErrorString());
		return false;
	}
	if (sret == 0) {
		api->ib_printf2(pluginnum,"AutoDJ (ss_shoutcast) -> Server closed socket! (errno: %d, desc: %s)\n",ad_config.sockets->GetLastError(),ad_config.sockets->GetLastErrorString());
		return false;
	}
	return true;
}

bool SC_Feeder::SendTitleUpdate(const char * title, TITLE_DATA * td) {
	char url[1024];
	ostringstream surl;

	DRIFT_DIGITAL_SIGNATURE();

	surl << "GET /admin.cgi?pass=" << urlencode(ad_config.Server.Pass, url, sizeof(url)) << "&mode=updinfo&song=" << title;

	if (td && td->url[0]) {
		surl << "&url=" << urlencode(td->url, url, sizeof(url));
	}
	surl << " HTTP/1.0\r\n";
	surl << "Connection: close\r\nUser-Agent: RadioBot/Mozilla\r\n\r\n";

	T_SOCKET * sockupd = ad_config.sockets->Create();
	api->ib_printf2(pluginnum, "AutoDJ -> Connecting to SS to send update...\n");
	bool ret = true;
	if (sockupd) {
		if (ad_config.sockets->ConnectWithTimeout(sockupd, sc.host, sc.port, 30000)) {
			ad_config.sockets->SetNonBlocking(sockupd, true);
			ad_config.sockets->Send(sockupd, surl.str().c_str(), surl.str().length());
			if (ad_config.sockets->Select_Read(sockupd, 5000) == 1) {
				memset(url, 0, sizeof(url));
				int n = ad_config.sockets->RecvLine(sockupd, url, sizeof(url) - 1);
				if (n > 0) {
					strtrim(url);
					if (stristr(url, "200 OK")) {
						api->ib_printf2(pluginnum, "AutoDJ -> Update sent!\n", url);
					} else {
						api->ib_printf2(pluginnum, "AutoDJ -> Reply: %s\n", url);
						ret = false;
					}
				}
			}
		} else {
			api->ib_printf2(pluginnum, "AutoDJ -> Error sending title update, could not connect to server...\n");
			ret = false;
		}
		ad_config.sockets->Close(sockupd);
	} else {
		api->ib_printf2(pluginnum, "AutoDJ -> Error creating socket to send title update!\n");
		ret = false;
	}
	return ret;
}

void SC_Feeder::Close() {
	if (sock != NULL) {
		ad_config.sockets->Close(sock);
		sock = NULL;
	}
}

AutoDJ_Feeder * sc_create() { return new SC_Feeder; }
void sc_destroy(AutoDJ_Feeder * feeder) {
	delete dynamic_cast<SC_Feeder *>(feeder);
}
