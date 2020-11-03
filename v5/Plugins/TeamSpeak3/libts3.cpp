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

#include "libts3.h"

string EncodeString(const char * p) {
	string ret;
	while (*p) {
		if (*p == ' ') {
			ret += "\\s";
		} else if (*p == '|') {
			ret += "\\p";
		} else if (*p == '\\' || *p == '/') {
			ret += "\\";
			ret += *p;
		} else if (*p == '\a') {
			ret += "\\a";
		} else if (*p == '\b') {
			ret += "\\b";
		} else if (*p == '\f') {
			ret += "\\f";
		} else if (*p == '\n') {
			ret += "\\n";
		} else if (*p == '\r') {
			ret += "\\r";
		} else if (*p == '\t') {
			ret += "\\t";
		} else if (*p == '\v') {
			ret += "\\v";
		} else {
			ret += *p;
		}
		p++;
	}
	return ret;
}

string DecodeString(const char * p) {
	string ret;
	while (*p) {
		if (*p == '\\') {
			p++;
			if (*p == 's') {
				ret += " ";
			} else if (*p == 'p') {
				ret += "|";
			} else if (*p == 'a') {
				ret += "\a";
			} else if (*p == 'b') {
				ret += "\b";
			} else if (*p == 'f') {
				ret += "\f";
			} else if (*p == 'n') {
				ret += "\n";
			} else if (*p == 'r') {
				ret += "\r";
			} else if (*p == 't') {
				ret += "\t";
			} else if (*p == 'v') {
				ret += "\v";
			} else {
				ret += *p;
			}
		} else {
			ret += *p;
		}
		p++;
	}
	return ret;
}

typedef std::map<string, string, ci_less> parmMap;
parmMap DecodeParms(const char *p) {
	parmMap ret;
	string key, val;
	bool inKey = true;
	while (*p) {
		if (*p == ' ') {
			if (key.length()) {
				ret[key] = DecodeString(val.c_str());
			}
			inKey = true;
			key = val = "";
		} else if (inKey) {
			if (*p == '=') {
				inKey = false;
			} else {
				key += *p;
			}
		} else {
			val += *p;
		}
		p++;
	}
	if (key.length()) {
		ret[key] = DecodeString(val.c_str());
	}
	return ret;
}
string GetParm(parmMap * m, string name, string sDefault="") {
	parmMap::const_iterator x = m->find(name);
	if (x != m->end()) {
		return x->second;
	}
	return sDefault;
}


TS3_Client::TS3_Client() {
	sock = NULL;
	nextPing = 0;
	my_username = "";
	my_client_id = my_channel_id = 0;
	my_channel_name = my_channel_topic = my_channel_desc = "";
}
TS3_Client::~TS3_Client() {
	AutoMutex(hMutex);
	Disconnect();
}

bool TS3_Client::EnableSSL(const char * pem_fn) {
	AutoMutex(hMutex);
	if (!socks.EnableSSL(pem_fn, TS3_SSL_METHOD_TLS)) {
		errorFromSocket(NULL);
		return false;
	}
	return true;
}

