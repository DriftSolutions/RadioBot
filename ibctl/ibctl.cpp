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

#include "../v5/src/plugins.h"
#include "../Common/libremote/libremote.h"

char file[4096]={0};
std::string defFile;

std::string host    = "127.0.0.1";
std::string user;
std::string pass;
int32 port          = 10001;

struct COMMAND_LIST {
	char * cmd;
	REMOTE_COMMANDS_C2S cmd_code;
	bool req_data;
};

COMMAND_LIST commands[] = {
	{ "query_stream",	RCMD_QUERY_STREAM, false },
	{ "req_logout",		RCMD_REQ_LOGOUT, false },
	{ "current",		RCMD_REQ_CURRENT, false },
	{ "request",		RCMD_REQ, true },
	{ "dospam",			RCMD_DOSPAM, false },
	{ "die",			RCMD_DIE, false },
	{ "restart",		RCMD_RESTART, false },
	{ "rehash",			RCMD_REHASH, false },
	{ "broadcast",		RCMD_BROADCAST_MSG, true },
	{ "rehash",			RCMD_REHASH, false },
	{ "upload",			RCMD_UPLOAD_START, true },
	{ "get_version",	RCMD_GET_VERSION, false },

	{ "source-force",		RCMD_SRC_FORCE_OFF, false },
	{ "source-play",		RCMD_SRC_FORCE_ON, false },
	{ "source-next",		RCMD_SRC_NEXT, false },
	{ "source-relay",		RCMD_SRC_RELAY, true },
	{ "source-reload",		RCMD_SRC_RELOAD, false },
	{ "source-get-song",	RCMD_SRC_GET_SONG, false },
	{ "source-get-song-info",	RCMD_SRC_GET_SONG_INFO, false },
	{ "source-status",		RCMD_SRC_STATUS, false },
	{ "source-get-name",	RCMD_SRC_GET_NAME, false },

	{ "getuserinfo",	RCMD_GETUSERINFO, true },

	/*
	RCMD_SEND_REQ		= 0x13,
	RCMD_SEND_DED		= 0x15,
	*/
	{ NULL, (REMOTE_COMMANDS_C2S)0, false }
};

COMMAND_LIST commands2[] = {
	{ "autodj-force",	RCMD_SRC_FORCE_OFF, false },
	{ "autodj-play",	RCMD_SRC_FORCE_ON, false },
	{ "autodj-next",	RCMD_SRC_NEXT, false },
	{ "autodj-reload",	RCMD_SRC_RELOAD, false },
	{ "autodj-get-song",RCMD_SRC_GET_SONG, false },
	{ "autodj-status",	RCMD_SRC_STATUS, false },
	{ NULL, (REMOTE_COMMANDS_C2S)0, false }
};

REMOTE_COMMANDS_C2S GetCommandCode(char * cmd) {
	int i;
	for (i=0; commands[i].cmd != NULL; i++) {
		if (!stricmp(commands[i].cmd, cmd)) {
			return commands[i].cmd_code;
		}
	}
	for (i=0; commands2[i].cmd != NULL; i++) {
		if (!stricmp(commands2[i].cmd, cmd)) {
			return commands2[i].cmd_code;
		}
	}
	return (REMOTE_COMMANDS_C2S)0;
}

COMMAND_LIST * GetCommand(char * cmd) {
	int i;
	for (i=0; commands[i].cmd != NULL; i++) {
		if (!stricmp(commands[i].cmd, cmd)) {
			return &commands[i];
		}
	}
	for (i=0; commands2[i].cmd != NULL; i++) {
		if (!stricmp(commands2[i].cmd, cmd)) {
			return &commands2[i];
		}
	}
	return NULL;
}

void PrintUsage() {
	printf("ibctl: RadioBot CLI Control Program\n\n");

	printf("Default config file: %s\n\n", defFile.c_str());

	printf("Options:\n");
	printf("\t-l\t\tList the available commands for -c\n");
	printf("\t-c command\tSet the command to send\n");
	printf("\t-d data\t\tSet the data to send with the command\n");
	printf("\t-f filename\tSet the config file to use (overriding the default)\n");
	printf("\t-r\t\t(Re)Configure the control program\n");
}

