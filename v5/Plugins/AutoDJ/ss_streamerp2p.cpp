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

class SP2P_Feeder: public AutoDJ_Feeder {
private:
	T_SOCKET * sock;
public:
	SP2P_Feeder();
	~SP2P_Feeder();
	virtual bool Connect();
	virtual bool IsConnected();
	virtual bool Send(unsigned char * buffer, int32 len);
	virtual bool SendTitleUpdate(const char * title, TITLE_DATA * td);
	virtual void Close();
};

SP2P_Feeder::SP2P_Feeder() {
	sock = NULL;
}
SP2P_Feeder::~SP2P_Feeder() {
	if (sock) {
		Close();
	}
}

bool SP2P_Feeder::IsConnected() {
	return sock ? true:false;
}

bool SP2P_Feeder::Connect() {
	char buf[256];
	API_SS sc;
	api->ss->GetSSInfo(0,&sc);
	sock = ad_config.sockets->Create();
	if (ad_config.sockets->ConnectWithTimeout(sock,ad_config.Server.SourceIP,ad_config.Server.SourcePort ? ad_config.Server.SourcePort:sc.port,10000)) {
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
				ad_config.sockets->SetSendTimeout(sock, 10000);
				ad_config.sockets->SetRecvTimeout(sock, 10000);
			} else {
				api->ib_printf2(pluginnum,"AutoDJ (ss_streamerp2p) -> Error connecting to SHOUTCast: %s\n", buf);
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

bool SP2P_Feeder::Send(unsigned char * buffer, int32 len) {
	if (sock == NULL) { return false; }
	if (len == 0) { return true; }

	int sret = ad_config.sockets->Send(sock,(char *)buffer,len);
	if (sret == -1) {
		api->ib_printf2(pluginnum,"AutoDJ (ss_streamerp2p) -> Socket error in feeder! (errno: %d, desc: %s)\n",ad_config.sockets->GetLastError(),ad_config.sockets->GetLastErrorString());
		return false;
	}
	if (sret == 0) {
		api->ib_printf2(pluginnum,"AutoDJ (ss_streamerp2p) -> Server closed socket! (errno: %d, desc: %s)\n",ad_config.sockets->GetLastError(),ad_config.sockets->GetLastErrorString());
		return false;
	}
	return true;
}

void SP2P_Feeder::Close() {
	if (sock != NULL) {
		ad_config.sockets->Close(sock);
		sock = NULL;
	}
}

bool SP2P_Feeder::SendTitleUpdate(const char * title, TITLE_DATA * td) {
	return false;
}

AutoDJ_Feeder * sp2p_create() { return new SP2P_Feeder; }
void sp2p_destroy(AutoDJ_Feeder * feeder) {
	delete dynamic_cast<SP2P_Feeder *>(feeder);
}
