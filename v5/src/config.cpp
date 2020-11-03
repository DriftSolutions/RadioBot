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

#include "ircbot.h"

bool CheckConfig() {
	if (config.base.reg_name.length() == 0 && config.base.reg_key.length() != 0) {
		ib_printf(_("ERROR: Either RegName and RegKey have to both be set, or neither of them can be set!\n"));
		return false;
	}
	if (config.base.reg_name.length() != 0 && config.base.reg_key.length() == 0) {
		ib_printf(_("ERROR: Either RegName and RegKey have to both be set, or neither of them can be set!\n"));
		return false;
	}

#if defined(IRCBOT_ENABLE_IRC)
	if (config.base.irc_nick.length() == 0) {
		ib_printf(_("%s: Configuration Error -> no Nickname found!\n"), IRCBOT_NAME);
		ib_printf(_("%s: Make sure there is a Base section in your config.\n"), IRCBOT_NAME);
		return false;
	}
	/*
	if (config.num_irc == 0) {
		ib_printf(_("%s: Configuration Error -> no IRC Servers found!\n"), IRCBOT_NAME);
		ib_printf(_("%s: Make sure there is an IRC section in your config.\n"), IRCBOT_NAME);
		return false;
	}
	*/
#endif

#if defined(IRCBOT_ENABLE_SS)
	if (config.num_ss == 0) {
		ib_printf(_("%s: Configuration Error -> no SHOUTCast/Icecast2 Servers found!\n"), IRCBOT_NAME);
		ib_printf(_("%s: Make sure there is a SS section in your config.\n"), IRCBOT_NAME);
		return false;
	}
#endif
	return true;
}