void GetValue(char * prompt, char * buf, int32 bufSize, const char * def=NULL) {
	buf[0]=0;
	if (def && !strlen(def)) { def = NULL; }
	printf("%s ", prompt);
	while (fgets(buf,bufSize,stdin)) {
		strtrim(buf);
		if (buf[0] == 0 && !def) {
			printf("%s ", prompt);
		}
	}
	if (buf[0] == 0 && def) {
		strlcpy(buf, def, bufSize);
	}
}

void DoConfig() {
	char buf[1024],buf2[1024],buf3[16];;

	sprintf(buf2, "What IP/Hostname is RadioBot running on? [cur: %s]", host.c_str());
	GetValue(buf2, buf, sizeof(buf), host.c_str());
	host = buf;

	sprintf(buf2, "What port is RadioBot running on? [cur: %d]", port);
	sprintf(buf3, "%d", port);
	GetValue(buf2, buf, sizeof(buf), buf3);
	port = atoi(buf);

	sprintf(buf2, "What username should I use to log into RadioBot? [cur: %s]", user.c_str());
	GetValue(buf2, buf, sizeof(buf), user.c_str());
	user = buf;

	GetValue("What password should I use to log into RadioBot?", buf, sizeof(buf), pass.c_str());
	pass = buf;

	Universal_Config * cfg = new Universal_Config;
	DS_CONFIG_SECTION * sec = cfg->FindOrAddSection(NULL, "Config");
	if (sec) {
		DS_VALUE val;
		val.type = DS_TYPE_STRING;
		val.pString = (char *)host.c_str();
		cfg->SetSectionValue(sec, "Host", &val);
		val.pString = (char *)user.c_str();
		cfg->SetSectionValue(sec, "User", &val);
		val.pString = (char *)pass.c_str();
		cfg->SetSectionValue(sec, "Pass", &val);
		val.type = DS_TYPE_LONG;
		val.lLong = port;
		cfg->SetSectionValue(sec, "Port", &val);
		cfg->WriteConfig(file);
		printf("Saved config to %s\n", file);
	}
	delete cfg;

}

int err_printf(void * uptr, const char * fmt, ...) {
	char buf[1024];
	va_list ap;
	va_start(ap, fmt);
	vsprintf(buf, fmt, ap);
	va_end(ap);
	return fprintf(stderr, "%s", buf);
}

