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

//Titus_Sockets3 sockets;

BOTAPI_DEF *api=NULL;
int pluginnum; // the number we were assigned

struct SS_CONFIG {
	char passwords[MAX_SOUND_SERVERS][256];
	char iptban[1024];
	char iptunban[1024];
};

SS_CONFIG ssc;

bool LoadConfig() {
	memset(&ssc, 0, sizeof(ssc));
	DS_CONFIG_SECTION * sec = api->config->GetConfigSection(NULL, "SS_Admin");

	if (sec == NULL) {
		api->ib_printf(_("SS Admin -> Error: You do not have an SS_Admin section!\n"));
		return false;
	}

	bool hadpass=false;
	char buf[16];
	for (int i=0; i < MAX_SOUND_SERVERS; i++) {
		sprintf(buf,"SS%dPass",i);
		api->config->GetConfigSectionValueBuf(sec, buf, ssc.passwords[i], sizeof(ssc.passwords[i]));
		if (ssc.passwords[i][0] == 0) {
			SOUND_SERVER ss;
			api->ss->GetSSInfo(i, &ss);
			strlcpy(ssc.passwords[i], ss.admin_pass, sizeof(ssc.passwords[i]));
		}
		if (strlen(ssc.passwords[i])) { hadpass=true; }
	}

	api->config->GetConfigSectionValueBuf(sec, "iptunban", ssc.iptunban, sizeof(ssc.iptunban));
	api->config->GetConfigSectionValueBuf(sec, "iptban", ssc.iptban, sizeof(ssc.iptban));

	if (!hadpass) {
		api->ib_printf(_("SS Admin -> Error: You did not set any sound server passwords!\n"));
	}

	return hadpass;
}

#if (LIBCURL_VERSION_MAJOR < 7) || (LIBCURL_VERSION_MAJOR == 7 && LIBCURL_VERSION_MINOR < 17)
#define OLD_CURL 1
#else
#undef OLD_CURL
#endif

struct SSA_BUFFER {
	CURL * handle;
	CURLcode result;
	char * data;
	uint32 len;
#if defined(OLD_CURL)
	char *url, *userpwd;
#endif
};

size_t ssa_buffer_write(void *ptr, size_t size, size_t count, void *stream) {
	SSA_BUFFER * buf = (SSA_BUFFER *)stream;
	uint32 len = size * count;
	if (len > 0) {
		buf->data = (char *)zrealloc(buf->data, buf->len + len + 1);
		memcpy(buf->data+buf->len, ptr, len);
		buf->len += len;
		buf->data[buf->len] = 0;
	}
	return count;
}

CURL * ssa_prepare_handle(SSA_BUFFER * buf) {
	CURL * h = api->curl->easy_init();
	api->curl->easy_setopt(h, CURLOPT_TIMEOUT, 30);
	api->curl->easy_setopt(h, CURLOPT_CONNECTTIMEOUT, 20);
	api->curl->easy_setopt(h, CURLOPT_NOPROGRESS, 1);
	api->curl->easy_setopt(h, CURLOPT_NOSIGNAL, 1 );
	api->curl->easy_setopt(h, CURLOPT_WRITEFUNCTION, ssa_buffer_write);
	api->curl->easy_setopt(h, CURLOPT_WRITEDATA, buf);
	char buf2[128];
	sprintf(buf2, "RadioBot SS Admin - www.shoutirc.com (Mozilla)");
	api->curl->easy_setopt(h, CURLOPT_USERAGENT, buf2);
	return h;
}

