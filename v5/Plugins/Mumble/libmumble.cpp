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

#include "libmumble.h"

#pragma pack(1)
struct MUMBLE_HEADER {
	uint16 type;
	uint32 len;
};
#pragma pack()

//Titus_Mutex MumbleClient::hMutex;

/*
void MumbleClient::SetSocks(Titus_Sockets * pSocks) {
	if (socks) {
	//	delete socks;
	}
	socks = pSocks;
}
*/

MumbleClient::MumbleClient() {
	//socks = NULL;//new Titus_Sockets;
	sock = NULL;
	nextPing = lastPingReply = 0;
	my_username = "";
	my_session_id = 0;
	my_channel_id = MUMBLE_INVALID_ID;
	conx_state = MumbleEnums::Disconnected;
}
MumbleClient::~MumbleClient() {
	AutoMutex(hMutex);
	Disconnect();
}

bool MumbleClient::EnableSSL(const char * pem_fn) {
	AutoMutex(hMutex);
	if (!socks.EnableSSL(pem_fn, TS3_SSL_METHOD_TLS)) {
		errorFromSocket(NULL);
		return false;
	}
	return true;
}

bool MumbleClient::Connect(const char * server, const char * username, const char * pass, int port, vector<string> * tokens) {
	AutoMutex(hMutex);
	Disconnect();
	sock = socks.Create();
	conx_state = MumbleEnums::Connecting;
	if (socks.ConnectWithTimeout(sock, server, port, 10000)) {
		if (socks.SwitchToSSL_Client(sock)) {
			MumbleProto::Version v;
			v.set_version(0x0100);
			v.set_release("libmumble");
			v.set_os(PLATFORM);
			v.set_os_version("Unknown");
			sendProtoBuf(MumbleEnums::Version, &v);
			MumbleProto::Authenticate a;
			a.set_username(username);
			if (pass && pass[0]) {
				a.set_password(pass);
			}
			if (tokens) {
				for (vector<string>::const_iterator x = tokens->begin(); x != tokens->end(); x++) {
					a.add_tokens(*x);
				}
			}
			sendProtoBuf(MumbleEnums::Authenticate, &a);
			lastPingReply = time(NULL)+30;
			nextPing = time(NULL)+10;
			return true;
		} else {
			errorFromSocket(sock);
		}
	} else {
		errorFromSocket(sock);
	}
	return false;
}

void MumbleClient::Work() {
	AutoMutex(hMutex);
	if (conx_state >= MumbleEnums::Connected) {
		if (time(NULL) >= nextPing) {
			MumbleProto::Ping p;
			p.set_timestamp(time(NULL));
			sendProtoBuf(MumbleEnums::Ping, &p);
			nextPing = time(NULL) + 10;
		}
	}
	recvPacket();
}

void MumbleClient::Disconnect() {
	AutoMutex(hMutex);
	if (sock) {
		socks.Close(sock);
		sock = NULL;
	}
	users.clear();
	channels.clear();
	my_username = "";
	my_session_id = 0;
	my_channel_id = MUMBLE_INVALID_ID;
	conx_state = MumbleEnums::Disconnected;
}

void MumbleClient::sendProtoBuf(uint16 type, ::google::protobuf::Message * msg) {
	AutoMutex(hMutex);
	sendPacket(type, msg->ByteSize(), msg->SerializeAsString().c_str());
	/*
	uint32 len = msg->ByteSize();
	char * buf = new char[6+len];
	MUMBLE_HEADER * mh = (MUMBLE_HEADER *)buf;
	mh->type = Get_UBE16(type);
	mh->len = Get_UBE32(len);
	if (len > 0) {
		memcpy(buf+6, msg->SerializeAsString().c_str(), len);
	}
	socks.Send(sock, buf, 6+len);
	delete [] buf;
	*/
}
void MumbleClient::sendPacket(uint16 type, uint32 len, const char * data) {
	AutoMutex(hMutex);
	char * buf = new char[6+len];
	MUMBLE_HEADER * mh = (MUMBLE_HEADER *)buf;
	mh->type = Get_UBE16(type);
	mh->len = Get_UBE32(len);
	if (len > 0) {
		memcpy(buf+6, data, len);
	}
	socks.Send(sock, buf, 6+len);
	delete [] buf;
}