void do_upload(RemoteClient * rc, const char * ffn);
int main(int argc, char * argv[]) {

#if defined(WIN32)
	char * p = getenv("APPDATA");
	if (p) {
		strcpy(file, p);
	} else {
		strcpy(file, ".");
	}
	if (file[strlen(file)-1] != PATH_SEP) {
		strcat(file, PATH_SEPS);
	}
	strcat(file, "IRCBot");
	if (access(file, 0) != 0) {
		fprintf(stderr,"Creating %s\n", file);
		mkdir(file);
	}
#else
	char * p = getenv("HOME");
	if (p) {
		strcpy(file, p);
	} else {
		strcpy(file, ".");
	}
	if (file[strlen(file)-1] != PATH_SEP) {
		strcat(file, PATH_SEPS);
	}
	strcat(file, ".ircbot");
	if (access(file, 0) != 0) {
		fprintf(stderr,"Creating %s\n", file);
		mkdir(file, 0700);
	}
#endif
	if (file[strlen(file)-1] != PATH_SEP) {
		strcat(file, PATH_SEPS);
	}
	strcat(file, "ibctl.conf");
	defFile = file;

	int c;
	opterr = 0;

	bool config = false;

	COMMAND_LIST * selCmd = NULL;
	REMOTE_HEADER rHead;
	memset(&rHead, 0, sizeof(rHead));
	std::string data;
	/*
	struct REMOTE_HEADER {
		REMOTE_COMMANDS cmd;
		uint32 datalen;
	};
	*/

	while ((c = getopt (argc, argv, "c:d:lrf:")) != -1) {
		switch (c) {
			case 'c':
				selCmd = GetCommand(optarg);
				if (selCmd) {
					rHead.ccmd = selCmd->cmd_code;
				} else {
					fprintf(stderr, "Error: Unknown command '%s'\n", optarg);
					return 1;
				}
				break;
			case 'd':
				data = optarg;
				rHead.datalen = data.length();
				break;
			case 'r':
				config = true;
				break;
			case 'f':
				strcpy(file, optarg);
				break;
			case 'l':
				printf("Command list:\n\n");
				for (int i=0; commands[i].cmd != NULL; i++) {
					printf("%s\n", commands[i].cmd);
				}
				printf("\nEnd of list.\n");
				return 1;
				break;
			case '?':
				if (isprint(optopt)) {
					fprintf(stderr, "Unknown option `-%c'.\n", optopt);
				} else {
					fprintf(stderr, "Unknown option character `\\x%x'.\n", optopt);
				}
				return 1;
				break;
			default:
				fprintf(stderr, "Error decoding command line options.\n");
				return 1;
		}
	}

	Universal_Config * cfg = new Universal_Config;
	if (cfg->LoadConfig(file)) {
		DS_CONFIG_SECTION * sec = cfg->GetSection(NULL, "Config");
		if (sec) {
			DS_VALUE * val = cfg->GetSectionValue(sec, "Host");
			if (val && val->type == DS_TYPE_STRING) {
				host = val->pString;
			}
			val = cfg->GetSectionValue(sec, "User");
			if (val && val->type == DS_TYPE_STRING) {
				user = val->pString;
			}
			val = cfg->GetSectionValue(sec, "Pass");
			if (val && val->type == DS_TYPE_STRING) {
				pass = val->pString;
			}
			val = cfg->GetSectionValue(sec, "Port");
			if (val && val->type == DS_TYPE_LONG) {
				port = val->lLong;
			}
		} else {
			fprintf(stderr,"Configuration loaded, but you have no 'Config' section!\n");
		}
	}
	delete cfg;

	if (config || !host.length() || !user.length() || !pass.length()) {
		DoConfig();
		return 1;
	}

	if (!selCmd) {
		PrintUsage();
		return 1;
	}

	if (selCmd->req_data && data.length() == 0) {
		fprintf(stderr, "Error: This command expects data to be sent with it. (specified with -d)\n");
		return 1;
	}

	RemoteClient rc;
	rc.SetOutput(err_printf);
	if (rc.Connect((char *)host.c_str(), port, (char *)user.c_str(), (char *)pass.c_str(), 0) && rc.IsReady()) {
		if (rHead.ccmd == RCMD_UPLOAD_START) {
			do_upload(&rc, data.c_str());
		} else if (rc.SendPacket(&rHead, (char *)data.c_str())) {
			if (rc.RecvPacket(&rHead, file, sizeof(file))) {
				printf("Reply: %02X\n", rHead.scmd);
				printf("Datalen: %u\n", rHead.datalen);

				if (rHead.scmd == RCMD_STREAM_INFO) {
					STREAM_INFO * si = (STREAM_INFO *)file;
					printf("Title: %s\n", si->title);
					printf("DJ: %s\n", si->dj);
					printf("Listeners: %d\n", si->listeners);
					printf("Peak: %d\n", si->peak);
					printf("Max: %d\n", si->max);
				} else if (rHead.scmd == RCMD_USERINFO) {
					if (rHead.datalen == sizeof(IBM_USEREXTENDED)) {
						IBM_USEREXTENDED * ue = (IBM_USEREXTENDED *)file;
						printf("Nick: %s\n", ue->nick);
						printf("Pass: %s\n", ue->pass);
						if (ue->ulevel_old_for_v4_binary_compat) {
							printf("Level: %d\n", ue->ulevel_old_for_v4_binary_compat);
						} else {
							printf("Flags: %08X\n", ue->flags);
						}
						printf("NumHostmasks: %d\n", ue->numhostmasks);
						for (int32 i=0; i < ue->numhostmasks; i++) {
							printf("Hostmask%d: %s\n", i, ue->hostmasks[i]);
						}
					} else {
						printf("Error: Unknown IBM_USEREXTENDED version\n");
					}
				} else if (rHead.scmd == RCMD_USERNOTFOUND) {
					printf("Error: User not found!\n");
				} else if (rHead.scmd == RCMD_SONG_INFO) {
					IBM_SONGINFO * si = (IBM_SONGINFO *)file;
					printf("File ID: %u\n", si->file_id);
					printf("Filename: %s\n", si->fn);
					if (si->title[0]) {
						printf("Title: %s\n", si->title);
						printf("Artist: %s\n", si->artist);
						printf("Album: %s\n", si->album);
						printf("Genre: %s\n", si->genre);
					}
					printf("SongLen: %d\n", si->songLen);
					printf("Is Request: %s\n", si->is_request ? "yes":"no");
					if (si->requested_by[0]) {
						printf("Requested By: %s\n", si->requested_by);
					}
					printf("Playback Position: " I64FMT "\n", si->playback_position);
					printf("Playback Length: " I64FMT "\n", si->playback_length);
				} else {
					if (strlen(file) == rHead.datalen) {
						printf("Data: %s\n", file);
					} else {
						printf("Data: ");
						for (uint32 i=0; i < rHead.datalen; i++) {
							printf("%c", file[i]);
						}
					}
				}
			}
		}
	}

	return 0;
}