void DoBan(USER_PRESENCE * ut, int ip1, int ip2, int ip3, int ip4, int mask) {
	SSA_BUFFER bufs[MAX_SOUND_SERVERS];
	memset(&bufs, 0, sizeof(bufs));
	CURLM * mh = api->curl->multi_init();
	int i;
	char buf[1024];//,buf2[128];
	int num = api->ss->GetSSInfo(-1, NULL);
	for (i=0; i < num; i++) {
		SOUND_SERVER ss;
		api->ss->GetSSInfo(i, &ss);
		if (ss.type == SS_TYPE_SHOUTCAST || ss.type == SS_TYPE_SHOUTCAST2) {
			if (strlen(ssc.passwords[i])) {
				bufs[i].handle = ssa_prepare_handle(&bufs[i]);
				if (bufs[i].handle) {
					sprintf(buf,"http://%s:%d/admin.cgi?mode=banip&ip1=%d&ip2=%d&ip3=%d&ip4=%d&banmsk=%d",ss.host,ss.port,ip1,ip2,ip3,ip4,mask);
					stringstream sstr,sstr2;
					sstr << buf;
					if (ss.type == SS_TYPE_SHOUTCAST2) {
						sstr << "&sid=" << ss.streamid;
					}
					sstr2 << "admin:" << ssc.passwords[i];
#if defined(OLD_CURL)
					bufs[i].url = zstrdup(sstr.str().c_str());
					api->curl->easy_setopt(bufs[i].handle, CURLOPT_URL, bufs[i].url);
					bufs[i].userpwd = zstrdup(sstr2.str().c_str());
					api->curl->easy_setopt(bufs[i].handle, CURLOPT_USERPWD, bufs[i].userpwd);
#else
					api->curl->easy_setopt(bufs[i].handle, CURLOPT_URL, sstr.str().c_str());
					api->curl->easy_setopt(bufs[i].handle, CURLOPT_USERPWD, sstr2.str().c_str());
#endif
					api->curl->multi_add_handle(mh, bufs[i].handle);
				}
			} else {
				sprintf(buf,_("Skipping server [%d] (no pass defined)"),i);
				ut->std_reply(ut, buf);
			}
		} else {
			sprintf(buf,_("Skipping server [%d] (not shoutcast)"),i);
			ut->std_reply(ut, buf);
		}
	}
	while (i > 0) {
		//CURLMcode cret =
		api->curl->multi_perform(mh, &i);
		safe_sleep(10, true);
	}
	CURLMsg * msg = api->curl->multi_info_read(mh, &i);
	while (msg) {
		if (msg->msg == CURLMSG_DONE) {
			for (i=0; i < num; i++) {
				if (bufs[i].handle == msg->easy_handle) {
					bufs[i].result = msg->data.result;
					break;
				}
			}
		}
		msg = api->curl->multi_info_read(mh, &i);
	}
	for (i=0; i < num; i++) {
		if (bufs[i].handle) {
			if (bufs[i].result == CURLE_OK) {
				if (stristr(bufs[i].data, "Unauthorized") == NULL) {
					sprintf(buf, _("Banned IP on server %d ..."), i);
				} else {
					sprintf(buf, _("Error banning IP on server [%d]: Invalid password"), i);
				}
			} else {
				sprintf(buf, _("Error banning IP on server [%d]: %d -> %s"), i, bufs[i].result, api->curl->easy_strerror(bufs[i].result));
			}
			ut->std_reply(ut, buf);
			api->curl->multi_remove_handle(mh, bufs[i].handle);
			api->curl->easy_cleanup(bufs[i].handle);
		}
		if (bufs[i].data) { zfree(bufs[i].data); }
#if defined(OLD_CURL)
		if (bufs[i].url) { zfree(bufs[i].url); }
		if (bufs[i].userpwd) { zfree(bufs[i].userpwd); }
#endif
	}
	api->curl->multi_cleanup(mh);
}