template <class T>
T ProtobufFromBuffer(char * buffer, uint32 length) {
	T pb;
	pb.ParseFromArray(buffer, length);
#if defined(DEBUG)
	//printf("Incoming protobuf: %s -> %s\n", typeid(T).name(), pb.DebugString());
#endif
	return pb;
}

#define BUF2PROTO(x,y) MumbleProto::x y = ProtobufFromBuffer<MumbleProto::x>(buf, n); \
	On##x(y);

#define BUF2PROTO2(x,y) MumbleProto::x y = ProtobufFromBuffer<MumbleProto::x>(buf, n);

void MumbleClient::recvPacket() {
	AutoMutex(hMutex);
	while (sock && socks.Select_Read(sock, 1000) == 1) {
		MUMBLE_HEADER mh;
		int n;
		if ((n = socks.Recv(sock, (char *)&mh, sizeof(mh))) == sizeof(mh)) {
			mh.type = Get_UBE16(mh.type);
			mh.len = Get_UBE32(mh.len);
			char * buf = NULL;
			if (mh.len > 0) {
				buf = (char *)malloc(mh.len);
				uint32 left = mh.len;
				uint32 ind = 0;
				while (left) {
					n = socks.Recv(sock, buf+ind, left);
					if (n > 0) {
						left -= n;
						ind += n;
					} else if (n <= 0) {
						errorFromSocket(sock);
						Disconnect();
						free(buf);
						return;
					}
				}
				//printf("recv data: %d / %s\n", n, buf);
			}
			//printf("recv packet: %u / %u / %p\n", mh.type, mh.len, buf);
			if (mh.len == 0 || buf == NULL) {
				if (buf) { free(buf); }
				continue;
			}
			switch (mh.type) {
				case MumbleEnums::Version:{ // 0
						BUF2PROTO(Version, v);
#if defined(DEBUG)
						printf("Got version: %u.%u.%u / %s / %s / %s\n", (v.version() & 0xFFFF0000) >> 16, (v.version() & 0xFF00) >> 8, (v.version() & 0xFF), v.release().c_str(), v.os().c_str(), v.os_version().c_str());
#endif
						break;
					}

	 			case MumbleEnums::Ping:{ // 5
						BUF2PROTO(Ping, p);
#if defined(DEBUG)
						printf("Ping from server...\n");
#endif
						lastPingReply = time(NULL);
						break;
					}

	 			case MumbleEnums::Reject:{ // 6
						BUF2PROTO2(Reject, r);
						error = r.reason();
#if defined(DEBUG)
						printf("Connection rejected: %s\n", r.reason().c_str());
#endif
						OnReject(r);
						Disconnect();
						break;
					}


	 			case MumbleEnums::ServerSync:{ // 5
						BUF2PROTO2(ServerSync, ss);
						my_session_id = ss.session();
						MumbleUser user = GetUserBySession(my_session_id);
						if (user.session_id != MUMBLE_INVALID_ID) {
							my_username = user.username;
						}
						conx_state = MumbleEnums::Connected;
#if defined(DEBUG)
						printf("Server sync: %u / " U64FMT " / %u / %s\n", ss.session(), ss.permissions(), ss.max_bandwidth(), ss.welcome_text().c_str());
#endif
						OnServerSync(ss);
						break;
					}

	 			case MumbleEnums::ChannelRemove:{ // 6
						BUF2PROTO(ChannelRemove, cr);
						channelMap::iterator x = channels.find(cr.channel_id());
						if (x != channels.end()) {
							channels.erase(x);
						}
						if (cr.channel_id() == my_channel_id) {
							my_channel_id = MUMBLE_INVALID_ID;
						}
#if defined(DEBUG)
						printf("Remove channel: %u\n", cr.channel_id());
#endif
						break;
					}

	 			case MumbleEnums::ChannelState:{ // 7
						BUF2PROTO2(ChannelState, cs);
						MumbleChannel chan;
						channelMap::iterator x = channels.find(cs.channel_id());
						if (x != channels.end()) {
							chan = x->second;
						} else {
							chan.id = cs.channel_id();
						}
						if (cs.has_description()) {
							chan.desc = cs.description();
						}
						if (cs.has_name()) {
							chan.name = cs.name();
						}
						channels[chan.id] = chan;
						OnChannelState(cs, &chan);
#if defined(DEBUG)
						printf("Channel state: %u / %s / %s\n", cs.channel_id(), cs.name().c_str(), cs.description().c_str());
#endif
						break;
					}

	 			case MumbleEnums::UserRemove:{ // 8
						BUF2PROTO(UserRemove, ur);
						userMap::iterator x = users.find(ur.session());
						if (x != users.end()) {
							users.erase(x);
						}
#if defined(DEBUG)
						printf("Remove user ses: %u / %u / %s / %d\n", ur.session(), ur.has_actor() ? ur.actor():0, ur.has_reason() ? ur.reason().c_str():"N/A", ur.has_ban());
#endif
						break;
					}

	 			case MumbleEnums::UserState:{ // 9
						BUF2PROTO2(UserState, us);
						MumbleUser user;
						/*
						name string Opt. User name, UTF-8 encoded
user id uint32 Opt. Registered user ID if the user is registered
channel id
						*/
						userMap::iterator x = users.find(us.session());
						if (x != users.end()) {
							user = x->second;
						} else {
							user.session_id = us.session();
						}
						if (us.has_user_id()) {
							user.user_id = us.user_id();
						}
						if (us.has_name()) {
							user.username = us.name();
						}
						if (us.has_channel_id()) {
							user.channel_id = us.channel_id();
						}
						if (user.session_id == my_session_id) {
							my_channel_id = user.channel_id;
						}
						users[user.session_id] = user;
						OnUserState(us, &user);
#if defined(DEBUG)
						printf("User state: %u / %u / %s\n", us.session(), us.channel_id(), us.name().c_str());
#endif
						break;
					}
	 			case MumbleEnums::TextMessage:{ // 11
						BUF2PROTO(TextMessage, tm);
#if defined(DEBUG)
						printf("TextMessage: %u / %u / %u / %s\n", tm.actor(), tm.session().begin(), tm.channel_id().begin(), tm.message().c_str());
#endif
						break;
					}

	 			case MumbleEnums::PermissionDenied:{ // 12
						BUF2PROTO2(PermissionDenied, pd);
						if (pd.session() == my_session_id) {
							stringstream sstr;
							switch (pd.type()) {
								case MumbleProto::PermissionDenied_DenyType_Text:
									error = pd.reason();
									break;
								case MumbleProto::PermissionDenied_DenyType_Permission:
									sstr << "Denied permission " << pd.permission() << " on channel with ID " << pd.channel_id();
									error = sstr.str();
									break;
								case MumbleProto::PermissionDenied_DenyType_SuperUser:
									error = "Cannot modify SuperUser";
									break;
								case MumbleProto::PermissionDenied_DenyType_ChannelName:
									error = "Invalid channel name";
									break;
								case MumbleProto::PermissionDenied_DenyType_TextTooLong:
									error = "Text message too long";
									break;
								case MumbleProto::PermissionDenied_DenyType_NestingLimit:
									error = "Nesting limit reached";
									break;
								case MumbleProto::PermissionDenied_DenyType_H9K:
									error = "The flux capacitor was spelled wrong";
									break;
								case MumbleProto::PermissionDenied_DenyType_TemporaryChannel:
									error = "Operation not permitted in temporary channel";
									break;
								case MumbleProto::PermissionDenied_DenyType_MissingCertificate:
									error = "Operation requires certicate";
									break;
								case MumbleProto::PermissionDenied_DenyType_UserName:
									//stringstream sstr;
									sstr << "Invalid username '" << pd.name() << "'";
									error = sstr.str();
									break;
								case MumbleProto::PermissionDenied_DenyType_ChannelFull:
									error = "Channel is full";
									break;
								default:
									error = "Unknown Error Type";
									break;
							}
						} else {
						}
#if defined(DEBUG)
						printf("PermissionDenied: %s\n", error.c_str());
#endif
						OnPermissionDenied(pd);
						break;
					}

				case MumbleEnums::CryptSetup:{ // 15
						BUF2PROTO(CryptSetup, cs);
						//MumbleProto::CryptSetup cs = ProtobufFromBuffer<MumbleProto::CryptSetup>(buf, n);
						break;
					}

				case MumbleEnums::PermissionQuery:{ // 20
						BUF2PROTO(PermissionQuery, pq);
#if defined(DEBUG)
						printf("PermissionQuery: %u / %u / %u\n", pq.channel_id(), pq.permissions(), pq.flush());
#endif
						break;
					}

				case MumbleEnums::CodecVersion:{ // 21
						BUF2PROTO(CodecVersion, cv);
						break;
					}

				case MumbleEnums::RequestBlob:{ // 23
						BUF2PROTO(RequestBlob, rb);
						break;
					}

				case MumbleEnums::ServerConfig:{ // 24
						BUF2PROTO(ServerConfig, sc);
#if defined(DEBUG)
						printf("ServerConfig: %s / %u\n", sc.welcome_text().c_str(), sc.message_length());
#endif
						break;
					}

				default:
#if defined(DEBUG)
					printf("Recv unknown packet of type: %u, len: %u\n", mh.type, mh.len);
#endif
					break;
			}
			if (buf) { free(buf); }
		} else {
			errorFromSocket(sock);
			Disconnect();
		}
	}
}

