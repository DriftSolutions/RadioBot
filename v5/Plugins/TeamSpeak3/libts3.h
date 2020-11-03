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

#define DSL_STATIC
#ifndef ENABLE_OPENSSL
#define ENABLE_OPENSSL
#endif
#include "../../src/plugins.h"

#define TS3_PORT 10011

class MumbleUser {
public:
	MumbleUser() { session_id = user_id = channel_id = 0; }
	uint32 session_id;
	string username;
	uint32 user_id;
	uint32 channel_id;
};
class MumbleChannel {
public:
	uint32 id;
	string name;
	string desc;
};

class TS3_Std_Reply {
public:
	TS3_Std_Reply() { err_no = -99; }
	int err_no;
	string msg;
};

class TS3_Client {
private:
	//bool loggedIn;
	time_t nextPing;
	time_t lastPingReply;
	string my_username;
	//string my_password;
	int my_client_id;
	int my_channel_id;
	string my_channel_name;
	string my_channel_topic;
	string my_channel_desc;
protected:
	Titus_Sockets socks;
	T_SOCKET * sock;
	Titus_Mutex hMutex;
	string error;
	void errorFromSocket(T_SOCKET * psock);
	void sendPacket(const char * data, int32 len=-1);
	bool recvLine(char * buf, int bufSize, int timeout=0);
	bool recvStdReply(TS3_Std_Reply * rep, int timeo=0);
	bool joinChannel(const char * chan);

public:
	TS3_Client();
	virtual ~TS3_Client();

	//void SetSocks(Titus_Sockets * pSocks);
	bool EnableSSL(const char * pem_fn);
	bool Connect(const char * server, const char * username, const char * pass, int port, int server_id, const char * channel, vector<string> * tokens=NULL);
	bool IsConnected() { AutoMutex(hMutex); return (sock != NULL); }
	void Work();
	/*
	void SetComment(const char * msg);
	void SetImage(const uint8 * data, size_t len);
	bool SetImageFromFile(const char * fn);
	*/

	void SendPM(int user_id, string msg);
	void SendChan(int channel_id, string msg);
	time_t LastPingReply() { return lastPingReply; }
	void Disconnect();

	string MyUsername() { return my_username; }
	int MyChannelID() { return my_channel_id; }
	string MyChannelName() { return my_channel_name; }
	string MyChannelTopic() { return my_channel_topic; }
	string MyChannelDescription() { return my_channel_desc; }
	void SetClientNick(string str);
	void SetClientDesc(string str);
	void SetChannelDesc(string str);
	void SetChannelTopic(string str);

	string GetError();
	void GetError(char * errbuf, size_t bufSize);

	virtual void OnPM(string nick, int nick_id, string msg){};
	virtual void OnChan(string nick, int nick_id, int channel_id, string channel_name, string msg){};
	/*
	virtual void OnVersion(MumbleProto::Version){};
	virtual void OnUDPTunnel(MumbleProto::UDPTunnel){};
	virtual void OnAuthenticate(MumbleProto::Authenticate){};
	virtual void OnPing(MumbleProto::Ping){};
	virtual void OnReject(MumbleProto::Reject){};
	virtual void OnServerSync(MumbleProto::ServerSync){};
	virtual void OnChannelRemove(MumbleProto::ChannelRemove){};
	virtual void OnChannelState(MumbleProto::ChannelState, MumbleChannel * chan){};
	virtual void OnUserRemove(MumbleProto::UserRemove){};
	virtual void OnUserState(MumbleProto::UserState, MumbleUser * user){};
	virtual void OnBanList(MumbleProto::BanList){};
	virtual void OnPermissionDenied(MumbleProto::PermissionDenied){};
	virtual void OnACL(MumbleProto::ACL){};
	virtual void OnQueryUsers(MumbleProto::QueryUsers){};
	virtual void OnCryptSetup(MumbleProto::CryptSetup){};
//	virtual void OnContextActionAdd(MumbleProto::ContextActionAdd){};
	virtual void OnContextAction(MumbleProto::ContextAction){};
	virtual void OnUserList(MumbleProto::UserList){};
	virtual void OnVoiceTarget(MumbleProto::VoiceTarget){};
	virtual void OnPermissionQuery(MumbleProto::PermissionQuery){};
	virtual void OnCodecVersion(MumbleProto::CodecVersion){};
	virtual void OnUserStats(MumbleProto::UserStats){};
	virtual void OnRequestBlob(MumbleProto::RequestBlob){};
	virtual void OnServerConfig(MumbleProto::ServerConfig){};
	*/
};