void DoUnban(USER_PRESENCE * ut, int ip1, int ip2, int ip3, int ip4, int mask) {
	SSA_BUFFER bufs[MAX_SOUND_SERVERS];
	memset(&bufs, 0, sizeof(bufs));
	CURLM * mh = api->curl->multi_init();
	int i;
	char buf[1024];//,buf2[128];
	int num = api->ss->GetSSInfo(-1, NULL);
	for (i=0; i < num; i++) {
		SOUND_SERVER ss;
		api->ss->GetSSInfo(i, &ss);
		if (ss.type == SS_TYPE_SHOUTCAST || ss.type == SS_TYPE_SHOUTCAST2) {
			if (strlen(ssc.passwords[i])) {
				bufs[i].handle = ssa_prepare_handle(&bufs[i]);
				if (bufs[i].handle) {
					sprintf(buf,"http://%s:%d/admin.cgi?mode=unbandst&bandst=", ss.host, ss.port);//%d&ip2=%d&ip3=%d&ip4=%d&banmsk=%d",,ip1,ip2,ip3,ip4,mask);
					stringstream sstr,sstr2;
					sstr << buf;
					if (ss.type == SS_TYPE_SHOUTCAST2) {
						sprintf(buf, "%d.%d.%d.%d&banmsk=%d",ip1,ip2,ip3,ip4,mask);
						sstr << buf;
						sstr << "&sid=" << ss.streamid;
					} else {
						uint32 ip=0;
						if (mask == 255) {
							ip |= ip4 << 24;
						}
						ip |= ip3 << 16;
						ip |= ip2 << 8;
						ip |= ip1;
						sstr << ip;
					}
					sstr2 << "admin:" << ssc.passwords[i];
#if defined(OLD_CURL)
					bufs[i].url = zstrdup(sstr.str().c_str());
					api->curl->easy_setopt(bufs[i].handle, CURLOPT_URL, bufs[i].url);
					bufs[i].userpwd = zstrdup(sstr2.str().c_str());
					api->curl->easy_setopt(bufs[i].handle, CURLOPT_USERPWD, bufs[i].userpwd);
#else
					api->curl->easy_setopt(bufs[i].handle, CURLOPT_URL, sstr.str().c_str());
					api->curl->easy_setopt(bufs[i].handle, CURLOPT_USERPWD, sstr2.str().c_str());
#endif
					api->curl->multi_add_handle(mh, bufs[i].handle);
				}
			} else {
				sprintf(buf,_("Skipping server [%d] (no pass defined)"),i);
				ut->std_reply(ut, buf);
			}
		} else {
			sprintf(buf,_("Skipping server [%d] (not shoutcast)"),i);
			ut->std_reply(ut, buf);
		}
	}
	while (i > 0) {
		//CURLMcode cret =
		api->curl->multi_perform(mh, &i);
		safe_sleep(10, true);
	}
	CURLMsg * msg = api->curl->multi_info_read(mh, &i);
	while (msg) {
		if (msg->msg == CURLMSG_DONE) {
			for (i=0; i < num; i++) {
				if (bufs[i].handle == msg->easy_handle) {
					bufs[i].result = msg->data.result;
					break;
				}
			}
		}
		msg = api->curl->multi_info_read(mh, &i);
	}
	for (i=0; i < num; i++) {
		if (bufs[i].handle) {
			if (bufs[i].result == CURLE_OK) {
				if (stristr(bufs[i].data, "Unauthorized") == NULL) {
					sprintf(buf, _("Unbanned IP on server %d ..."), i);
				} else {
					sprintf(buf, _("Error unbanning IP on server [%d]: Invalid password"), i);
				}
			} else {
				sprintf(buf, _("Error unbanning IP on server [%d]: %d -> %s"), i, bufs[i].result, api->curl->easy_strerror(bufs[i].result));
			}
			ut->std_reply(ut, buf);
			api->curl->multi_remove_handle(mh, bufs[i].handle);
			api->curl->easy_cleanup(bufs[i].handle);
		}
		if (bufs[i].data) { zfree(bufs[i].data); }
#if defined(OLD_CURL)
		if (bufs[i].url) { zfree(bufs[i].url); }
		if (bufs[i].userpwd) { zfree(bufs[i].userpwd); }
#endif
	}
	api->curl->multi_cleanup(mh);
}

