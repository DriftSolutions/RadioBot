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

#include "../../src/plugins.h"
#define TIXML_USE_STL
#include "../../../Common/tinyxml/tinyxml.h"

BOTAPI_DEF *api=NULL;
int pluginnum; // the number we were assigned

struct CENTOVA_CONFIG {
	bool shutdown_now;
	bool playing;

	char url[1024];
	char user[128];
	char pass[128];
};

CENTOVA_CONFIG centova_config;

bool LoadConfig() {
	memset(&centova_config, 0, sizeof(centova_config));
	DS_CONFIG_SECTION * sec = api->config->GetConfigSection(NULL, "Centovacast");

	if (sec == NULL) {
		api->ib_printf(_("Centovacast Support -> Error: You do not have a Centovacast section!\n"));
		return false;
	}

	api->config->GetConfigSectionValueBuf(sec, "URL", centova_config.url, sizeof(centova_config.url));
	api->config->GetConfigSectionValueBuf(sec, "User", centova_config.user, sizeof(centova_config.user));
	api->config->GetConfigSectionValueBuf(sec, "Pass", centova_config.pass, sizeof(centova_config.pass));

	if (centova_config.url[0] == 0 || centova_config.user[0] == 0 || centova_config.pass[0] == 0) {
		api->ib_printf(_("Centovacast Support -> Error: You must provide an API URL, Username, and Password!\n"));
		return false;
	}

	return true;
}

#if (LIBCURL_VERSION_MAJOR < 7) || (LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR < 17)
#define OLD_CURL 1
#else
#undef OLD_CURL
#endif

/*
struct DSL_BUFFER {
	char * data;
	uint32 len;
};
*/

size_t cv_buffer_write(void *ptr, size_t size, size_t count, void *stream) {
	DSL_BUFFER * buf = (DSL_BUFFER *)stream;
	uint32 len = size * count;
	if (len > 0) {
		buffer_append(buf, (char *)ptr, len);
	}
	return count;
}

CURL * cv_prepare_handle(DSL_BUFFER * buf) {
	CURL * h = api->curl->easy_init();
	if (h == NULL) { return NULL; }
	api->curl->easy_setopt(h, CURLOPT_TIMEOUT, 30);
	api->curl->easy_setopt(h, CURLOPT_CONNECTTIMEOUT, 20);
	api->curl->easy_setopt(h, CURLOPT_NOPROGRESS, 1);
	api->curl->easy_setopt(h, CURLOPT_NOSIGNAL, 1 );
	api->curl->easy_setopt(h, CURLOPT_SSL_VERIFYHOST, 0);
	api->curl->easy_setopt(h, CURLOPT_SSL_VERIFYPEER, 0);
	api->curl->easy_setopt(h, CURLOPT_SSLVERSION, CURL_SSLVERSION_TLSv1);
	api->curl->easy_setopt(h, CURLOPT_WRITEFUNCTION, cv_buffer_write);
	api->curl->easy_setopt(h, CURLOPT_WRITEDATA, buf);
	char buf2[128];
	sprintf(buf2, "RadioBot Centovacast Support - www.shoutirc.com (Mozilla)");
	api->curl->easy_setopt(h, CURLOPT_USERAGENT, buf2);
	return h;
}