void MumbleClient::JoinChannel(uint32 channel_id) {
	AutoMutex(hMutex);
	MumbleProto::UserState us;
	us.set_channel_id(channel_id);
	sendProtoBuf(MumbleEnums::UserState, &us);
}

void MumbleClient::SetComment(const char * msg) {
	AutoMutex(hMutex);
	MumbleProto::UserState us;
	us.set_comment(msg ? msg:"");
	sendProtoBuf(MumbleEnums::UserState, &us);
}

void MumbleClient::SetImage(const uint8 * data, size_t len) {
	AutoMutex(hMutex);
	MumbleProto::UserState us;
	us.set_texture(data, len);
	sendProtoBuf(MumbleEnums::UserState, &us);
}

bool MumbleClient::SetImageFromFile(const char * fn) {
	AutoMutex(hMutex);
	bool ret = false;
	FILE * fp = fopen(fn, "rb");
	if (fp) {
		fseek(fp, 0, SEEK_END);
		long len = ftell(fp);
		if (len > 0) {
			fseek(fp, 0, SEEK_SET);
			uint8 * buf = new uint8[len];
			if (fread(buf, len, 1, fp) == 1) {
				SetImage(buf, len);
				ret = true;
			} else {
				error = "Error reading file!";
			}
			delete [] buf;
		} else {
			error = "File is zero-length or error getting file length!";
		}
		fclose(fp);
	} else {
		error = "Error opening file for reading!";
	}
	return ret;
}