void DoRIP(USER_PRESENCE * ut, int ip1, int ip2, int ip3, int ip4) {
	SSA_BUFFER bufs[MAX_SOUND_SERVERS];
	memset(&bufs, 0, sizeof(bufs));
	CURLM * mh = api->curl->multi_init();
	int i;
	char buf[1024];//,buf2[128];
	int num = api->ss->GetSSInfo(-1, NULL);
	for (i=0; i < num; i++) {
		SOUND_SERVER ss;
		api->ss->GetSSInfo(i, &ss);
		if (ss.type == SS_TYPE_SHOUTCAST || ss.type == SS_TYPE_SHOUTCAST2) {
			if (strlen(ssc.passwords[i])) {
				bufs[i].handle = ssa_prepare_handle(&bufs[i]);
				if (bufs[i].handle) {
					sprintf(buf,"http://%s:%d/admin.cgi?mode=ripip&ip1=%d&ip2=%d&ip3=%d&ip4=%d",ss.host,ss.port,ip1,ip2,ip3,ip4);
					stringstream sstr,sstr2;
					sstr << buf;
					if (ss.type == SS_TYPE_SHOUTCAST2) {
						sstr << "&sid=" << ss.streamid;
					}
					sstr2 << "admin:" << ssc.passwords[i];
#if defined(OLD_CURL)
					bufs[i].url = zstrdup(sstr.str().c_str());
					api->curl->easy_setopt(bufs[i].handle, CURLOPT_URL, bufs[i].url);
					bufs[i].userpwd = zstrdup(sstr2.str().c_str());
					api->curl->easy_setopt(bufs[i].handle, CURLOPT_USERPWD, bufs[i].userpwd);
#else
					api->curl->easy_setopt(bufs[i].handle, CURLOPT_URL, sstr.str().c_str());
					api->curl->easy_setopt(bufs[i].handle, CURLOPT_USERPWD, sstr2.str().c_str());
#endif
					api->curl->multi_add_handle(mh, bufs[i].handle);
				}
			} else {
				sprintf(buf,_("Skipping server [%d] (no pass defined)"),i);
				ut->std_reply(ut, buf);
			}
		} else {
			sprintf(buf,_("Skipping server [%d] (not shoutcast)"),i);
			ut->std_reply(ut, buf);
		}
	}
	while (i > 0) {
		//CURLMcode cret =
		api->curl->multi_perform(mh, &i);
		safe_sleep(10, true);
	}
	CURLMsg * msg = api->curl->multi_info_read(mh, &i);
	while (msg) {
		if (msg->msg == CURLMSG_DONE) {
			for (i=0; i < num; i++) {
				if (bufs[i].handle == msg->easy_handle) {
					bufs[i].result = msg->data.result;
					break;
				}
			}
		}
		msg = api->curl->multi_info_read(mh, &i);
	}
	for (i=0; i < num; i++) {
		if (bufs[i].handle) {
			if (bufs[i].result == CURLE_OK) {
				if (stristr(bufs[i].data, "Unauthorized") == NULL) {
					sprintf(buf, _("Added RIP entry on server %d ..."), i);
				} else {
					sprintf(buf, _("Error adding RIP entry on server [%d]: Invalid password"), i);
				}
			} else {
				sprintf(buf, _("Error adding RIP entry on server [%d]: %d -> %s"), i, bufs[i].result, api->curl->easy_strerror(bufs[i].result));
			}
			ut->std_reply(ut, buf);
			api->curl->multi_remove_handle(mh, bufs[i].handle);
			api->curl->easy_cleanup(bufs[i].handle);
		}
		if (bufs[i].data) { zfree(bufs[i].data); }
#if defined(OLD_CURL)
		if (bufs[i].url) { zfree(bufs[i].url); }
		if (bufs[i].userpwd) { zfree(bufs[i].userpwd); }
#endif
	}
	api->curl->multi_cleanup(mh);
}