bool TS3_Client::Connect(const char * server, const char * username, const char * pass, int port, int server_id, const char * channel, vector<string> * tokens) {
	AutoMutex(hMutex);
	Disconnect();
	sock = socks.Create();
	if (socks.ConnectWithTimeout(sock, server, port, 10000)) {
		socks.SetKeepAlive(sock);
		char buf[1024];
		//time_t timeo = time(NULL)+10;
		if (!recvLine(buf, sizeof(buf), 10) || stricmp(buf, "TS3")) {
			error = "Invalid reply from TS3 server (is the port correct?)";
			socks.Close(sock);
			sock = NULL;
			return false;
		}
		//now receive welcome message
		if (!recvLine(buf, sizeof(buf), 10)) {
			socks.Close(sock);
			sock = NULL;
			return false;
		}

		ostringstream slogin;
		slogin << "login client_login_name=" << EncodeString(username) << " client_login_password=" << EncodeString(pass) << "\r\n";
		sendPacket(slogin.str().c_str());
		TS3_Std_Reply rep;
		if (!recvStdReply(&rep, 10) || rep.err_no != 0) {
			ostringstream serr;
			serr << "Error logging in to TS3 server: " << rep.err_no << " - " << rep.msg;
			error = serr.str();
			socks.Close(sock);
			sock = NULL;
			return false;
		}

		ostringstream suse;
		suse << "use sid=" << server_id << "\r\n";
		sendPacket(suse.str().c_str());
		if (!recvStdReply(&rep, 10) || rep.err_no != 0) {
			ostringstream serr;
			serr << "Error selecting TS3 server instance: " << rep.err_no << " - " << rep.msg;
			error = serr.str();
			socks.Close(sock);
			sock = NULL;
			return false;
		}

		sendPacket("whoami\r\n");
		while (recvLine(buf, sizeof(buf), 10)) {
			strtrim(buf);
			if (buf[0]) {
				if (!strnicmp(buf, "error ", 6)) {
					break;
				} else {
					parmMap m = DecodeParms(buf);
					parmMap::const_iterator x = m.find("client_id");
					if (x != m.end()) {
						my_client_id = atoi(x->second.c_str());
					}
					x = m.find("client_channel_id");
					if (x != m.end()) {
						my_channel_id = atoi(x->second.c_str());
					}
				}
			}
		}
		if (my_client_id == 0 || my_channel_id == 0) {
			error = "Error getting my client ID and channel ID!";
			socks.Close(sock);
			sock = NULL;
			return false;
		}

		if (channel && channel[0] && !joinChannel(channel)) {
			socks.Close(sock);
			sock = NULL;
			return false;
		}

		ostringstream schaninfo;
		schaninfo << "channelinfo cid=" << my_channel_id << "\r\n";
		sendPacket(schaninfo.str().c_str());
		while (recvLine(buf, sizeof(buf), 10)) {
			strtrim(buf);
			if (buf[0]) {
				if (!strnicmp(buf, "error ", 6)) {
					break;
				} else {
					parmMap m = DecodeParms(buf);
					parmMap::const_iterator x = m.find("channel_name");
					if (x != m.end()) {
						my_channel_name = x->second;
					}
					x = m.find("channel_topic");
					if (x != m.end()) {
						my_channel_topic = x->second;
					}
					x = m.find("channel_description");
					if (x != m.end()) {
						my_channel_desc = x->second;
					}
				}
			}
		}
		if (my_channel_name.length() == 0) {
			error = "Error getting channel name!";
			socks.Close(sock);
			sock = NULL;
			return false;
		}

		ostringstream snotify1;
		snotify1 << "servernotifyregister event=textprivate\r\n";
		sendPacket(snotify1.str().c_str());
		if (!recvStdReply(&rep, 10) || rep.err_no != 0) {
			ostringstream serr;
			serr << "Error registering for notifications 1: " << rep.err_no << " - " << rep.msg;
			error = serr.str();
			socks.Close(sock);
			sock = NULL;
			return false;
		}

		ostringstream snotify2;
		snotify2 << "servernotifyregister event=channel id=" << my_channel_id << "\r\n";
		sendPacket(snotify2.str().c_str());
		if (!recvStdReply(&rep, 10) || rep.err_no != 0) {
			ostringstream serr;
			serr << "Error registering for notifications 2: " << rep.err_no << " - " << rep.msg;
			error = serr.str();
			socks.Close(sock);
			sock = NULL;
			return false;
		}

		ostringstream snotify3;
		snotify3 << "servernotifyregister event=textchannel\r\n";// << " id=" << my_channel_id << "\r\n";
		sendPacket(snotify3.str().c_str());
		if (!recvStdReply(&rep, 10) || rep.err_no != 0) {
			ostringstream serr;
			serr << "Error registering for notifications 3: " << rep.err_no << " - " << rep.msg;
			error = serr.str();
			socks.Close(sock);
			sock = NULL;
			return false;
		}

		if (tokens) {
			for (vector<string>::const_iterator x = tokens->begin(); x != tokens->end(); x++) {
				//a.add_tokens(*x);
			}
		}

		//sendPacket("gm msg=I\\'m\\shere\r\n");

		lastPingReply = time(NULL)+30;
		nextPing = time(NULL)+10;
		return true;
	} else {
		errorFromSocket(sock);
	}
	return false;
}