bool SyncConfigNoRehash() {
	char buf[1024];
	DS_CONFIG_SECTION * sec = config.config->GetSection(NULL, "Base");
	DS_VALUE * val;
	if (sec) {
#if defined(IRCBOT_ENABLE_IRC)
		val = config.config->GetSectionValue(sec, "Nick");
		if (val && val->type == DS_TYPE_STRING) {
			config.base.irc_nick = val->pString;
		}
		val = config.config->GetSectionValue(sec, "LogChan");
		if (val && val->type == DS_TYPE_STRING) {
			config.base.log_chan = val->pString;
		}
		val = config.config->GetSectionValue(sec, "LogChanKey");
		if (val && val->type == DS_TYPE_STRING) {
			config.base.log_chan_key = val->pString;
		}
#endif
		val = config.config->GetSectionValue(sec, "LogFile");
		if (val && val->type == DS_TYPE_STRING) {
			config.base.log_file = val->pString;
		}
		val = config.config->GetSectionValue(sec, "PIDFile");
		if (val && val->type == DS_TYPE_STRING) {
			config.base.pid_file = val->pString;
		}
		val = config.config->GetSectionValue(sec, "LangCode");
		if (val && val->type == DS_TYPE_STRING) {
			config.base.langCode = val->pString;
		}
		val = config.config->GetSectionValue(sec, "RegName");
		if (val && val->type == DS_TYPE_STRING) {
			config.base.reg_name = val->pString;
		}
		val = config.config->GetSectionValue(sec, "RegKey");
		if (val && val->type == DS_TYPE_STRING) {
			config.base.reg_key = val->pString;
		}
		val = config.config->GetSectionValue(sec, "TLS_Cert");
		if (val && val->type == DS_TYPE_STRING) {
			config.base.ssl_cert = val->pString;
		} else {
			val = config.config->GetSectionValue(sec, "SSL_Cert");
			if (val && val->type == DS_TYPE_STRING) {
				config.base.ssl_cert = val->pString;
			}
		}
		val = config.config->GetSectionValue(sec, "BindIP");
		if (val && val->type == DS_TYPE_STRING) {
			config.base.bind_ip = val->pString;
		}
		val = config.config->GetSectionValue(sec, "CommandPrefixes");
		if (val && val->type == DS_TYPE_STRING) {
			config.base.command_prefixes = val->pString;
		}
		/*
		val = config.config->GetSectionValue(sec, "DynamicConfigIsPrimary");
		if (val && val->type == DS_TYPE_STRING) {
			strcpy(config.base.dynconf_fn,val->pString);
		}
		*/
#if defined(IRCBOT_ENABLE_IRC)
		val = config.config->GetSectionValue(sec, "DoSpam");
		if (val && val->type == DS_TYPE_LONG) {
			config.dospam = val->lLong ? true:false;
		}
		val = config.config->GetSectionValue(sec, "DoTopic");
		if (val && val->type == DS_TYPE_LONG) {
			config.dotopic = val->lLong ? true:false;
		}
		val = config.config->GetSectionValue(sec, "DoOnJoin");
		if (val && val->type == DS_TYPE_LONG) {
			config.doonjoin = val->lLong ? true:false;
		}
#endif
		val = config.config->GetSectionValue(sec, "AutoRegOnHello");
		if (val && val->type == DS_TYPE_LONG) {
			config.base.AutoRegOnHello = val->lLong ? true:false;
		}
		val = config.config->GetSectionValue(sec, "OnlineBackup");
		if (val && val->type == DS_TYPE_LONG) {
			config.base.OnlineBackup = val->lLong ? true:false;
			if (config.base.OnlineBackup && config.base.reg_key.length() == 0) {
				ib_printf(_("%s: Online Backup cannot be enabled without a RegName and RegKey set! Disabling...\n"), IRCBOT_NAME);
				config.base.OnlineBackup = false;
			}
		}
		val = config.config->GetSectionValue(sec, "Fork");
		if (val && val->type == DS_TYPE_LONG) {
			config.base.Fork = val->lLong ? true:false;
		}
		val = config.config->GetSectionValue(sec, "SendQ");
		if (val && val->type == DS_TYPE_LONG && val->lLong >= 0) {
			config.base.sendq = val->lLong;
		}
		val = config.config->GetSectionValue(sec, "BackupDays");
		if (val && val->type == DS_TYPE_LONG && val->lLong > 0) {
			config.base.backup_days = val->lLong;
		}
		val = config.config->GetSectionValue(sec, "RemotePort");
		if (val && val->type == DS_TYPE_LONG) {
			if (val->lLong >= 0 && val->lLong < 65536) {
				config.remote.port = val->lLong;
			}
		}
		val = config.config->GetSectionValue(sec, "RemoteUploads");
		if (val && val->type == DS_TYPE_LONG) {
			config.remote.uploads_enabled = val->lLong ? true:false;
		}
		val = config.config->GetSectionValue(sec, "RemoteUploadPath");
		if (val && val->type == DS_TYPE_STRING) {
			sstrcpy(config.remote.upload_path, val->pString);
			if (config.remote.upload_path[strlen(config.remote.upload_path)-1] == '/' || config.remote.upload_path[strlen(config.remote.upload_path)-1] == '\\') {
				config.remote.upload_path[strlen(config.remote.upload_path)-1] = 0;
			}
		}
		val = config.config->GetSectionValue(sec, "DefaultFlags");
		if (val && val->type == DS_TYPE_STRING) {
			config.base.default_flags = string_to_uflag(val->pString, config.base.default_flags);
		}

#if defined(IRCBOT_ENABLE_SS)
		val = config.config->GetSectionValue(sec, "MultiSoundServer");
		if (val && val->type == DS_TYPE_LONG) {
			config.base.multiserver = val->lLong;
		}
		val = config.config->GetSectionValue(sec, "EnableRating");
		if (val && val->type == DS_TYPE_LONG) {
			config.base.EnableRatings = val->lLong ? true:false;
		}
		val = config.config->GetSectionValue(sec, "MaxRating");
		if (val && val->type == DS_TYPE_LONG) {
			if (val->lLong >= 5) {
				config.base.MaxRating = val->lLong;
			} else {
				ib_printf(_("Warning: minimum MaxRating is 5...\n"));
			}
		}
		val = config.config->GetSectionValue(sec, "SecsBetweenUpdates");
		if (val && val->type == DS_TYPE_LONG) {
			if (val->lLong >= 15) {
				config.base.updinterval = val->lLong;
			} else {
				ib_printf(_("Warning: minimum SecsBetweenUpdates is 15...\n"));
				config.base.updinterval = 15;
			}
		}
		val = config.config->GetSectionValue(sec, "EnableRequestSystem");
		if (val && val->type == DS_TYPE_LONG) {
			config.base.enable_requests = (val->lLong == 0) ? false:true;
		}
		val = config.config->GetSectionValue(sec, "EnableFind");
		if (val && val->type == DS_TYPE_LONG) {
			config.base.enable_find = (val->lLong == 0) ? false:true;
		}
		val = config.config->GetSectionValue(sec, "MaxFindResults");
		if (val && val->type == DS_TYPE_LONG && val->lLong > 0) {
			config.base.max_find_results = val->lLong;
		}
		val = config.config->GetSectionValue(sec, "ExpireFindResults");
		if (val && val->type == DS_TYPE_LONG && val->lLong > 0) {
			config.base.expire_find_results = val->lLong;
		}
		val = config.config->GetSectionValue(sec, "AllowPMRequests");
		if (val && val->type == DS_TYPE_LONG) {
			config.base.allow_pm_requests = val->lLong ? true:false;
		}
		val = config.config->GetSectionValue(sec, "PullNameFromAnyServer");
		if (val && val->type == DS_TYPE_LONG) {
			config.base.anyserver = val->lLong ? true:false;
		}
		val = config.config->GetSectionValue(sec, "DJName");
		if (val) {
			if (!stricmp(val->pString, "Nick")) {
				config.base.dj_name_mode = DJNAME_MODE_NICK;
			} else if (!stricmp(val->pString, "User") || !stricmp(val->pString, "Username")) {
				config.base.dj_name_mode = DJNAME_MODE_USERNAME;
			} else {
				config.base.dj_name_mode = DJNAME_MODE_STANDARD;
			}
		}
#if defined(IRCBOT_ENABLE_IRC)
		val = config.config->GetSectionValue(sec, "RequestFallback");
		if (val && val->type == DS_TYPE_STRING) {
			config.base.req_fallback = val->pString;
		}
		val = config.config->GetSectionValue(sec, "ReqModesOnLogin");
		if (val && val->type == DS_TYPE_STRING) {
			config.base.req_modes_on_login = val->pString;
		}
#endif
#endif
	}
	if (config.remote.uploads_enabled && config.remote.upload_path[0] == 0) {
		ib_printf(_("%s: WARNING: Remote uploads enabled but no RemoteUploadPath set! Disabling...\n"), IRCBOT_NAME);
		config.remote.uploads_enabled = false;
	}

	sec = config.config->GetSection(NULL, "Plugin");
	if (sec) {
		int i=0;
		for (i=0; i < MAX_PLUGINS; i++) {
			sprintf(buf,"Module%d",i);
			val = config.config->GetSectionValue(sec, buf);
			if (val && val->type == DS_TYPE_STRING) {
				sstrcpy(buf, val->pString);
				str_replaceA(buf,sizeof(buf),"%pathsep%",PATH_SEPS);
				str_replaceA(buf,sizeof(buf),"%soext%",DL_SOEXT);
				config.plugins[i].fn = buf;
			}
		}
	}

#if defined(IRCBOT_ENABLE_SS)
	sec = config.config->GetSection(NULL, "SS");
	if (sec) {
		int i=0;
		for (i=0; i < MAX_SOUND_SERVERS; i++) {
			sprintf(buf,"Server%d",i);
			DS_CONFIG_SECTION * srv = config.config->GetSection(sec,buf);
			if (srv) {
				//memset(&config.s_servers[i], 0, sizeof(SOUND_SERVERS));
				config.s_servers[i].type = SS_TYPE_SHOUTCAST;
				config.s_servers[i].streamid = 1;

				val = config.config->GetSectionValue(srv, "Host");
				if (val && val->type == DS_TYPE_STRING) {
					config.s_servers[i].host = val->pString;
				}
				val = config.config->GetSectionValue(srv, "Mount");
				if (val && val->type == DS_TYPE_STRING) {
					config.s_servers[i].mount = val->pString;
				}
				val = config.config->GetSectionValue(srv, "User");
				if (val && val->type == DS_TYPE_STRING) {
					config.s_servers[i].user = val->pString;
				} else {
					config.s_servers[i].user = "admin";
				}
				val = config.config->GetSectionValue(srv, "Pass");
				if (val && val->type == DS_TYPE_STRING) {
					config.s_servers[i].pass = val->pString;
				}
				val = config.config->GetSectionValue(srv, "Type");
				if (val && val->type == DS_TYPE_STRING) {
					if (!stricmp(val->pString, "shoutcast2")) {
						config.s_servers[i].type = SS_TYPE_SHOUTCAST2;
					} else if (!strnicmp(val->pString, "ice", 3)) {
						config.s_servers[i].type = SS_TYPE_ICECAST;
						if (config.s_servers[i].mount.length() == 0) {
							config.s_servers[i].mount = "/";
						}
					} else if (!stricmp(val->pString, "steamcast")) {
						config.s_servers[i].type = SS_TYPE_STEAMCAST;
						if (config.s_servers[i].mount.length() == 0) {
							config.s_servers[i].mount = "/";
						}
					} else {
						config.s_servers[i].type = SS_TYPE_SHOUTCAST;
						config.s_servers[i].mount = "";
					}
				}
				val = config.config->GetSectionValue(srv, "Port");
				if (val && val->type == DS_TYPE_LONG) {
					config.s_servers[i].port = val->lLong;
				}
				val = config.config->GetSectionValue(srv, "StreamID");
				if (val && val->type == DS_TYPE_LONG) {
					config.s_servers[i].streamid = val->lLong;
				}

				if (!config.s_servers[i].port) {
					config.s_servers[i].port = 8000;
				}
				if (config.s_servers[i].host.length()) {
					config.num_ss = ((i+1) > config.num_ss) ? i+1:config.num_ss;
				}
			} else {
				break;
			}
		}
	}

#endif

	return true;
}