typedef std::map<string, string> centovaRequestParms;
bool DoCentovaRequest(const char * sclass, const char * smethod, char * result, size_t resultSize, centovaRequestParms * parms=NULL, centovaRequestParms * data_row=NULL) {
	if (result) { result[0] = 0; }
	if (data_row) { data_row->clear(); }
	DSL_BUFFER cvbuf;
	buffer_init(&cvbuf);
	CURL * h = cv_prepare_handle(&cvbuf);
	bool ret = false;
	if (h) {
		api->curl->easy_setopt(h, CURLOPT_URL, centova_config.url);

		TiXmlDocument * doc = new TiXmlDocument();
		TiXmlElement * root = new TiXmlElement("centovacast");
		TiXmlElement * req = new TiXmlElement("request");
		req->SetAttribute("class", sclass);
		req->SetAttribute("method", smethod);

		TiXmlElement * sub = new TiXmlElement("password");
		sub->LinkEndChild(new TiXmlText(centova_config.pass));
		req->LinkEndChild(sub);
		sub = new TiXmlElement("username");
		sub->LinkEndChild(new TiXmlText(centova_config.user));
		req->LinkEndChild(sub);

		if (parms && parms->size()) {
			for (centovaRequestParms::const_iterator x = parms->begin(); x != parms->end(); x++) {
				sub = new TiXmlElement(x->first.c_str());
				sub->LinkEndChild(new TiXmlText(x->second.c_str()));
				req->LinkEndChild(sub);
			}
		}

		root->LinkEndChild(req);
		doc->LinkEndChild(root);

		TiXmlPrinter printer;
		printer.SetStreamPrinting();
		//printer.SetLineBreak("\n");
		doc->Accept(&printer);
		char * post = zstrdup(printer.CStr());
		delete doc;

#ifdef DEBUG
		api->ib_printf("Centovacast Support -> XML Request: %s\n", post);
#endif

		api->curl->easy_setopt(h, CURLOPT_POSTFIELDS, post);
		api->curl->easy_setopt(h, CURLOPT_POSTFIELDSIZE, strlen(post));
		CURLcode code = api->curl->easy_perform(h);
		if (code == CURLE_OK) {
			if (cvbuf.data && cvbuf.len > 0) {
				buffer_append_int(&cvbuf, uint8(0));
#ifdef DEBUG
				api->ib_printf("Centovacast Support -> XML Reply: %s\n", cvbuf.data);
#endif
				doc = new TiXmlDocument();
				doc->Parse(cvbuf.data);
				if (!doc->Error()) {
					root = doc->FirstChildElement("centovacast");
					if (root) {
						TiXmlElement * res = root->FirstChildElement("response");
						if (res) {
							const char * p = res->Attribute("type");
							if (p && !stricmp(p, "success")) {
								ret = true;
								if (data_row) {
									TiXmlElement * data = res->FirstChildElement("data");
									if (data) {
										TiXmlElement * row = data->FirstChildElement("row");
										if (row) {
											TiXmlElement * field = row->FirstChildElement("field");
											while (field) {
												string name = field->Attribute("name");
												string value = field->GetText();
												(*data_row)[name] = value;
												field = field->NextSiblingElement("field");
											}
										}
									}
								}
							} else {
								res = res->FirstChildElement("message");
								if (res) {
									if (result) { strlcpy(result, res->GetText(), resultSize); }
								} else {
									if (result) { strlcpy(result, "Error reply received, but no reason from server...", resultSize); }
									api->ib_printf("Centovacast Support -> Error parsing XML reply: %s\n", cvbuf.data);
								}
							}
						} else {
							if (result) { strlcpy(result, "No 'response' element in response from server...", resultSize); }
							api->ib_printf("Centovacast Support -> Error parsing XML reply: %s\n", cvbuf.data);
						}
					} else {
						if (result) { strlcpy(result, "No 'centovacast' element in response from server...", resultSize); }
						api->ib_printf("Centovacast Support -> Error parsing XML reply: %s\n", cvbuf.data);
					}
				} else {
					if (result) { snprintf(result, resultSize, "Error parsing XML response at %d:%d -> %s (XML dump is on bot console)", doc->ErrorRow(), doc->ErrorCol(), doc->ErrorDesc()); }
					api->ib_printf("Centovacast Support -> Error parsing XML reply: %s\n", cvbuf.data);
				}
				delete doc;
			} else {
				if (result) { strlcpy(result, "Unknown response from server...", resultSize); }
				api->ib_printf("Centovacast Support -> Unknown response from server...\n");
			}
		} else {
			if (result) { strlcpy(result, api->curl->easy_strerror(code), resultSize); }
			api->ib_printf("Centovacast Support -> Error: %s\n", api->curl->easy_strerror(code));
		}

		api->curl->easy_cleanup(h);
		zfree(post);
	} else {
		if (result) { strlcpy(result, "Error creating CURL handle!", resultSize); }
	}
	buffer_free(&cvbuf);
	return ret;
}