void MumbleClient::SendTextMessage(uint32 target_id, const char * msg, bool is_chan_message) {
	AutoMutex(hMutex);
	MumbleProto::TextMessage tm;
	if (is_chan_message) {
		tm.add_channel_id(target_id);
	} else {
		tm.add_session(target_id);
	}
//	tm.set_session(session_id);
	tm.set_message(msg);
	sendProtoBuf(MumbleEnums::TextMessage, &tm);
}

size_t MumbleClient::NumUsers() {
	AutoMutex(hMutex);
	return users.size();
}
size_t MumbleClient::NumChannels() {
	AutoMutex(hMutex);
	return channels.size();
}
/*
MumbleUser MumbleClient::GetUser(size_t num) {
	AutoMutex(hMutex);
	return users.size();
}
MumbleChannel MumbleClient::GetChannel(size_t num) {
}
*/
MumbleUser MumbleClient::GetUserBySession(uint32 ses) {
	AutoMutex(hMutex);
	userMap::const_iterator x = users.find(ses);
	if (x != users.end()) {
		return x->second;
	}
	MumbleUser u;
	u.session_id = MUMBLE_INVALID_ID;
	return u;
}
MumbleChannel MumbleClient::GetChannelByID(uint32 id) {
	AutoMutex(hMutex);
	channelMap::const_iterator x = channels.find(id);
	if (x != channels.end()) {
		return x->second;
	}
	MumbleChannel c;
	c.id = MUMBLE_INVALID_ID;
	return c;
}

MumbleChannel MumbleClient::GetChannelByName(const char * name) {
	AutoMutex(hMutex);
	for (channelMap::const_iterator x = channels.begin(); x != channels.end(); x++) {
		if (!stricmp(x->second.name.c_str(), name)) {
			return x->second;
		}
	}
	MumbleChannel c;
	c.id = MUMBLE_INVALID_ID;
	return c;
}

void MumbleClient::errorFromSocket(T_SOCKET * psock) {
	AutoMutex(hMutex);
	error = socks.GetLastErrorString(psock);
}
string MumbleClient::GetError() {
	AutoMutex(hMutex);
	return error;
}
void MumbleClient::GetError(char * errbuf, size_t bufSize) {
	AutoMutex(hMutex);
	errbuf[0] = 0;
	strlcpy(errbuf, error.c_str(), bufSize);
}