bool SyncConfig(Universal_Config * con) {
	//ib_printf(_("IRCBot: SyncConfig()...\n"));
	DRIFT_DIGITAL_SIGNATURE();

	if (con == NULL && !SyncConfigNoRehash()) {
		return false;
	}

	if (con == NULL) { con = config.config; }
	DS_CONFIG_SECTION *sec=NULL;
	DS_VALUE * val=NULL;

#if defined(IRCBOT_ENABLE_IRC)
	char buf[1024];
	sec = con->GetSection(NULL, "IRC");
	if (sec) {
		int i=0;
		for (i=0; i < MAX_IRC_SERVERS; i++) {
			sprintf(buf,"Server%d",i);
			DS_CONFIG_SECTION * srv = con->GetSection(sec,buf);
			if (srv) {
				//memset(&config.ircnets[i], 0, sizeof(IRCNETS));
				config.ircnets[i].port = 6667;
				val = con->GetSectionValue(srv, "Host");
				if (val && val->type == DS_TYPE_STRING) {
					config.ircnets[i].host = val->pString;
					if (config.ircnets[i].host.length()) {
						config.num_irc = i+1;
					}
				}
				val = con->GetSectionValue(srv, "Nick");
				if (val && val->type == DS_TYPE_STRING) {
					config.ircnets[i].base_nick = val->pString;
				} else {
					config.ircnets[i].base_nick = config.base.irc_nick;
				}
				val = con->GetSectionValue(srv, "BindIP");
				if (val && val->type == DS_TYPE_STRING) {
					config.ircnets[i].bindip = val->pString;
				} else {
					config.ircnets[i].bindip = "";
				}
				val = con->GetSectionValue(srv, "OnConnect");
				if (val && val->type == DS_TYPE_STRING) {
					config.ircnets[i].onconnect = val->pString;
				} else {
					config.ircnets[i].onconnect = "";
				}
				val = con->GetSectionValue(srv, "OnNickInUse");
				if (val && val->type == DS_TYPE_STRING) {
					config.ircnets[i].onnickinusecommand = val->pString;
				} else {
					config.ircnets[i].onnickinusecommand = "";
				}
				val = con->GetSectionValue(srv, "RemovePreText");
				if (val && val->type == DS_TYPE_STRING) {
					config.ircnets[i].removePreText = val->pString;
				} else {
					config.ircnets[i].removePreText = "";
				}
				val = con->GetSectionValue(srv, "Pass");
				if (val && val->type == DS_TYPE_STRING) {
					config.ircnets[i].pass = val->pString;
				}
				val = con->GetSectionValue(srv, "On512");
				if (val && val->type == DS_TYPE_STRING) {
					config.ircnets[i].on512 = val->pString;
				}
				val = con->GetSectionValue(srv, "CustomIdent");
				if (val && val->type == DS_TYPE_STRING) {
					config.ircnets[i].custIdent = val->pString;
				}
				val = con->GetSectionValue(srv, "Port");
				if (val && val->type == DS_TYPE_LONG) {
					config.ircnets[i].port = val->lLong;
				}
				val = con->GetSectionValue(srv, "TLS");
				if (val && val->type == DS_TYPE_LONG) {
					config.ircnets[i].ssl = clamp<int32>(val->lLong, 0, 2);
				} else {
					val = con->GetSectionValue(srv, "SSL");
					if (val && val->type == DS_TYPE_LONG) {
						config.ircnets[i].ssl = clamp<int32>(val->lLong, 0, 2);
					}
				}
				val = con->GetSectionValue(srv, "Log");
				if (val && val->type == DS_TYPE_LONG) {
					config.ircnets[i].log = val->lLong ? true:false;
				}
				val = con->GetSectionValue(srv, "shortIdent");
				if (val && val->type == DS_TYPE_LONG) {
					config.ircnets[i].shortIdent = val->lLong ? true:false;
				}
				val = con->GetSectionValue(srv, "CAP");
				if (val && val->type == DS_TYPE_LONG) {
					config.ircnets[i].enable_cap = val->lLong ? true : false;
				}
				val = con->GetSectionValue(srv, "regainNick");
				if (val && val->type == DS_TYPE_LONG) {
					config.ircnets[i].regainNick = val->lLong ? true:false;
				}

				for (int j=0; j < MAX_IRC_CHANNELS; j++) {
					sprintf(buf,"Channel%d",j);
					config.ircnets[i].channels[j].Reset(false);
					DS_CONFIG_SECTION * chan = con->GetSection(srv,buf);
					if (chan) {
						val = con->GetSectionValue(chan, "Channel");
						if (val && val->type == DS_TYPE_STRING) {
							config.ircnets[i].channels[j].channel = val->pString;
						}
						val = con->GetSectionValue(chan, "Key");
						if (val && val->type == DS_TYPE_STRING) {
							config.ircnets[i].channels[j].key = val->pString;
						}
						val = con->GetSectionValue(chan, "AltTopicCommand");
						if (val && val->type == DS_TYPE_STRING) {
							config.ircnets[i].channels[j].altTopicCommand = val->pString;
						}
						val = con->GetSectionValue(chan, "AltJoinCommand");
						if (val && val->type == DS_TYPE_STRING) {
							config.ircnets[i].channels[j].altJoinCommand = val->pString;
						}
						val = con->GetSectionValue(chan, "DoSpam");
						if (val && val->type == DS_TYPE_LONG) {
							config.ircnets[i].channels[j].dospam = val->lLong ? true:false;
						}
						val = con->GetSectionValue(chan, "DoOnJoin");
						if (val && val->type == DS_TYPE_LONG) {
							config.ircnets[i].channels[j].doonjoin = val->lLong ? true:false;
						}
						val = con->GetSectionValue(chan, "DoTopic");
						if (val && val->type == DS_TYPE_LONG) {
							config.ircnets[i].channels[j].dotopic = val->lLong ? true:false;
						}
						val = con->GetSectionValue(chan, "noTopicCheck");
						if (val && val->type == DS_TYPE_LONG) {
							config.ircnets[i].channels[j].noTopicCheck = val->lLong ? true:false;
						}
						val = con->GetSectionValue(chan, "autoVoice");
						if (val && val->type == DS_TYPE_LONG) {
							config.ircnets[i].channels[j].autoVoice = val->lLong ? true:false;
						}
						val = con->GetSectionValue(chan, "moderateOnUpdate");
						if (val && val->type == DS_TYPE_LONG) {
							config.ircnets[i].channels[j].moderateOnUpdate = val->lLong ? true:false;
						}
						val = con->GetSectionValue(chan, "SongInterval");
						if (val && val->type == DS_TYPE_LONG) {
							config.ircnets[i].channels[j].songinterval = val->lLong;
						}
						config.ircnets[i].channels[j].songintervalsource = -1;
						val = con->GetSectionValue(chan, "SongIntervalSource");
						if (val && val->type == DS_TYPE_LONG) {
							config.ircnets[i].channels[j].songintervalsource = val->lLong;
						}

						if (config.ircnets[i].channels[config.ircnets[i].num_chans].channel.length()) {
							config.ircnets[i].num_chans = ((j+1) > config.ircnets[i].num_chans) ? j+1:config.ircnets[i].num_chans;
						}
					} else {
						break;
					}
				}
			} else {
				break;
			}
		}
	}

	sec = con->GetSection(NULL, "Timer");
	if (sec) {
		int i=0;
		for (i=0; i < MAX_TIMERS; i++) {
			sprintf(buf,"Timer%d",i);
			DS_CONFIG_SECTION * srv = con->GetSection(sec,buf);
			if (srv) {
				val = con->GetSectionValue(srv, "Interval");
				if (val->type == DS_TYPE_STRING) {
					get_range(val->pString, &config.timers[i].delaymin, &config.timers[i].delaymax);
					config.timers[i].nextFire = genrand_range(config.timers[i].delaymin, config.timers[i].delaymax);
				} else {
					config.timers[i].delaymin = config.timers[i].delaymax = config.timers[i].nextFire = val->lLong;
				}
				val = con->GetSectionValue(srv, "Action");
				if (val && val->type == DS_TYPE_STRING) {
					if (!strnicmp(val->pString, "random:", 7)) {
						FILE * fp = fopen(val->pString+7, "rb");
						if (fp != NULL) {
							while (!feof(fp) && fgets(buf, sizeof(buf), fp)) {
								strtrim(buf);
								if (buf[0] == 0 || buf[0] == '#' || buf[0] == '/') {
									continue;
								}
								config.timers[i].lines.push_back(buf);
							}
							ib_printf("Timer%d: Read %u lines from %s.\n", i, config.timers[i].lines.size(), val->pString+7);
							fclose(fp);
						} else {
							ib_printf("Timer%d: Error opening %s for reading!\n", i, val->pString+7);
						}
					} else {
						config.timers[i].action = val->pString;
					}
				}
				val = con->GetSectionValue(srv, "Network");
				config.timers[i].network = val->lLong;
				for (int j=0; j < 4; j++) {
					char buf2[32],buf3[32];
					sprintf(buf,"Parm%d",j);
					sprintf(buf3,"%%parm%d%%",j);
					val = con->GetSectionValue(srv, buf);
					if (val) {
						char * p = NULL;
						if (val->type == DS_TYPE_STRING) {
							p = val->pString;
						} else if (val->type == DS_TYPE_LONG) {
							sprintf(buf2, "%d", val->lLong);
							p = buf2;
						} else if (val->type == DS_TYPE_FLOAT) {
							sprintf(buf2, "%.02f", val->lFloat);
							p = buf2;
						}
						if (config.timers[i].lines.size() > 0) {
							for (size_t k=0; k < config.timers[i].lines.size(); k++) {
								strlcpy(buf,config.timers[i].lines[k].c_str(),sizeof(buf));
								str_replace(buf, sizeof(buf), buf3, p);
								config.timers[i].lines[k] = buf;
							}
						} else {
							strlcpy(buf,config.timers[i].action.c_str(),sizeof(buf));
							str_replace(buf, sizeof(buf), buf3, p);
							config.timers[i].action = buf;
						}
					}
				}
			}
		}
	}
#endif // defined(IRCBOT_ENABLE_IRC)

	//DRIFT_DIGITAL_SIGNATURE();
	return true;
}