int CV_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
	char buf[1024],buf2[1024];
	if (!stricmp(command, "cv-ss-stop")) {
		if (DoCentovaRequest("server", "stop", buf, sizeof(buf))) {
			ut->std_reply(ut, _("Command executed!"));
		} else {
			snprintf(buf2, sizeof(buf2), "Error executing command: %s", buf);
			ut->std_reply(ut, buf2);
		}
		return 1;
	}
	if (!stricmp(command, "cv-ss-start")) {
		if (DoCentovaRequest("server", "start", buf, sizeof(buf))) {
			ut->std_reply(ut, _("Command executed!"));
		} else {
			snprintf(buf2, sizeof(buf2), "Error executing command: %s", buf);
			ut->std_reply(ut, buf2);
		}
		return 1;
	}
	if (!stricmp(command, "cv-ss-restart")) {
		if (DoCentovaRequest("server", "restart", buf, sizeof(buf))) {
			ut->std_reply(ut, _("Command executed!"));
		} else {
			snprintf(buf2, sizeof(buf2), "Error executing command: %s", buf);
			ut->std_reply(ut, buf2);
		}
		return 1;
	}
	if (!stricmp(command, "cv-autodj-start")) {
		centovaRequestParms crp;
		crp["state"] = "up";
		if (DoCentovaRequest("server", "switchsource", buf, sizeof(buf), &crp)) {
			ut->std_reply(ut, _("Command executed!"));
		} else {
			snprintf(buf2, sizeof(buf2), "Error executing command: %s", buf);
			ut->std_reply(ut, buf2);
		}
		return 1;
	}
	if (!stricmp(command, "cv-autodj-stop")) {
		centovaRequestParms crp;
		crp["state"] = "down";
		if (DoCentovaRequest("server", "switchsource", buf, sizeof(buf), &crp)) {
			ut->std_reply(ut, _("Command executed!"));
		} else {
			snprintf(buf2, sizeof(buf2), "Error executing command: %s", buf);
			ut->std_reply(ut, buf2);
		}
		return 1;
	}

	if ((!stricmp(command,"cv-autodj-next") || !stricmp(command,"next")) && (ut->Flags & UFLAG_ADVANCED_SOURCE)) {
		if (DoCentovaRequest("server", "nextsong", buf, sizeof(buf))) {
			ut->std_reply(ut, _("Skipping song..."));
		} else {
			snprintf(buf2, sizeof(buf2), "Error executing command: %s", buf);
			ut->std_reply(ut, buf2);
		}
		return 1;
	}
	return 0;
}

TT_DEFINE_THREAD(CentovaThread) {
	TT_THREAD_START
	char buf[1024];
	while (!centova_config.shutdown_now) {
		centovaRequestParms data;
		if (DoCentovaRequest("system", "info", buf, sizeof(buf), NULL, &data)) {
			buf[0]=buf[0];
			bool playing = false;
			for (centovaRequestParms::const_iterator x = data.begin(); x != data.end(); x++) {
				//api->ib_printf("Resp: %s -> %s\n", x->first.c_str(), x->second.c_str());
				if (!stricmp(x->first.c_str(), "sourcestate")) {
					if (!stricmp(x->second.c_str(), "up")) {
						playing = true;
					}
					break;
				}
			}
			if (playing != centova_config.playing) {
				if (playing) {
					api->SendMessage(-1, SOURCE_I_PLAYING, NULL, 0);
				} else {
					api->SendMessage(-1, SOURCE_I_STOPPED, NULL, 0);
				}
			}
			centova_config.playing = playing;
		} else {
			buf[0]=buf[0];
		}
		for (int i=0; i < 30 && !centova_config.shutdown_now; i++) {
			safe_sleep(1);
		}
	}
	TT_THREAD_END
}