void DoUnRIP(USER_PRESENCE * ut, int ip1, int ip2, int ip3, int ip4) {
	SSA_BUFFER bufs[MAX_SOUND_SERVERS];
	memset(&bufs, 0, sizeof(bufs));
	CURLM * mh = api->curl->multi_init();
	int i;
	char buf[1024];//,buf2[128];
	int num = api->ss->GetSSInfo(-1, NULL);
	for (i=0; i < num; i++) {
		SOUND_SERVER ss;
		api->ss->GetSSInfo(i, &ss);
		if (ss.type == SS_TYPE_SHOUTCAST || ss.type == SS_TYPE_SHOUTCAST2) {
			if (strlen(ssc.passwords[i])) {
				bufs[i].handle = ssa_prepare_handle(&bufs[i]);
				if (bufs[i].handle) {
					sprintf(buf,"http://%s:%d/admin.cgi?mode=unripdst&ripdst=", ss.host, ss.port);//%d&ip2=%d&ip3=%d&ip4=%d&banmsk=%d",,ip1,ip2,ip3,ip4,mask);
					stringstream sstr,sstr2;
					sstr << buf;
					if (ss.type == SS_TYPE_SHOUTCAST2) {
						sprintf(buf, "%d.%d.%d.%d",ip1,ip2,ip3,ip4);
						sstr << buf;
						sstr << "&sid=" << ss.streamid;
					} else {
						uint32 ip=0;
						ip |= ip4 << 24;
						ip |= ip3 << 16;
						ip |= ip2 << 8;
						ip |= ip1;
						sstr << ip;
					}
					sstr2 << "admin:" << ssc.passwords[i];
#if defined(OLD_CURL)
					bufs[i].url = zstrdup(sstr.str().c_str());
					api->curl->easy_setopt(bufs[i].handle, CURLOPT_URL, bufs[i].url);
					bufs[i].userpwd = zstrdup(sstr2.str().c_str());
					api->curl->easy_setopt(bufs[i].handle, CURLOPT_USERPWD, bufs[i].userpwd);
#else
					api->curl->easy_setopt(bufs[i].handle, CURLOPT_URL, sstr.str().c_str());
					api->curl->easy_setopt(bufs[i].handle, CURLOPT_USERPWD, sstr2.str().c_str());
#endif
					api->curl->multi_add_handle(mh, bufs[i].handle);
				}
			} else {
				sprintf(buf,_("Skipping server [%d] (no pass defined)"),i);
				ut->std_reply(ut, buf);
			}
		} else {
			sprintf(buf,_("Skipping server [%d] (not shoutcast)"),i);
			ut->std_reply(ut, buf);
		}
	}
	while (i > 0) {
		//CURLMcode cret =
		api->curl->multi_perform(mh, &i);
		safe_sleep(10, true);
	}
	CURLMsg * msg = api->curl->multi_info_read(mh, &i);
	while (msg) {
		if (msg->msg == CURLMSG_DONE) {
			for (i=0; i < num; i++) {
				if (bufs[i].handle == msg->easy_handle) {
					bufs[i].result = msg->data.result;
					break;
				}
			}
		}
		msg = api->curl->multi_info_read(mh, &i);
	}
	for (i=0; i < num; i++) {
		if (bufs[i].handle) {
			if (bufs[i].result == CURLE_OK) {
				if (stristr(bufs[i].data, "Unauthorized") == NULL) {
					sprintf(buf, _("Removed RIP entry on server %d ..."), i);
				} else {
					sprintf(buf, _("Error removing RIP entry on server [%d]: Invalid password"), i);
				}
			} else {
				sprintf(buf, _("Error removing RIP entry on server [%d]: %d -> %s"), i, bufs[i].result, api->curl->easy_strerror(bufs[i].result));
			}
			ut->std_reply(ut, buf);
			api->curl->multi_remove_handle(mh, bufs[i].handle);
			api->curl->easy_cleanup(bufs[i].handle);
		}
		if (bufs[i].data) { zfree(bufs[i].data); }
#if defined(OLD_CURL)
		if (bufs[i].url) { zfree(bufs[i].url); }
		if (bufs[i].userpwd) { zfree(bufs[i].userpwd); }
#endif
	}
	api->curl->multi_cleanup(mh);
}

void DoKickSrc(USER_PRESENCE * ut) {
	SSA_BUFFER bufs[MAX_SOUND_SERVERS];
	memset(&bufs, 0, sizeof(bufs));
	CURLM * mh = api->curl->multi_init();
	int i;
	char buf[1024];//,buf2[128];
	int num = api->ss->GetSSInfo(-1, NULL);
	for (i=0; i < num; i++) {
		SOUND_SERVER ss;
		api->ss->GetSSInfo(i, &ss);
		if (ss.type == SS_TYPE_SHOUTCAST || ss.type == SS_TYPE_SHOUTCAST2) {
			if (strlen(ssc.passwords[i])) {
				bufs[i].handle = ssa_prepare_handle(&bufs[i]);
				if (bufs[i].handle) {
					sprintf(buf,"http://%s:%d/admin.cgi?mode=kicksrc", ss.host, ss.port);
					stringstream sstr,sstr2;
					sstr << buf;
					if (ss.type == SS_TYPE_SHOUTCAST2) {
						sstr << "&sid=" << ss.streamid;
					}
					sstr2 << "admin:" << ssc.passwords[i];
#if defined(OLD_CURL)
					bufs[i].url = zstrdup(sstr.str().c_str());
					api->curl->easy_setopt(bufs[i].handle, CURLOPT_URL, bufs[i].url);
					bufs[i].userpwd = zstrdup(sstr2.str().c_str());
					api->curl->easy_setopt(bufs[i].handle, CURLOPT_USERPWD, bufs[i].userpwd);
#else
					api->curl->easy_setopt(bufs[i].handle, CURLOPT_URL, sstr.str().c_str());
					api->curl->easy_setopt(bufs[i].handle, CURLOPT_USERPWD, sstr2.str().c_str());
#endif
					api->curl->multi_add_handle(mh, bufs[i].handle);
				}
			} else {
				sprintf(buf,_("Skipping server [%d] (no pass defined)"),i);
				ut->std_reply(ut, buf);
			}
		} else {
			sprintf(buf,_("Skipping server [%d] (not shoutcast)"),i);
			ut->std_reply(ut, buf);
		}
	}
	while (i > 0) {
		//CURLMcode cret =
		api->curl->multi_perform(mh, &i);
		safe_sleep(10, true);
	}
	CURLMsg * msg = api->curl->multi_info_read(mh, &i);
	while (msg) {
		if (msg->msg == CURLMSG_DONE) {
			for (i=0; i < num; i++) {
				if (bufs[i].handle == msg->easy_handle) {
					bufs[i].result = msg->data.result;
					break;
				}
			}
		}
		msg = api->curl->multi_info_read(mh, &i);
	}
	for (i=0; i < num; i++) {
		if (bufs[i].handle) {
			if (bufs[i].result == CURLE_OK) {
				if (stristr(bufs[i].data, "Unauthorized") == NULL) {
					sprintf(buf, _("Kicked source on server %d ..."), i);
				} else {
					sprintf(buf, _("Error kicking source on server [%d]: Invalid password"), i);
				}
			} else {
				sprintf(buf, _("Error kicking source on server [%d]: %d -> %s"), i, bufs[i].result, api->curl->easy_strerror(bufs[i].result));
			}
			ut->std_reply(ut, buf);
			api->curl->multi_remove_handle(mh, bufs[i].handle);
			api->curl->easy_cleanup(bufs[i].handle);
		}
		if (bufs[i].data) { zfree(bufs[i].data); }
#if defined(OLD_CURL)
		if (bufs[i].url) { zfree(bufs[i].url); }
		if (bufs[i].userpwd) { zfree(bufs[i].userpwd); }
#endif
	}
	api->curl->multi_cleanup(mh);
}