void do_upload(RemoteClient * rc, const char * ffn) {
	char buf[MAX_REMOTE_PACKET_SIZE+1];
	REMOTE_UPLOAD_START * rus = (REMOTE_UPLOAD_START *)buf;
	REMOTE_UPLOAD_DATA * rud = (REMOTE_UPLOAD_DATA *)buf;
	REMOTE_HEADER rHead;

	const char * fn = nopath(ffn);
	FILE * fp = fopen(ffn, "rb");
	if (fp != NULL) {
		fseek(fp, 0, SEEK_END);
		int64 len64 = ftell64(fp);
		if (len64 <= 0xFFFFFFFE) {
			fseek(fp, 0, SEEK_SET);
			rus->size = len64;
			hashfile("sha1", ffn, (char *)rus->hash, sizeof(rus->hash)+1);
			strlcpy(rus->filename, fn, MAX_REMOTE_PACKET_SIZE - sizeof(REMOTE_UPLOAD_START));
			rHead.ccmd = RCMD_UPLOAD_START;
			rHead.datalen = sizeof(REMOTE_UPLOAD_START) + strlen(fn);
			if (rc->SendPacketAwaitResponse(&rHead, buf, sizeof(buf))) {
				buf[rHead.datalen] = 0;
				if (rHead.scmd == RCMD_UPLOAD_OK) {
					uint8 xferid = (uint8)buf[0];
					uint32 block_size = MAX_REMOTE_PACKET_SIZE - sizeof(REMOTE_UPLOAD_DATA);
					bool errored = false;
					printf("Sending data...\n");
					while (!errored && !feof(fp)) {
						uint32 block = ftell64(fp);
						rud->xferid = xferid;
						rud->offset = block;
						uint32 len = fread(&buf[sizeof(REMOTE_UPLOAD_DATA)], 1, block_size, fp);
						if (len > 0) {
							rHead.ccmd = RCMD_UPLOAD_DATA;
							rHead.datalen = sizeof(REMOTE_UPLOAD_DATA) + len;
							if (rc->SendPacketAwaitResponse(&rHead, buf, sizeof(buf)) && rHead.scmd == RCMD_UPLOAD_DATA_ACK) {
								if (rud->offset != block) {
									if (rHead.datalen > sizeof(REMOTE_UPLOAD_DATA)) {
										printf("Send error: %s\n", &buf[sizeof(REMOTE_UPLOAD_DATA)]);
									} else {
										printf("Received ack for wrong offset!\n");
									}
									errored = true;
								}
							} else {
								errored = true;
								if (rHead.scmd == RCMD_GENERIC_ERROR) {
									printf("RadioBot upload error: %s\n", buf);
								} else {
									printf("Unknown reply from RadioBot!\n");
								}
							}
						}
					}
					if (!errored) {
						rHead.ccmd = RCMD_UPLOAD_DONE;
						rHead.datalen = 1;
						buf[0] = xferid;
						if (rc->SendPacketAwaitResponse(&rHead, buf, sizeof(buf)) && rHead.scmd == RCMD_UPLOAD_DONE_ACK) {
							if (buf[0] == 0) {
								printf("Upload complete!\n");
							} else {
								printf("RadioBot upload error: %s\n", &buf[1]);
							}
						} else {
							if (rHead.scmd == RCMD_GENERIC_ERROR) {
								printf("RadioBot upload error: %s\n", buf);
							} else {
								printf("Unknown reply from RadioBot!\n");
							}
						}
					}
				} else if (rHead.scmd == RCMD_UPLOAD_FAILED || rHead.scmd == RCMD_GENERIC_ERROR) {
					printf("RadioBot denied upload: %s\n", buf);
				} else {
					printf("Unknown reply from RadioBot!\n");
				}
			} else {
				printf("Error receiving reply from RadioBot!\n");
			}
		} else {
			printf("File too big, 4GB - 2 byte max\n");
		}
		fclose(fp);
	} else {
		printf("Error opening file for input: %s\n", strerror(errno));
	}
}