int init(int num, BOTAPI_DEF * BotAPI) {
	api=BotAPI;
	pluginnum=num;
	api->ib_printf(_("Centovacast Support -> Loading...\n"));
	if (api->ss == NULL) {
		api->ib_printf2(pluginnum,_("Centovacast Support -> This version of RadioBot is not supported!\n"));
		return 0;
	}
	if (!LoadConfig()) {
		return 0;
	}

	COMMAND_ACL acl;

	api->commands->FillACL(&acl, 0, UFLAG_MASTER|UFLAG_OP, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "cv-ss-stop", &acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE, CV_Commands, _("This command will tell Centovacast to shut down your Shoutcast server"));
	api->commands->RegisterCommand_Proc(pluginnum, "cv-ss-start", &acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE, CV_Commands, _("This command will tell Centovacast to start your Shoutcast server"));
	api->commands->RegisterCommand_Proc(pluginnum, "cv-ss-restart", &acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE, CV_Commands, _("This command will tell Centovacast to restart your Shoutcast server"));

	api->commands->FillACL(&acl, 0, UFLAG_ADVANCED_SOURCE, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "cv-autodj-stop", &acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE, CV_Commands, _("This command will tell Centovacast to shut down it's AutoDJ"));
	api->commands->RegisterCommand_Proc(pluginnum, "cv-autodj-start", &acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE, CV_Commands, _("This command will tell Centovacast to start it's AutoDJ"));
	api->commands->RegisterCommand_Proc(pluginnum, "cv-autodj-next", &acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE, CV_Commands, _("This command will tell Centovacast to skip to the next song in it's AutoDJ"));

	api->commands->FillACL(&acl, 0, 0, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "next", &acl, CM_ALLOW_IRC_PUBLIC|CM_ALLOW_CONSOLE_PUBLIC, CV_Commands, _("This command will tell Centovacast to skip to the next song in it's AutoDJ"));
	TT_StartThread(CentovaThread, NULL, "Centova Thread", pluginnum);
	api->ib_printf(_("Centovacast Support -> Loaded.\n"));
	return 1;
}

void quit() {
	api->ib_printf(_("Centovacast Support -> Shutting down...\n"));
	centova_config.shutdown_now = true;
	int left=11;
	while (TT_NumThreadsWithID(pluginnum) > 0) {
		api->ib_printf(_("Centovacast Support -> Waiting for threads to exit (will wait %d more seconds)...\n"), left);
		safe_sleep(1);
		left--;
		if (left <= 0) { break; }
	}
	api->commands->UnregisterCommandsByPlugin(pluginnum);
	api->ib_printf(_("Centovacast Support -> Shut down.\n"));
}

#define cv_poly 0xEDB88320

/* On entry, addr=>start of data
             num = length of data
             crc = incoming CRC     */
int cv_crc32(char *addr, int num, int crc)
{
int i;

for (; num>0; num--)              /* Step through bytes in memory */
  {
  crc = crc ^ *addr++;            /* Fetch byte from memory, XOR into CRC */
  for (i=0; i<8; i++)             /* Prepare to rotate 8 bits */
  {
    if (crc & 1)                  /* b0 is set... */
      crc = (crc >> 1) ^ cv_poly;    /* rotate and XOR with ZIP polynomic */
    else                          /* b0 is clear... */
      crc >>= 1;                  /* just rotate */
  /* Some compilers need:
    crc &= 0xFFFFFFFF;
   */
    }                             /* Loop for 8 bits */
  }                               /* Loop until num=0 */
  return(crc);                    /* Return updated CRC */
}