bool LoadText_Stage2(FILE * fConf, const char * fn) {
	int i=0;
	char curline[8192],name[128];

#if defined(IRCBOT_ENABLE_IRC)
	COMMAND_ACL acl;
#endif

	memset(curline, 0, sizeof(curline));
	for (i=0; fgets(curline, sizeof(curline), fConf) != NULL; i++) {
		strtrim(curline,"\r\n\t ");

		if (!strnicmp(curline, "#include", strlen("#include"))) {
			char * p = ((char *)curline) + strlen("#include");
			if (!strnicmp(p, " \"",2)) {
				p += 2;
				char * q = strchr(p, '\"');
				if (q) {
					q[0]=0;
					if (strlen(p)) {
						FILE * fp = fopen(p, "rb");
						if (fp) {
							LoadText_Stage2(fp,(const char *)p);
							fclose(fp);
						} else {
							ib_printf(_("ERROR: Error opening %s\n"), p);
							return false;
						}
					} else {
						ib_printf(_("ERROR: Usage #include \"filename\"\n"));
						return false;
					}
				} else {
					ib_printf(_("ERROR: Usage #include \"filename\"\n"));
					return false;
				}
			} else {
				ib_printf(_("ERROR: Usage #include \"filename\"\n"));
				return false;
			}
		}

		if (curline[0] == '#' || curline[0] == '/' || curline[0] == ';') {
			continue;
		}

		if (strlen(curline) >= 3) {
			char *p = strchr(curline,'=');
			if (p == NULL) { continue; }
			p[0]=0;
			memset(name,0,sizeof(name));
			strlcpy(name, curline, sizeof(name));
			strtrim(name,"\t ");

			p++;
			strtrim(p,"\t ");

			if (name[0] == '%') {
				sesMutex.Lock();
				config.customVars[name] = p;
				sesMutex.Release();
			} else if (name[0] == '!' || name[0] == '@' || name[0] == '?') {
				if (p[0] == '!') {
					char * parms = strchr(p, ' ');
					if (parms) {
						*parms=0;
						parms++;
					}
					if (stricmp(p+1, "version")) {
						RegisterCommandAlias(((char *)name)+1, p+1, parms);
					}
#if defined(IRCBOT_ENABLE_IRC)
				} else {
					if (p[1] == '/') {
						FillACL(&acl, letter_to_uflag(p[0]), 0, 0);
						p +=2;
						strtrim(p, "\t ");
					} else {
						FillACL(&acl, 0, 0, 0);
					}
					if (stricmp(((char *)name)+1, "version")) {
						uint32 type = 0;
						if (name[0] == '?' || name[0] == '@') {
							type |= CM_ALLOW_IRC_PRIVATE;
						}
						if (name[0] == '!' || name[0] == '@') {
							type |= CM_ALLOW_IRC_PUBLIC;
						}
						RegisterCommand(((char *)name)+1, &acl, type|CM_FLAG_FROM_TEXT, p, "This is a user-defined function from " IRCBOT_TEXT "");
					}
#endif
				}
			} else {
				sesMutex.Lock();
				config.messages[name] = p;
				sesMutex.Release();
#if defined(DEBUG)
				//ib_printf("Message[%s]: \"%s\" -> %u\n", name, p, strlen(p));
#endif
			}
		}
	}

	return true;
}