void TS3_Client::Work() {
	AutoMutex(hMutex);
	if (time(NULL) >= nextPing) {
		sendPacket("ircbot_anti_idle\r\n");
		nextPing = time(NULL) + 30;
	}
	if (socks.Select_Read(sock, uint32(0)) == 1) {
		char buf[1024];
		int n;
		if ((n = socks.RecvLine(sock, buf, sizeof(buf)-1)) > 0) {
			buf[n] = 0;
			strtrim(buf);
			if (buf[0]) {
#if defined(DEBUG)
				printf("TS3 -> recvLine(): %s\n", buf);
#endif
				if (!strnicmp(buf, "notifytextmessage ", 18)) {
					parmMap m = DecodeParms(&buf[18]);
					int tarmode = atoi(GetParm(&m, "targetmode","-1").c_str());
					if (tarmode == 1) { // PM
						//recvLine(): Incoming: notifytextmessage targetmode=1 msg=dfgfdgfdgdfg target=2 invokerid=1 invokername=serveradmin invokeruid=yPGnoee5cMI1I8VMMIUdj9buVA4=
						int target = atoi(GetParm(&m, "target", "-1").c_str());
						if (target == my_client_id) {
							string msg = GetParm(&m, "msg");
							string nick = GetParm(&m, "invokername");
							int uid = atoi(GetParm(&m, "invokerid", "-1").c_str());
							if (msg.length() && nick.length() && uid >= 0) {
								OnPM(nick, uid, msg);
							}
#if defined(DEBUG)
						} else {
							printf("TS3 -> Unknown PM target: %d\n", target);
#endif
						}
					} else if (tarmode == 2) { // Channel message
						//recvLine(): Incoming: notifytextmessage targetmode=2 msg=test invokerid=1 invokername=serveradmin invokeruid=yPGnoee5cMI1I8VMMIUdj9buVA4=
						string msg = GetParm(&m, "msg");
						string nick = GetParm(&m, "invokername");
						int uid = atoi(GetParm(&m, "invokerid", "-1").c_str());
						if (msg.length() && nick.length() && uid >= 0 && uid != my_client_id) {
							OnChan(nick, uid, my_channel_id, my_channel_name, msg);
						}
					} else {
						printf("TS3 -> Unknown target mode: %d\n", tarmode);
					}
				}
			}
		} else if (n < RL3_NOLINE) {
			errorFromSocket(sock);
			socks.Close(sock);
			sock = NULL;
		}
	}
}

void TS3_Client::Disconnect() {
	AutoMutex(hMutex);
	if (sock) {
		sendPacket("QUIT\r\n");
		safe_sleep(2);
		socks.Close(sock);
		sock = NULL;
	}
	my_username = "";
	my_client_id = my_channel_id = 0;
	my_channel_name = my_channel_topic = my_channel_desc = "";
}

void TS3_Client::sendPacket(const char * data, int32 len) {
	AutoMutex(hMutex);
	if (len == -1) { len = strlen(data); }
	socks.Send(sock, data, len);
}

bool TS3_Client::recvLine(char * buf, int bufSize, int timeout) {
	AutoMutex(hMutex);
	time_t timeo = time(NULL) + timeout + 1;
	int n;
	while (time(NULL) < timeo) {
		if ((n = socks.RecvLine(sock, buf, bufSize)) > 0) {
			buf[n] = 0;
			strtrim(buf);
			if (buf[0] == 0) { continue; }
#if defined(DEBUG)
			printf("recvLine(): Incoming: %s\n", buf);
#endif
			return true;
		} else if (n < RL3_NOLINE) {
			return false;
		} else {
			safe_sleep(100, true);
		}
	}
	return false;
}

bool TS3_Client::recvStdReply(TS3_Std_Reply * rep, int timeout) {
	time_t timeo = time(NULL) + timeout;
	char buf[1024];
	int n = RL3_NOLINE;
	rep->err_no = -3;
	rep->msg = "Unknown error";
	while (time(NULL) < timeo) {
		if ((n = socks.RecvLine(sock, buf, sizeof(buf))) > 0) {
			buf[n] = 0;
			strtrim(buf);
			if (buf[0]) {
				if (!strnicmp(buf, "error ", 6)) {
					parmMap m = DecodeParms(&buf[6]);
					parmMap::const_iterator x = m.find("id");
					if (x != m.end()) {
						rep->err_no = atoi(x->second.c_str());
					}
					x = m.find("msg");
					if (x != m.end()) {
						rep->msg = x->second;
					}
					return true;
				}
			}
		} else {
			safe_sleep(100, true);
		}
	}
	if (n == RL3_NOLINE) {
		rep->err_no = -2;
		rep->msg = "No line received (yet)";
	} else if (n < RL3_NOLINE) {
		rep->err_no = -1;
		rep->msg = "Connection error";
	}
	return false;
}

void TS3_Client::SetClientNick(string str) {
	AutoMutex(hMutex);
	ostringstream sfind;
	sfind << "clientupdate client_nickname=" << EncodeString(str.c_str()) << "\r\n";
	sendPacket(sfind.str().c_str());
}
void TS3_Client::SetClientDesc(string str) {
	AutoMutex(hMutex);
	ostringstream sfind;
	sfind << "clientupdate client_description=" << EncodeString(str.c_str()) << "\r\n";
	sendPacket(sfind.str().c_str());
}
void TS3_Client::SetChannelDesc(string str) {
	AutoMutex(hMutex);
	ostringstream sfind;
	sfind << "channeledit cid=" << my_channel_id << " channel_description=" << EncodeString(str.c_str()) << "\r\n";
	sendPacket(sfind.str().c_str());
	my_channel_desc = str;
}
void TS3_Client::SetChannelTopic(string str) {
	AutoMutex(hMutex);
	ostringstream sfind;
	sfind << "channeledit cid=" << my_channel_id << " channel_topic=" << EncodeString(str.c_str()) << "\r\n";
	sendPacket(sfind.str().c_str());
	my_channel_topic = str;
}