int message(unsigned int MsgID, char * data, int datalen) {
	char buf[1024];
	switch(MsgID) {

		case SOURCE_C_PLAY:{
				centovaRequestParms crp;
				crp["state"] = "up";
				if (!DoCentovaRequest("server", "switchsource", buf, sizeof(buf), &crp)) {
					api->ib_printf("Centova Support -> Error executing play command: %s\n", buf);
				}
				return 1;
			}
			break;

		case SOURCE_C_NEXT:
			if (centova_config.playing) {
				if (!DoCentovaRequest("server", "nextsong", buf, sizeof(buf))) {
					api->ib_printf("Centova Support -> Error executing next command: %s\n", buf);
				}
				return 1;
			}
			break;

		case SOURCE_C_STOP:
			if (centova_config.playing) {
				centovaRequestParms crp;
				crp["state"] = "down";
				api->SendMessage(-1, SOURCE_I_STOPPING, NULL, 0);
				if (!DoCentovaRequest("server", "switchsource", buf, sizeof(buf), &crp)) {
					api->ib_printf("Centova Support -> Error executing stop command: %s\n", buf);
				}
				return 1;
			}
			break;

		case SOURCE_IS_PLAYING:
			if (centova_config.playing) {
				return 1;
			}
			break;

		case SOURCE_GET_SONGID:{
				if (centova_config.playing) {
					uint32 * id = (uint32 *)data;
					STATS *s = api->ss->GetStreamInfo();
					if (s && s->cursong[0]) {
						*id = cv_crc32(s->cursong, strlen(s->cursong), 0xFFFFFFFF);
						return 1;
					}
				}
			}
			break;
	}
	return 0;
}

int remote(USER_PRESENCE * ut, unsigned char cliversion, REMOTE_HEADER * head, char * data) {
	char buf[512];
	if ((head->ccmd == RCMD_SRC_COUNTDOWN && (ut->Flags & UFLAG_BASIC_SOURCE)) || (head->ccmd == RCMD_SRC_FORCE_OFF && (ut->Flags & UFLAG_ADVANCED_SOURCE))) {
		if (centova_config.playing) {
			sstrcpy(buf,_("Stopping immediately..."));
			api->SendMessage(-1, SOURCE_C_STOP, NULL, 0);
		} else {
			sstrcpy(buf,_("Centovacast is not currently playing..."));
		}
		head->scmd = RCMD_GENERIC_MSG;
		head->datalen = strlen(buf);
		api->SendRemoteReply(ut->Sock,head,buf);
		return 1;
	}

	if (head->ccmd == RCMD_SRC_FORCE_ON && (ut->Flags & UFLAG_BASIC_SOURCE)) {
		sstrcpy(buf,_("Playing..."));
		head->scmd = RCMD_GENERIC_MSG;
		head->datalen = strlen(buf);
		api->SendRemoteReply(ut->Sock,head,buf);
		api->SendMessage(-1, SOURCE_C_PLAY, NULL, 0);
		return 1;
	}

	if (head->ccmd == RCMD_SRC_NEXT && (ut->Flags & UFLAG_ADVANCED_SOURCE)) {
		if (centova_config.playing) {
			sstrcpy(buf,_("Skipping song..."));
			head->scmd = RCMD_GENERIC_MSG;
			head->datalen = strlen(buf);
			api->SendRemoteReply(ut->Sock,head,buf);
			api->SendMessage(-1, SOURCE_C_NEXT, NULL, 0);
		} else {
			sstrcpy(buf,_("Centovacast is not currently playing..."));
			head->scmd = RCMD_GENERIC_MSG;
			head->datalen = strlen(buf);
			api->SendRemoteReply(ut->Sock,head,buf);
		}
		return 1;
	}

	if (head->ccmd == RCMD_SRC_STATUS) {
		if (centova_config.playing) {
			sstrcpy(buf, "playing");
		} else {
			sstrcpy(buf, "stopped");
		}
		head->scmd = RCMD_GENERIC_MSG;
		head->datalen = strlen(buf);
		api->SendRemoteReply(ut->Sock,head,buf);
		return 1;
	}

	if (head->ccmd == RCMD_SRC_GET_NAME) {
		sstrcpy(buf, "centovacast");
		head->scmd = RCMD_GENERIC_MSG;
		head->datalen = strlen(buf);
		api->SendRemoteReply(ut->Sock,head,buf);
		return 1;
	}

	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{B64F8C6C-546E-4784-90D8-DDF2E86F29AF}",
	"Centovacast Support",
	"Centovacast Support Plugin 1.0",
	"Drift Solutions",
	init,
	quit,
	message,
	NULL,
	NULL,
	NULL,
	remote,
};

PLUGIN_EXPORT_FULL
