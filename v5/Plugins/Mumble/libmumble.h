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
#include <drift/dsl.h>
#include <mumble.pb.h>

#if defined(WIN32)
	#if defined(DEBUG)
		#pragma comment(lib, "libprotobuf_d.lib")
	#else
		#pragma comment(lib, "libprotobuf.lib")
	#endif
#endif

#define MUMBLE_PORT 64738
#define MUMBLE_INVALID_ID 0xFFFFFFFF

namespace MumbleEnums {
	enum MumbleConnectionState {
		Disconnected,
		Connecting,
		Connected
	};
	enum MumbleMessageType {
		Version,
		UDPTunnel,
		Authenticate,
		Ping,
		Reject,
		ServerSync,
		ChannelRemove,
		ChannelState,
		UserRemove,
		UserState,
		BanList,
		TextMessage,
		PermissionDenied,
		ACL,
		QueryUsers,
		CryptSetup,
		ContextActionAdd,
		ContextAction,
		UserList,
		VoiceTarget,
		PermissionQuery,
		CodecVersion,
		UserStats,
		RequestBlob,
		ServerConfig
	};
};

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

class MumbleClient {
private:
	time_t nextPing;
	time_t lastPingReply;
	uint32 my_session_id;
	uint32 my_channel_id;
	string my_username;
	MumbleEnums::MumbleConnectionState conx_state;
protected:
	Titus_Sockets socks;
	T_SOCKET * sock;
	Titus_Mutex hMutex;
	string error;
	void errorFromSocket(T_SOCKET * psock);
	void recvPacket();
	void sendPacket(uint16 type, uint32 len, const char * data); ///< type is one of MumbleMessageType
	void sendProtoBuf(uint16 type, ::google::protobuf::Message * msg); ///< type is one of MumbleMessageType

	typedef std::map<uint32, MumbleUser> userMap;
	userMap users;
	typedef std::map<uint32, MumbleChannel> channelMap;
	channelMap channels;
public:
	MumbleClient();
	virtual ~MumbleClient();

	//void SetSocks(Titus_Sockets * pSocks);
	bool EnableSSL(const char * pem_fn);
	bool Connect(const char * server, const char * username, const char * pass=NULL, int port=MUMBLE_PORT, vector<string> * tokens=NULL);
	MumbleEnums::MumbleConnectionState GetConnectionState() { return conx_state; }
	void Work();
	void JoinChannel(uint32 channel_id);
	void SetComment(const char * msg);
	void SetImage(const uint8 * data, size_t len);
	bool SetImageFromFile(const char * fn);
	void SendTextMessage(uint32 target_id, const char * msg, bool is_chan_message=false);
	time_t LastPingReply() { return lastPingReply; }
	void Disconnect();

	uint32 MySessionID() { return my_session_id; }
	uint32 MyChannelID() { return my_channel_id; }
	string MyUsername() { return my_username; }

	size_t NumUsers();
	size_t NumChannels();
	MumbleUser GetUser(size_t num);
	MumbleChannel GetChannel(size_t num);

	MumbleUser GetUserBySession(uint32 ses);
	MumbleChannel GetChannelByID(uint32 id);
	MumbleChannel GetChannelByName(const char * name);

	string GetError();
	void GetError(char * errbuf, size_t bufSize);

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
	virtual void OnTextMessage(MumbleProto::TextMessage){};
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
};