void SS_Help(USER_PRESENCE * ut) {
	ut->std_reply(ut, _("SS Admin Help"));
	ut->std_reply(ut, _("ss ban x.x.x[.y]"));
	ut->std_reply(ut, _("ss unban x.x.x[.y]"));
	ut->std_reply(ut, _("ss rip ip"));
	ut->std_reply(ut, _("ss unrip ip"));
	ut->std_reply(ut, _("ss kicksrc"));
	//ut->std_reply(ut, "ss kick x.x.x.x");
	if (strlen(ssc.iptban)) {
		ut->std_reply(ut, _("ss iptban [parms]"));
	}
	if (strlen(ssc.iptunban)) {
		ut->std_reply(ut, _("ss iptunban [parms]"));
	}
}

int SS_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
	if (!stricmp(command, "ss") && (!parms || !strlen(parms))) {
		SS_Help(ut);
		return 1;
	}

	if (!stricmp(command, "ss")) {
		StrTokenizer st((char *)parms, ' ');

		char buf[2048];
		std::string cmd = st.stdGetSingleTok(1);

		if (st.NumTok() >= 2) {
			if (!stricmp(cmd.c_str(), "ban")) {
				StrTokenizer st2((char *)st.stdGetSingleTok(2).c_str(),'.');
				if (st2.NumTok() < 3 || st2.NumTok() > 4) {
					ut->std_reply(ut, _("Syntax: ss ban x.x.x[.y]"));
				} else {
					char ip4[16]="0";
					int mask=0;
					if (st2.NumTok() == 4) {
						mask = 255;
						sstrcpy(ip4, st2.stdGetSingleTok(4).c_str());
					}
					DoBan(ut, atoi(st2.stdGetSingleTok(1).c_str()), atoi(st2.stdGetSingleTok(2).c_str()), atoi(st2.stdGetSingleTok(3).c_str()), atoi(ip4), mask);
				}
			} else if (!stricmp(cmd.c_str(), "unban")) {
				StrTokenizer st2((char *)st.stdGetSingleTok(2).c_str(),'.');
				if (st2.NumTok() < 3 || st2.NumTok() > 4) {
					ut->std_reply(ut, _("Syntax: ss unban x.x.x[.y]"));
				} else {
					char ip4[16]="0";
					int mask=0;
					if (st2.NumTok() == 4) {
						mask = 255;
						sstrcpy(ip4, st2.stdGetSingleTok(4).c_str());
					}
					DoUnban(ut, atoi(st2.stdGetSingleTok(1).c_str()), atoi(st2.stdGetSingleTok(2).c_str()), atoi(st2.stdGetSingleTok(3).c_str()), atoi(ip4), mask);
				}
			} else if (!stricmp(cmd.c_str(), "rip")) {
				StrTokenizer st2((char *)st.stdGetSingleTok(2).c_str(),'.');
				if (st2.NumTok() != 4) {
					ut->std_reply(ut, _("Syntax: ss rip x.x.x.x"));
				} else {
					DoRIP(ut, atoi(st2.stdGetSingleTok(1).c_str()), atoi(st2.stdGetSingleTok(2).c_str()), atoi(st2.stdGetSingleTok(3).c_str()), atoi(st2.stdGetSingleTok(4).c_str()));
				}
			} else if (!stricmp(cmd.c_str(), "unrip")) {
				StrTokenizer st2((char *)st.stdGetSingleTok(2).c_str(),'.');
				if (st2.NumTok() != 4) {
					ut->std_reply(ut, _("Syntax: ss unrip x.x.x.x"));
				} else {
					DoUnRIP(ut, atoi(st2.stdGetSingleTok(1).c_str()), atoi(st2.stdGetSingleTok(2).c_str()), atoi(st2.stdGetSingleTok(3).c_str()), atoi(st2.stdGetSingleTok(4).c_str()));
				}
			} else if (!stricmp(cmd.c_str(), "iptban")) {
				if (strlen(ssc.iptban)) {
					sstrcpy(buf, ssc.iptban);
					char buf2[16];
					for (unsigned long i=2; i <= st.NumTok(); i++) {
						sprintf(buf2,"%%%u%%", i-1);
						str_replace(buf, sizeof(buf), buf2, st.stdGetSingleTok(i).c_str());
					}
					system(buf);
					ut->std_reply(ut, _("Command executed!"));
				} else {
					ut->std_reply(ut, _("Error: You do not have an iptban command set!"));
				}
			} else if (!stricmp(cmd.c_str(), "iptunban")) {
				if (strlen(ssc.iptunban)) {
					sstrcpy(buf, ssc.iptunban);
					char buf2[16];
					for (unsigned long i=2; i <= st.NumTok(); i++) {
						sprintf(buf2,"%%%u%%", i-1);
						str_replace(buf, sizeof(buf), buf2, st.stdGetSingleTok(i).c_str());
					}
					system(buf);
					ut->std_reply(ut, _("Command executed!"));
				} else {
					ut->std_reply(ut, _("Error: You do not have an iptunban command set!"));
				}
			} else {
				SS_Help(ut);
			}
		} else {
			if (!stricmp(cmd.c_str(), "kicksrc")) {
				DoKickSrc(ut);
				if ((type & (CM_ALLOW_IRC_PUBLIC|CM_ALLOW_IRC_PRIVATE)) != 0 && api->irc && api->LoadMessage("OnKickSource", buf, sizeof(buf))) {
					str_replace(buf, sizeof(buf), "%nick", ut->Nick);
					if (ut->Channel) {
						str_replace(buf, sizeof(buf), "%chan", ut->Channel);
					}
					api->ProcText(buf, sizeof(buf));
					strlcat(buf, "\r\n", sizeof(buf));
					api->irc->SendIRC(ut->NetNo, buf, strlen(buf));
				}
			} else {
				SS_Help(ut);
			}
		}
		return 1;
	}
	return 0;
}


int init(int num, BOTAPI_DEF * BotAPI) {
	api=BotAPI;
	pluginnum=num;
	api->ib_printf(_("SS Admin -> Loading...\n"));
	if (api->ss == NULL) {
		api->ib_printf(_("SS Admin -> This plugin will not run on RadioBot Lite!\n"));
		return 0;
	}
	if (!LoadConfig()) {
		return 0;
	}
	COMMAND_ACL acl;
	api->commands->FillACL(&acl, 0, UFLAG_MASTER|UFLAG_OP, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "ss", &acl, CM_ALLOW_ALL, SS_Commands, _("This is for administering your Sound Servers"));
	return 1;
}

void quit() {
	api->ib_printf(_("SS Admin -> Shutting down...\n"));
	api->commands->UnregisterCommandByName("ss");
	api->ib_printf(_("SS Admin -> Shut down.\n"));
}

int message(unsigned int MsgID, char * data, int datalen) {
	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{FDA13FCE-5357-4f74-B22A-B784A6E9A2FB}",
	"SS Admin",
	"Sound Server (SS) Admin Plugin 1.0",
	"Drift Solutions",
	init,
	quit,
	message,
	NULL,
	NULL,
	NULL,
	NULL,
};

PLUGIN_EXPORT_FULL