bool LoadText(const char * fn) {
	ib_printf(_("%s: Loading %s...\n"), IRCBOT_NAME, fn);

	FILE * fConf = fopen(fn,"rb");
	if (fConf == NULL) {
		ib_printf(_("%s: Couldn't open %s\n"), IRCBOT_NAME, fn);
		return false;
	}

	sesMutex.Lock();
	if (config.messages.size()) {
		ib_printf(_("%s: Freeing existing messages...\n"), IRCBOT_NAME);
		config.messages.clear();
	}
	sesMutex.Release();

	sesMutex.Lock();
	if (config.customVars.size()) {
		ib_printf(_("%s: Freeing existing custom variables...\n"), IRCBOT_NAME);
		config.customVars.clear();
	}
	sesMutex.Release();

	UnregisterCommandsWithMask(CM_FLAG_FROM_TEXT);

	bool ret = LoadText_Stage2(fConf, fn);

	fclose(fConf);
	return ret;
}

void LoadReperm() {
	char fn[] = "reperm.conf";
	if (access(fn, 0) == 0) {
		ib_printf(_("%s: Loading %s...\n"), IRCBOT_NAME, fn);

		FILE * fConf = fopen(fn,"rb");
		if (fConf != NULL) {
			char curline[1024];
			memset(curline, 0, sizeof(curline));
			for (int i=0; fgets(curline, sizeof(curline), fConf) != NULL; i++) {
				strtrim(curline,"\r\n\t ");
				if (curline[0] && curline[0] != '/' && curline[0] != '#') {
					StrTokenizer st(curline, ' ');
					if (st.NumTok() >= 2) {
						char * pcmd = st.GetSingleTok(1);
						char * cmd = pcmd;
						if (IsCommandPrefix(cmd[0])) {
							cmd++;
						}
						COMMAND * com = FindCommand(cmd);
						if (com) {
							char * perms = st.GetSingleTok(2);
							com->acl.flags_any = string_to_uflag(perms, com->acl.flags_any);
							st.FreeString(perms);
							if (st.NumTok() >= 3) {
								perms = st.GetSingleTok(3);
								com->acl.flags_all = string_to_uflag(perms, com->acl.flags_all);
								st.FreeString(perms);
							}
							if (st.NumTok() >= 4) {
								perms = st.GetSingleTok(4);
								com->acl.flags_not = string_to_uflag(perms, com->acl.flags_not);
								st.FreeString(perms);
							}
							char buf1[32],buf2[32],buf3[32];
							uflag_to_string(com->acl.flags_any, buf1, sizeof(buf1));
							uflag_to_string(com->acl.flags_all, buf2, sizeof(buf2));
							uflag_to_string(com->acl.flags_not, buf3, sizeof(buf3));
							ib_printf(_("%s: Command '%s' new permissions -> any: %s, all: %s, not: %s\n"), IRCBOT_NAME, cmd, buf1, buf2, buf3);
						} else {
							ib_printf(_("%s: Could not find command '%s' specified in %s on line %d\n"), IRCBOT_NAME, cmd, fn, i);
						}
						st.FreeString(pcmd);
					} else {
						ib_printf(_("%s: No command specified in %s on line %d\n"), IRCBOT_NAME, fn, i);
					}
				}
			}
			fclose(fConf);
		} else {
			ib_printf(_("%s: Couldn't open %s\n"), IRCBOT_NAME, fn);
		}
	}
}