bool TS3_Client::joinChannel(const char * chan) {
	AutoMutex(hMutex);

	ostringstream sfind;
	sfind << "channelfind pattern=" << EncodeString(chan) << "\r\n";
	sendPacket(sfind.str().c_str());
	char buf[1024];
	if (recvLine(buf, sizeof(buf), 10)) {
		if (strnicmp(buf, "error ", 6)) {
			TS3_Std_Reply rep;
			if (recvStdReply(&rep, 10)) {
				if (rep.err_no == 0) {
					strtrim(buf);
					parmMap m = DecodeParms(buf);
					parmMap::const_iterator x = m.find("cid");
					if (x != m.end()) {
						int channel_id = atoi(x->second.c_str());
						if (my_channel_id == channel_id) {
							return true;
						}
						ostringstream sjoin;
						sjoin << "clientmove clid=" << my_client_id << " cid=" << x->second << " \r\n";
						sendPacket(sjoin.str().c_str());
						if (recvStdReply(&rep, 10)) {
							if (rep.err_no == 0) {
								my_channel_id = channel_id;
								return true;
							} else {
								ostringstream serr;
								serr << "Error joining channel: " << rep.err_no << " - " << rep.msg;
								error = serr.str();
							}
						}
					} else {
						error = "Could not find cid in TS3 server response!";
					}
				} else {
					ostringstream serr;
					serr << "Error finding channel: " << rep.err_no << " - " << rep.msg;
					error = serr.str();
				}
			}
		} else {
			error = "Error finding channel (no permissions)!";
		}
	}
	return false;
}

void TS3_Client::SendPM(int user_id, string msg) {
	AutoMutex(hMutex);
	if (sock) {
		ostringstream omsg;
		omsg << "sendtextmessage targetmode=1 target=" << user_id << " msg=" << EncodeString(msg.c_str()) << "\r\n";
		sendPacket(omsg.str().c_str());
	}
}
void TS3_Client::SendChan(int channel_id, string msg) {
	AutoMutex(hMutex);
	if (sock) {
		ostringstream omsg;
		omsg << "sendtextmessage targetmode=2 target=" << channel_id << " msg=" << EncodeString(msg.c_str()) << "\r\n";
		sendPacket(omsg.str().c_str());
	}
}

/*
void TS3_Client::SetComment(const char * msg) {
	AutoMutex(hMutex);
	MumbleProto::UserState us;
	us.set_comment(msg ? msg:"");
	sendProtoBuf(MumbleEnums::UserState, &us);
}

void TS3_Client::SetImage(const uint8 * data, size_t len) {
	AutoMutex(hMutex);
	MumbleProto::UserState us;
	us.set_texture(data, len);
	sendProtoBuf(MumbleEnums::UserState, &us);
}

bool TS3_Client::SetImageFromFile(const char * fn) {
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

void TS3_Client::SendTextMessage(uint32 target_id, const char * msg, bool is_chan_message) {
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

size_t TS3_Client::NumUsers() {
	AutoMutex(hMutex);
	return users.size();
}
size_t TS3_Client::NumChannels() {
	AutoMutex(hMutex);
	return channels.size();
}
/*
MumbleUser TS3_Client::GetUser(size_t num) {
	AutoMutex(hMutex);
	return users.size();
}
MumbleChannel TS3_Client::GetChannel(size_t num) {
}
*/
	/*
MumbleUser TS3_Client::GetUserBySession(uint32 ses) {
	AutoMutex(hMutex);
	userMap::const_iterator x = users.find(ses);
	if (x != users.end()) {
		return x->second;
	}
	MumbleUser u;
	u.session_id = MUMBLE_INVALID_ID;
	return u;
}
MumbleChannel TS3_Client::GetChannelByID(uint32 id) {
	AutoMutex(hMutex);
	channelMap::const_iterator x = channels.find(id);
	if (x != channels.end()) {
		return x->second;
	}
	MumbleChannel c;
	c.id = MUMBLE_INVALID_ID;
	return c;
}

MumbleChannel TS3_Client::GetChannelByName(const char * name) {
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
*/

void TS3_Client::errorFromSocket(T_SOCKET * psock) {
	AutoMutex(hMutex);
	error = socks.GetLastErrorString(psock);
}
string TS3_Client::GetError() {
	AutoMutex(hMutex);
	return error;
}
void TS3_Client::GetError(char * errbuf, size_t bufSize) {
	AutoMutex(hMutex);
	errbuf[0] = 0;
	strlcpy(errbuf, error.c_str(), bufSize);
}
