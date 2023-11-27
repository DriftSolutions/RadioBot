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

void find_finish(USER_PRESENCE * ut, FIND_RESULTS * qRes, int max);

//#define CM_ONLY_FLAGS(x) (x & ~0x0000000F)
char * GenCommandMaskString(COMMAND * com, char * buf, size_t bufSize) {
	int ind=0;
	//buf[ind++] = '0'+com->minlevel;
	if (com->mask & CM_ALLOW_IRC_PUBLIC) {
		buf[ind++] = 'c';
	}
	if (com->mask & CM_ALLOW_IRC_PRIVATE) {
		buf[ind++] = 'p';
	}
	if (com->mask & CM_ALLOW_CONSOLE_PRIVATE) {
		buf[ind++] = 'o';
	}
	if (com->mask & CM_ALLOW_CONSOLE_PUBLIC) {
		buf[ind++] = 'g';
	}
	if (com->mask & CM_FLAG_FROM_TEXT) {
		buf[ind++] = 't';
	}
	if (com->mask & CM_FLAG_ASYNC) {
		buf[ind++] = 'a';
	}
	buf[ind] = 0;
	return buf;
}

void BuildMemStatString(char * buf, const char * name, int64 num_pointers, int64 bytes_allocated) {
	//ut->send_msg(ut, "Module    | Pointers | Memory in Use");
	/*
#ifdef WIN32
#define SFMT "%08I64d"
#define LFMT "%010I64d"
#else
#define SFMT "%08lld"
#define LFMT "%010lld"
#endif
	*/

	buf[0] = 0;
	strcat(buf, name);
	while (strlen(buf) < 11) {
		strcat(buf, " ");
	}
	strcat(buf, " ");
	string s = number_format(num_pointers);
	int num = 8 - s.length();
	for (int i=0; i < num; i++) {
		strcat(buf, " ");
	}
	strcat(buf, s.c_str());
//	sprintf(buf + strlen(buf), SFMT, num_pointers);
	while (strlen(buf) < 22) {
		strcat(buf, " ");
	}

	strcat(buf, " ");
	s = number_format(bytes_allocated);
	strcat(buf, s.c_str());
//	sprintf(buf + strlen(buf), LFMT, bytes_allocated);
	strcat(buf, _(" bytes"));
}

bool viewusers_cb(const char * nick, void * ptr) {
	USER_PRESENCE * ut = (USER_PRESENCE *)ptr;
	ut->std_reply(ut, nick);
	return true;
}

bool is_number(const char * op) {
	if (*op == 0) { return false; }
	const char * p = op;
	while (*p) {
		if (*p == ' ') {
			if (p == op) { return false; }
			break;
		}
		if (p == op && *p == '-') { p++; continue; }
		if (!is_numeric(*p)) { return false; }
		p++;
	}
	return true;
}

int InternalCommandsHelp(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
	if (!stricmp(command,"viewuser")) {
		if (!stricmp(parms, "flags")) {
			ut->std_reply(ut, "To view a list of user flags, check this page: http://wiki.shoutirc.com/index.php/User_Flags");
			return 1;
		}
	}
	return 0;
}

int InternalCommands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
	char buf[4096], buf2[4096];

	if (!stricmp(command,"cfgdump")) {
		config.config->PrintConfigTree();
		ut->std_reply(ut, "Ok, check the bot console...");
	}

	if (!stricmp(command,"hello")) {
		if (ut->User) {
			uflag_to_string(ut->Flags, buf, sizeof(buf));
			sprintf(buf2, _("Hello %s, your user flags are: %s"), ut->Nick, buf);
		} else {
			USER * user = GetUserByNick(ut->Nick);
			if (user == NULL) {
				if (GetUserCount() == 0) {
					sprintf(buf2, "%s", ut->Nick);
					FixUsername(buf2);
					user = buf2[0] ? AddUser(buf2, "", GetLevelFlags(1)):NULL;
					if (user) {
						sprintf(buf2, "*%s*!*@*", ut->Nick);
						AddHostmask(user, buf2);
						sprintf(buf2, _("Hello %s, you are now my Master. You by default have a lax hostmask, you should add a better one for security reasons. Also don't forget to set a password with %cchpass"), ut->Nick, PrimaryCommandPrefix());
					} else {
						sstrcpy(buf2, _("Error: Could not create you a user account!"));
					}
				} else {
					if (config.base.AutoRegOnHello) {
						sprintf(buf2, "%s", ut->Nick);
						FixUsername(buf2);
						user = buf2[0] ? AddUser(buf2, "", GetLevelFlags(5)):NULL;
						if (user) {
							const char * hostend = strrchr(ut->Hostmask, '.');
							if (hostend) {
								if (stricmp(hostend,".IP")) {
									hostend = strrchr(hostend-1, '.');
								} else {
									hostend = NULL;
								}
							}
							sprintf(buf, "*%s*!*@*%s", ut->Nick, hostend ? hostend:"");
							AddHostmask(user, buf);
							sprintf(buf2, _("Hello %s, You have been registered with the hostmask %s."), ut->Nick, buf);
						} else {
							sstrcpy(buf2, _("Error: Could not create you a user account!"));
						}
					} else {
						sprintf(buf2, _("Hello %s, I don't know you"), ut->Nick);
					}
				}
			} else {
				sprintf(buf2, _("Hello %s, I don't recognize you from that host. Type !identify yourpass to have this hostmask added to your account..."), user->Nick);
			}
			if (user) { UnrefUser(user); }
		}
		ut->send_msg(ut, buf2);
		return 1;
	}

	if (!stricmp(command,"identify") && (!parms || !strlen(parms))) {
		sprintf(buf, _("Usage: %cidentify [username] password"), PrimaryCommandPrefix());
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command,"identify")) {
		if (ut->User) {
			ut->send_msg(ut, _("You are already recognized!"));
		} else {
			char * tmp = zstrdup(parms);
			const char * nick = ut->Nick;
			const char * pass = parms;
			if (strchr(tmp, ' ')) {
				nick = tmp;
				char * p = strchr(tmp, ' ');
				*p = 0;
				pass = ++p;
			}

			USER * uRec = GetUserByNick(nick);
			if (uRec == NULL) {
				sprintf(buf2, _("I don't have any users by the nickname %s!"), nick);
			} else {
				if (strlen(uRec->Pass) && !stricmp(pass,uRec->Pass)) {
					if (ut->NetNo >= 0) {
						sprintf(buf, "*%s*!*@*", ut->Nick);
					} else {
						sstrcpy(buf, ut->Hostmask);
					}
					if (AddHostmask(uRec, buf)) {
						sprintf(buf2, _("I have added the hostmask '%s' to your account, you should now be recognized"), buf);
					} else {
						sstrcpy(buf2, _("Error adding new hostmask, you may have too many already. Get an Admin to help you..."));
					}
				} else {
					sstrcpy(buf2, _("Invalid password!"));
				}
				UnrefUser(uRec);
			}
			ut->send_msg(ut, buf2);

			zfree(tmp);
		}
		return 1;
	}

	if (!stricmp(command,"whoami")) {
		sprintf(buf, _("Your current nick is: %s"), ut->Nick);
		ut->send_msg(ut, buf);

		USER * user = ut->User;
		if (user) {
			uflag_to_string(ut->Flags, buf2, sizeof(buf2));
			sprintf(buf, _("Your %s username is: %s, user flags: %s"), IRCBOT_NAME, user->Nick, buf2);
		} else {
			sstrcpy(buf, _("You are not registered or not recognized by your current hostmask"));
		}
		ut->send_msg(ut, buf);
		/*
		if (type & CM_ALLOW_IRC_PRIVATE) {
			if (ut->netno >= 0) {
				sprintf(buf, "You are PMing me on IRC network %d", ut->netno);
			} else {
				sprintf(buf, "You are PMing me on an unknown IRC network");
			}
		} else {
			sprintf(buf, "You are typing from a Console interface");
		}
		*/
		sprintf(buf, _("You are messaging me from %s"), ut->Desc);
		ut->send_msg(ut, buf);
		sprintf(buf, _("Your hostmask is: %s"), ut->Hostmask);
		ut->send_msg(ut, buf);
		return 1;
	}

	if (!stricmp(command,"modules")) {
		sprintf(buf,_("Number of loaded modules: %d"),NumPlugins());
		ut->send_msg(ut, buf);
		for (int i = 0; i < MAX_PLUGINS; i++) {
			if (config.plugins[i].plug != NULL) {
				sprintf(buf,_("[%d] %s [%s]"), i, config.plugins[i].plug->plugin_name_long, config.plugins[i].name.c_str());
				ut->send_msg(ut, buf);
			} else if (config.plugins[i].fn.length()) {
				sprintf(buf,_("[%d] %s [plugin not loaded]"), i, nopathA(config.plugins[i].fn.c_str()));
				ut->send_msg(ut, buf);
			}
		}
		utprintf(ut, ut->send_msg, _("End of list. Use %cmodinfo num to get more information on a specific plugin."), PrimaryCommandPrefix());
		return 1;
	}

	if (!stricmp(command,"modinfo") && (!parms || !strlen(parms))) {
		snprintf(buf, sizeof(buf), _("Usage: %cmodinfo num (Get num from the command modules)"), PrimaryCommandPrefix());
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command,"modinfo")) {
		int num = atoi(parms);
		if (config.plugins[num].plug) {
			PLUGIN_PUBLIC *p = config.plugins[num].plug;
			sprintf(buf,_("Information for %s ..."),nopathA((char *)config.plugins[num].fn.c_str()));
			ut->send_msg(ut, buf);
			sprintf(buf,_("Long Name: %s"), p->plugin_name_long);
			ut->send_msg(ut, buf);
			sprintf(buf,_("Short Name: %s"), p->plugin_name_short);
			ut->send_msg(ut, buf);
			sprintf(buf,_("Author: %s"), p->plugin_author);
			ut->send_msg(ut, buf);
			sprintf(buf,_("GUID: %s"), p->guid);
			ut->send_msg(ut, buf);
			sprintf(buf,_("Binary Interface (ABI): %02X"), p->plugin_ver);
			ut->send_msg(ut, buf);
		} else {
			ut->send_msg(ut, _("That plugin is not loaded!"));
		}
		//bSend(config.ircnets[netno].sock,buf,strlen(buf));
		//UnloadPlugin(num);
		//sprintf(buf,"Done.",ut->Nick);
		//ut->send_msg(ut, buf);
		return 1;
	}

	if (!stricmp(command,"modunload") && (!parms || !strlen(parms))) {
		snprintf(buf, sizeof(buf), _("Usage: %cmodunload num (Get num from the command modules)"), PrimaryCommandPrefix());
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command,"modunload")) {
		int num = atoi(parms);
		if (config.plugins[num].plug) {
			sprintf(buf,_("Unloading module %d..."),num);
			ut->send_msg(ut, buf);
			config.unload_plugin = num;
		} else {
			ut->send_msg(ut, _("That plugin is not loaded!"));
		}
		//bSend(config.ircnets[netno].sock,buf,strlen(buf));
		//UnloadPlugin(num);
		//sprintf(buf,"Done.",ut->Nick);
		//ut->send_msg(ut, buf);
		return 1;
	}

	if (!stricmp(command,"modload") && (!parms || !stricmp(parms,""))) {
		snprintf(buf, sizeof(buf), _("Usage: %cmodload num (Get num from the command modules)"), PrimaryCommandPrefix());
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command,"modload")) {
		int num = atoi(parms);
		if (!config.plugins[num].plug) {
			sprintf(buf,_("Loading module %d..."),num);
			ut->send_msg(ut, buf);
			if (LoadPlugin(num)) {
				ut->send_msg(ut, _("Successfully loaded plugin!"));
				LoadReperm();
			} else {
				ut->send_msg(ut, _("Error loading plugin!"));
			}
		} else {
			ut->send_msg(ut, _("That plugin is already loaded!"));
		}
		return 1;
	}

#if defined(IRCBOT_ENABLE_IRC)
	if (!stricmp(command, "sendq-stats")) {
		SENDQ_STATS * s = SendQ_GetStats();
		sprintf(buf, "[Bytes Sent (IRC): " I64FMT "] [Bytes Sent (Other): " I64FMT "] [Avg. BPS: %u] [Currently Queued Items: %u]", s->bytesSentIRC, s->bytesSent, s->avgBps, s->queued_items);
		ut->send_msg(ut, buf);
		return 1;
	}

	if (!stricmp(command, "sendq-set")) {
		if (parms && strlen(parms)) {
			int tmp = atoi(parms);
			if (tmp > 0) {
				config.base.sendq = tmp;
				sprintf(buf, _("SendQ Max BPS is now set to: %d"), config.base.sendq);
			} else {
				sstrcpy(buf, _("Error: SendQ must be greater than 0"));
			}
		} else {
			sprintf(buf, _("SendQ Max BPS is currently set to: %d"), config.base.sendq);
		}
		ut->send_msg(ut, buf);
		return 1;
	}

	if (!stricmp(command,"bcast") && (!parms || !stricmp(parms,""))) {
		snprintf(buf, sizeof(buf), _("Usage: %cbcast text"), PrimaryCommandPrefix());
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command,"bcast")) {
		if (config.dospam) {
			sstrcpy(buf, parms);
			ProcText(buf,sizeof(buf));
			BroadcastMsg(ut, buf);
			sstrcpy(buf,_("Broadcast Sent!"));
		} else {
			sstrcpy(buf,_("Spamming is currently off!"));
		}
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command,"dospam")) {
		if (!parms || !strlen(parms)) {
			if (config.dospam) {
				config.dospam=false;
			} else {
				config.dospam=true;
			}
			sprintf(buf,_("Global Spamming is now: %s"),config.dospam ? _("On"):_("Off"));
		} else {
			StrTokenizer st((char *)parms, ' ');
			int net = ut->NetNo;
			std::string chan = "";
			if (type != CM_ALLOW_IRC_PRIVATE) { net = 0; }
			if (st.NumTok() == 1) {
				chan = st.stdGetSingleTok(1);
			} else if (st.NumTok() == 2) {
				net = atoi(st.stdGetSingleTok(1).c_str());
				chan = st.stdGetSingleTok(2);
			}
			if (net >= 0 && net < config.num_irc && chan.length()) {
				int nchan = 0;
				bool found = false;
				for (nchan=0; nchan<config.ircnets[net].num_chans; nchan++) {
					if (!stricmp(chan.c_str(), config.ircnets[net].channels[nchan].channel.c_str())) {
						found = true;
						break;
					}
				}
				if (found) {
					config.ircnets[net].channels[nchan].dospam = config.ircnets[net].channels[nchan].dospam ? false:true;
					sprintf(buf, _("Channel Spamming in channel %s on IRC Network %d is now: %s"),config.ircnets[net].channels[nchan].channel.c_str(), net, config.ircnets[net].channels[nchan].dospam ? "On":"Off");
				} else {
					sprintf(buf, _("Channel %s was not found in my records!"), chan.c_str());
				}
			} else {
				sprintf(buf, _("Usage: %cdospam [netno] [chan]"), PrimaryCommandPrefix());
			}
		}
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command,"dotopic")) {
		if (!parms || !strlen(parms)) {
			if (config.dotopic) {
				config.dotopic=false;
			} else {
				config.dotopic=true;
			}
			sprintf(buf,_("Global topic management is now: %s"),config.dotopic ? _("On"):_("Off"));
		} else {
			StrTokenizer st((char *)parms, ' ');
			int net = ut->NetNo;
			std::string chan = "";
			if (type != CM_ALLOW_IRC_PRIVATE) { net = 0; }
			if (st.NumTok() == 1) {
				chan = st.stdGetSingleTok(1);
			} else if (st.NumTok() == 2) {
				net = atoi(st.stdGetSingleTok(1).c_str());
				chan = st.stdGetSingleTok(2);
			}
			if (net >= 0 && net < config.num_irc && chan.length()) {
				int nchan = 0;
				bool found = false;
				for (nchan=0; nchan<config.ircnets[net].num_chans; nchan++) {
					if (!stricmp(chan.c_str(), config.ircnets[net].channels[nchan].channel.c_str())) {
						found = true;
						break;
					}
				}
				if (found) {
					config.ircnets[net].channels[nchan].dotopic = config.ircnets[net].channels[nchan].dotopic ? false:true;
					sprintf(buf, _("Topic management in channel %s on IRC Network %d is now: %s"),config.ircnets[net].channels[nchan].channel.c_str(), net, config.ircnets[net].channels[nchan].dotopic ? "On":"Off");
				} else {
					sprintf(buf, _("Channel %s was not found in my records!"), chan.c_str());
				}
			} else {
				sprintf(buf, _("Usage: %cdotopic [netno] [chan]"), PrimaryCommandPrefix());
			}
		}
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command,"doonjoin")) {
		if (config.doonjoin) {
			config.doonjoin=false;
		} else {
			config.doonjoin=true;
		}
		sprintf(buf,_("OnJoins are now: %s"),config.doonjoin ? _("On"):_("Off"));
		ut->std_reply(ut, buf);
		return 1;
	}
#endif // defined(IRCBOT_ENABLE_IRC)

	if (!stricmp(command,"die")) {
		ut->std_reply(ut, _("Shutting down..."));
#if defined(IRCBOT_ENABLE_IRC)
		char * tmp = zmprintf(_("%cdie ordered by %s"),PrimaryCommandPrefix(),ut->Nick);
		config.base.quitmsg = tmp;
		zfree(tmp);
#endif
		config.shutdown_now=true;
		return 1;
	}

	if (!stricmp(command,"restart")) {
		ut->std_reply(ut, _("Restarting..."));
#if defined(IRCBOT_ENABLE_IRC)
		char * tmp = zmprintf(_("%crestart ordered by %s"),PrimaryCommandPrefix(),ut->Nick);
		config.base.quitmsg = tmp;
		zfree(tmp);
#endif
		config.shutdown_reboot=true;
		config.shutdown_now=true;
		return 1;
	}

	if (!stricmp(command, "rehash")) {
		snprintf(buf, sizeof(buf), _("Reloading %s ..."), IRCBOT_TEXT);
		ut->std_reply(ut, buf);
		if (parms && parms[0]) {
			config.base.fnText = parms;
		}
		config.rehash = true;
		return 1;
	}

	if (!stricmp(command, "reload")) {
		LockMutex(sesMutex);
		ut->std_reply(ut, "Hold on to your butts...");
		Universal_Config * con = new Universal_Config();
		if (con->LoadConfig((char *)config.base.fnConf.c_str())) {
#if defined(IRCBOT_ENABLE_IRC)
			for (int i=0; i < config.num_irc; i++) {
				if (config.ircnets[i].ready) {
					for (int j=0; j < config.ircnets[i].num_chans; j++) {
						sprintf(buf, "PART %s :Reloading config...\r\n", config.ircnets[i].channels[j].channel.c_str());
						bSend(config.ircnets[i].sock, buf, strlen(buf), PRIORITY_IMMEDIATE);
					}
				}
			}
#endif
			ut->std_reply(ut, "Config loaded in to memory, processing...");
			SyncConfig(con);
			ut->std_reply(ut, "Reload complete.");
		} else {
			ut->std_reply(ut, "Error reading configuration! (probably a syntax error)");
		}
		delete con;
		RelMutex(sesMutex);
		return 1;
	}

	if (!stricmp(command,"adduser") && (!parms || !stricmp(parms,""))) {
		snprintf(buf, sizeof(buf), _("Usage: %cadduser nick +flags pass [hostmask]"), PrimaryCommandPrefix());
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command,"adduser")) {
		char * cparms = zstrdup(parms);
		char * p2 = NULL;
		char *nick = strtok_r(cparms," ",&p2);
		if (nick == NULL) {
			snprintf(buf, sizeof(buf), _("Usage: %cadduser nick +flags pass [hostmask]"), PrimaryCommandPrefix());
			ut->std_reply(ut, buf);
			zfree(cparms);
			return 1;
		}

		char *clev = strtok_r(NULL," ",&p2);
		if (clev == NULL) {
			snprintf(buf, sizeof(buf), _("Usage: %cadduser nick +flags pass [hostmask]"), PrimaryCommandPrefix());
			ut->std_reply(ut, buf);
			zfree(cparms);
			return 1;
		}

		char * flags = clev;
		uint32 nflags = string_to_uflag(clev);
		if (nflags == 0) {
			snprintf(buf, sizeof(buf), _("Usage: %cadduser nick +flags pass [hostmask]"), PrimaryCommandPrefix());
			ut->std_reply(ut, buf);
			zfree(cparms);
			return 1;
		}

		char *pass = strtok_r(NULL," ",&p2);
		if (pass == NULL) {
			snprintf(buf, sizeof(buf), _("Usage: %cadduser nick +flags pass [hostmask]"), PrimaryCommandPrefix());
			ut->std_reply(ut, buf);
			zfree(cparms);
			return 1;
		}

		char * hm = strtok_r(NULL, " ", &p2);
		if (!hm) {
			hm = buf2;
			sprintf(buf2, "*%s*!*@*", nick);
		}

		if (!IsUsernameLegal(nick)) {
			sprintf(buf, _("Error: %s usernames can only start with a letter and can only contain the following characters: %s"), IRCBOT_NAME, valid_nick_chars);
			ut->std_reply(ut, buf);
#if defined(IRCBOT_ENABLE_IRC)
			sprintf(buf, _("Note: The %s username does not have to match the user's IRC nickname since it uses hostmask matching."), IRCBOT_NAME);
			ut->std_reply(ut, buf);
			ut->std_reply(ut, _("For example, if a person has a nick of Soul|Taker, you might use SoulTaker"));
#endif
			zfree(cparms);
			return 1;
		}

		ib_printf(_("AddUser: %s/%s/%s/%s\n"),nick,flags,pass,hm);

		USER * Scan = GetUserByNick(nick);
		if (Scan) {
			ut->std_reply(ut, _("That user already exists! Use !chpass/!chflags/+host/-host/etc. to alter a user."));
			UnrefUser(Scan);
		} else {
			IBM_USERFULL ui;
			ui.flags = nflags;
			sstrcpy(ui.nick,nick);
			sstrcpy(ui.pass,pass);
			sstrcpy(ui.hostmask, hm);
			int ret = SendMessage(-1,IB_ADD_USER,(char *)&ui,sizeof(ui));
			if (ret > 0) {
				if (ret == 2) {
					ut->std_reply(ut, _("User addition completed successfully!"));
				} else {
					ut->std_reply(ut, _("User addition FAILED! (The bot console may have more information)"));
				}
			} else {
				Scan = AddUser(nick, pass, nflags);
				if (Scan) {
					AddHostmask(Scan, hm);
					UnrefUser(Scan);
					ut->std_reply(ut, _("User addition completed successfully!"));
				} else {
					ut->std_reply(ut, _("User addition FAILED! (The bot console may have more information)"));
				}
			}
		}
		zfree(cparms);
		return 1;
	}

	if (!stricmp(command,"deluser") && (!parms || !stricmp(parms,""))) {
		snprintf(buf, sizeof(buf), _("Usage: %cdeluser nick"), PrimaryCommandPrefix());
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command,"deluser")) {
		USER * Scan = GetUserByNick(parms);
		if (Scan) {
			if (uflag_compare(ut->Flags, Scan->Flags) < 0) {
				ut->std_reply(ut, _("Sorry, you are not high enough level to delete that user!"));
				UnrefUser(Scan);
			} else {
				DelUser(Scan);//DelUser will unref() for us
				ut->std_reply(ut, _("User deleted!"));
			}
		} else {
			ut->std_reply(ut, _("Sorry, I couldn't find a user by that name..."));
		}
		return 1;
	}

	if (!stricmp(command,"viewuser") && (!parms || !stricmp(parms,""))) {
		snprintf(buf, sizeof(buf), _("Usage: %cviewuser nick"), PrimaryCommandPrefix());
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command,"viewuser")) {
		USER *Scan = GetUserByNick(parms);
		if (Scan) {
			if (uflag_compare(ut->Flags, Scan->Flags) < 0) {
				ut->std_reply(ut, _("Sorry, you are not high enough level to view that user's information."));
			} else {
				sprintf(buf,_("User Information for %s ..."),Scan->Nick);
				ut->std_reply(ut, buf);
				uflag_to_string(Scan->Flags, buf2, sizeof(buf2));
				sprintf(buf,_("User Flags: %s ('%chelp viewuser flags' for more info"),buf2,PrimaryCommandPrefix());
				ut->std_reply(ut, buf);
				sprintf(buf,_("Password: %s"),Scan->Pass[0] ? Scan->Pass:"\002not set\002");
				ut->std_reply(ut, buf);
				if (Scan->LastHostmask[0]) {
					sprintf(buf,_("Last Known Hostmask: %s"),Scan->LastHostmask);
					ut->std_reply(ut, buf);
				}
				tm tm1,tm2;
				time_t today = time(NULL);
				time_t seen = Scan->Seen;
				localtime_r(&today, &tm1);
				localtime_r(&seen, &tm2);
				//memcpy(&tm1, localtime(&today), sizeof(tm1));
				//memcpy(&tm2, localtime(&seen), sizeof(tm2));
				if (tm1.tm_year == tm2.tm_year && tm1.tm_yday == tm2.tm_yday) {
					strftime(buf2, sizeof(buf2), "\002today\002, %I:%M:%S %p", &tm2);
				} else if (tm1.tm_year == tm2.tm_year && tm2.tm_yday == (tm1.tm_yday-1)) {
					strftime(buf2, sizeof(buf2), "\002yesterday\002, %I:%M:%S %p", &tm2);
				} else if (seen == 0) {
					strcpy(buf2, "never");
				} else {
					strftime(buf2, sizeof(buf2), "%B %d, %Y %I:%M:%S %p", &tm2);
				}
				sprintf(buf,_("Last Seen: %s"),buf2);
				ut->std_reply(ut, buf);

				for (int i=0; i < Scan->NumHostmasks; i++) {
					sprintf(buf, _("Hostmask %d: %s"), i, Scan->Hostmasks[i]);
					ut->std_reply(ut, buf);
				}
				ut->std_reply(ut, _("You can use +host/-host to add/remove hostmasks."));
			}
			UnrefUser(Scan);
		} else {
			ut->std_reply(ut, _("Sorry, I couldn't find a user by that name..."));
		}
		return 1;
	}

	if (!stricmp(command,"viewusers")) {
		ut->std_reply(ut, _("Users in the system: (* only lists users in DB, not ones from auth plugins)"));
		EnumUsers(viewusers_cb,  ut);
		ut->std_reply(ut, _("End of list."));
		return 1;
	}

	if (!stricmp(command,"+host") && (!parms || !stricmp(parms,""))) {
		ut->std_reply(ut, _("Usage: +host [nick] hostmask"));
		return 1;
	}

	if (!stricmp(command,"+host")) {
		char * cparms = zstrdup(parms);
		char * p2 = NULL;
		const char * nick = strtok_r(cparms," ", &p2);
		if (nick == NULL) {
			ut->std_reply(ut, _("Usage: +host [nick] hostmask"));
			zfree(cparms);
			return 1;
		}

		const char *mask = strtok_r(NULL, " ", &p2);
		if (mask) {
			if (!uflag_have_any_of(ut->Flags, UFLAG_MASTER)) {
				ut->std_reply(ut, _("You must be a bot master to use this form of +host. Try it again as +host hostmask to add a hostmask to yourself."));
				zfree(cparms);
				return 1;
			}
		} else {
			mask = nick;
			nick = ut->User ? ut->User->Nick:ut->Nick;
		}

		USER * user = GetUserByNick(nick);
		if (user) {
			if (AddHostmask(user, mask)) {
				ut->std_reply(ut, _("Hostmask added successfully!"));
			} else {
				ut->std_reply(ut, _("Error adding hostmask! (User possibly already has too many?)"));
			}
			UnrefUser(user);
		} else {
			ut->std_reply(ut, _("Sorry, I couldn't find a user by that name..."));
		}
		zfree(cparms);
		return 1;
	}

	if (!stricmp(command,"-host") && (!parms || !stricmp(parms,""))) {
		ut->send_msg(ut, _("Usage: -host [nick] hostmask"));
		return 1;
	}

	if (!stricmp(command,"-host")) {
		char * cparms = zstrdup(parms);
		char * p2 = NULL;
		const char *nick = strtok_r(cparms," ",&p2);
		if (nick == NULL) {
			ut->std_reply(ut, _("Usage: -host [nick] hostmask"));
			zfree(cparms);
			return 1;
		}

		const char *mask = strtok_r(NULL, " ", &p2);
		if (mask) {
			if (!uflag_have_any_of(ut->Flags, UFLAG_MASTER)) {
				ut->std_reply(ut, _("You must be a bot master to use this form of -host. Try it again as -host hostmask to remove a hostmask from yourself."));
				zfree(cparms);
				return 1;
			}
		} else {
			mask = nick;
			nick = ut->User ? ut->User->Nick:ut->Nick;
		}

		USER * user = GetUserByNick(nick);
		if (user) {
			if (ClearHostmask(user, mask)) {
				ut->std_reply(ut, _("Hostmask removed successfully!"));
			} else {
				ut->std_reply(ut, _("Error removing hostmask! (Does this mask exist on user?)"));
			}
			UnrefUser(user);
		} else {
			ut->std_reply(ut, _("Sorry, I couldn't find a user by that name..."));
		}
		zfree(cparms);
		return 1;
	}

	if (!stricmp(command,"chpass") && (!parms || !stricmp(parms,""))) {
		snprintf(buf, sizeof(buf), _("Usage: %cchpass [nick] newpass"), PrimaryCommandPrefix());
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command,"chpass")) {
		char * cparms = zstrdup(parms);
		char * p2 = NULL;
		const char *nick = strtok_r(cparms," ", &p2);
		if (nick == NULL) {
			snprintf(buf, sizeof(buf), _("Usage: %cchpass [nick] newpass"), PrimaryCommandPrefix());
			ut->std_reply(ut, buf);
			zfree(cparms);
			return 1;
		}

		const char *pass = strtok_r(NULL, " ", &p2);
		if (pass) {
			if (!uflag_have_any_of(ut->Flags, UFLAG_MASTER)) {
				ut->send_msg(ut, _("You must be a bot master to use this form of chpass. Try it again as chpass pass to change your own password."));
				zfree(cparms);
				return 1;
			}
		} else {
			pass = nick;
			nick = ut->User ? ut->User->Nick:ut->Nick;
		}

		USER * user = GetUserByNick(nick);
		if (user) {
			SetUserPass(user, pass);
			ut->send_msg(ut, _("Password updated successfully!"));
			UnrefUser(user);
		} else {
			ut->std_reply(ut, _("Sorry, I couldn't find a user by that name..."));
		}
		zfree(cparms);
		return 1;
	}

	if (!stricmp(command,"chflags") && (!parms || !stricmp(parms,""))) {
		snprintf(buf, sizeof(buf), _("Usage: %cchflags nick [+-=]flags"), PrimaryCommandPrefix());
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command,"chflags")) {
		char * cparms = zstrdup(parms);
		char * p2 = NULL;
		char *nick = strtok_r(cparms," ",&p2);
		if (nick == NULL) {
			snprintf(buf, sizeof(buf), _("Usage: %cchflags nick [+-=]flags"), PrimaryCommandPrefix());
			ut->std_reply(ut, buf);
			zfree(cparms);
			return 1;
		}

		char *lvl = strtok_r(NULL, " ",&p2);
		if (lvl == NULL) {
			snprintf(buf, sizeof(buf), _("Usage: %cchflags nick [+-=]flags"), PrimaryCommandPrefix());
			ut->std_reply(ut, buf);
			zfree(cparms);
			return 1;
		}

		USER * user = GetUserByNick(nick);
		if (user) {
			if (uflag_compare(ut->Flags, user->Flags) >= 0) {
				uint32 nm = string_to_uflag(lvl, user->Flags);
				if (!uflag_have_any_of(ut->Flags, UFLAG_MASTER)) {
					nm &= ~UFLAG_MASTER;
					if (!(ut->Flags & UFLAG_DIE)) {
						nm &= ~UFLAG_DIE;
					}
				}
				SetUserFlags(user, nm);
				ut->send_msg(ut, _("User flags updated successfully!"));
			} else {
				ut->send_msg(ut, _("You cannot edit this user's flags!"));
			}
			UnrefUser(user);
		} else {
			ut->std_reply(ut, _("Sorry, I couldn't find a user by that name..."));
		}
		zfree(cparms);
		return 1;
	}

	if (!stricmp(command,"raw") && (!parms || !stricmp(parms,""))) {
		snprintf(buf, sizeof(buf), _("Usage: %craw [netno] raw IRC command"), PrimaryCommandPrefix());
		ut->std_reply(ut, buf);
		return 1;
	}

#if defined(IRCBOT_ENABLE_IRC)
	if (!stricmp(command,"raw")) {
		StrTokenizer st((char *)parms, ' ');
		int netno = ut->NetNo;
		if (st.NumTok() > 1 && is_number(st.stdGetSingleTok(1).c_str())) {
			int x = atoi(st.stdGetSingleTok(1).c_str());
			if (x >= 0 && x < config.num_irc) {
				netno = x;
				snprintf(buf,sizeof(buf),"%s\r\n",st.stdGetTok(2, st.NumTok()).c_str());
			} else {
				ut->std_reply(ut, _("Invalid network number!"));
				return 1;
			}
		} else {
			snprintf(buf,sizeof(buf),"%s\r\n",parms);
		}
		if (config.ircnets[netno].ready) {
			ut->std_reply(ut, _("Executing raw command..."));
			ib_printf(_("Execute: %s"),buf);
			bSend(config.ircnets[netno].sock,buf,strlen(buf));
		} else {
			ut->std_reply(ut, _("That network is not ready!"));
		}
		return 1;
	}
#endif

	if (!stricmp(command,"commands")) {
		bool ex = false;
		int plugfilt = -2;
		if (parms) {
			if (stristr(parms, "ex")) {
				ex = true;
			}
			if (is_number(parms)) {
				plugfilt = atoi(parms);
			}
		}
		std::map<std::string, COMMAND *> unusables;
		uint32 tiers[] = { UFLAG_BASIC_SOURCE|UFLAG_ADVANCED_SOURCE, UFLAG_CHANADMIN, UFLAG_DIE, UFLAG_DJ, UFLAG_HOP, UFLAG_OP, UFLAG_MASTER, 0, 0xFFFFFFFF };
		char * tiernames[] = {
			"Source Control",
			"Channel Admin",
			"Die Flag",
			"DJ",
			"HalfOp",
			"Operator",
			"Master",
			"Other"
		};
		uint32 unot = 0;//UFLAG_MASTER|UFLAG_DIE|UFLAG_OP|UFLAG_HOP|UFLAG_DJ;
		uint32 mask = CM_NO_FLAGS(type);
		if (mask & CM_ALLOW_IRC_PUBLIC) {
			mask = CM_ALLOW_IRC_PUBLIC;
		}
		for (int i=0; tiers[i] != 0xFFFFFFFF; i++) {
			if ((ut->Flags & tiers[i]) || tiers[i] == 0) {
				COMMAND * Scan = EnumCommandsByFlags2(NULL, ut->Flags, tiers[i], unot, mask, plugfilt);
				if (Scan) {
					sprintf(buf, _("%s Commands:"), tiernames[i]);
					ut->std_reply(ut, buf);
				}
				int ind=0;
				buf[0]=0;
				while (Scan) {
					if (ind == 0) {
						if (strlen(buf)) {
							strtrim(buf, " ,");
							ut->std_reply(ut, buf);
						}
						buf[0] = 0;
					}
					if (Scan->mask & type) {
						strcat(buf, Scan->command);
						if (ex) {
							strcat(buf, " (");
							GenCommandMaskString(Scan, buf2, sizeof(buf2));
							strcat(buf, buf2);
							strcat(buf, "), ");
						} else {
							strcat(buf, " ");
						}
						//ut->std_reply(ut, Scan->command);
					} else {
						unusables[Scan->command] = Scan;
					}

					if (ind == 10) {
						ind = 0;
					} else {
						ind++;
					}
					Scan = EnumCommandsByFlags2(Scan, ut->Flags, tiers[i], unot, mask, plugfilt);
				}
				if (strlen(buf)) {
					strtrim(buf, " ,");
					ut->std_reply(ut, buf);
				}
				if (i != 5 || unusables.size() > 0) {
					ut->std_reply(ut, "");
				}
				unot |= tiers[i];
			}
		}

		if (unusables.size() && !(type & (CM_ALLOW_IRC_PUBLIC|CM_FLAG_SLOW))) {
			snprintf(buf, sizeof(buf), _("Commands unavailble in your current context: (c = Chan, p = PM, o = cOnsole, t = from %s, a = async)"), IRCBOT_TEXT);
			ut->std_reply(ut, buf);
			int ind=0;
			buf[0] = 0;
			for (std::map<std::string, COMMAND *>::const_iterator x = unusables.begin(); x != unusables.end(); x++) {
				if (ind == 0) {
					if (strlen(buf)) {
						strtrim(buf, " ,");
						ut->std_reply(ut, buf);
					}
					buf[0] = 0;
				}
				strcat(buf, x->second->command);
				strcat(buf, " (");
				GenCommandMaskString(x->second, buf2, sizeof(buf2));
				strcat(buf, buf2);
				strcat(buf, "), ");
				if (ind == 10) {
					ind = 0;
				} else {
					ind++;
				}
			}

			if (strlen(buf)) {
				strtrim(buf, " ,");
				ut->std_reply(ut, buf);
			}
		}

		return 1;
	}

	if (!stricmp(command,"version")) {
		sprintf(buf, "Version: %%version %%build [Platform: %%platform] [Built on %%build_date at %%build_time] [Uptime: %%uptime]");
		ProcText(buf, sizeof(buf));
		if (ut->Channel && parms && !stricmp(parms,"public") && uflag_have_any_of(ut->Flags, UFLAG_MASTER|UFLAG_OP|UFLAG_HOP|UFLAG_DJ)) {
			ut->send_chan(ut, buf);
		} else {
			ut->std_reply(ut, buf);
		}
		return 1;
	}

	/*
	if (!stricmp(command,"memstat")) {
		IBM_MEMINFO stat, tmp;
		memset(&stat, 0, sizeof(stat));
		memset(&tmp, 0, sizeof(tmp));

		ut->send_msg(ut, _("Module    | Pointers | Memory in Use"));
		ut->send_msg(ut, _("===================================="));
		for (int i = 0; i < MAX_PLUGINS; i++) {
			if (config.plugins[i].plug != NULL) {
				if (SendMessage(i, IB_GETMEMINFO, (char *)&tmp, sizeof(tmp)) == 1) {
					BuildMemStatString(buf, config.plugins[i].plug->plugin_name_short, tmp.num_pointers, tmp.bytes_allocated);
				} else {
					snprintf(buf, sizeof(buf), _("Plugin %s does not provide memory information"), config.plugins[i].plug->plugin_name_short);
				}
				ut->send_msg(ut, buf);
			}
		}

		BuildMemStatString(buf, IRCBOT_NAME, GetNumPtrsAllocated(), GetBytesAllocated());
		ut->send_msg(ut, buf);
		ut->send_msg(ut, _("End of list."));
	}
	*/

	if (!stricmp(command,"help")) {
		if (parms && strlen(parms)) {
			StrTokenizer st((char *)parms, ' ');
			string command = st.stdGetSingleTok(1);
			COMMAND * com = FindCommand(command.c_str());
			if (com) {
				if (com->proc_type == COMMAND_PROC) {
					if (st.NumTok() == 1) {
						ut->send_msg(ut, com->desc);
					} else {
						if (com->help_proc == NULL || !com->help_proc(com->command, st.stdGetTok(2, st.NumTok()).c_str(), ut, type)) {
							ut->send_msg(ut, _("There is no extended help available on this subject..."));
						}
					}
				} else {
					sprintf(buf, _("%s is a user-defined channel command"), command.c_str());
					ut->send_msg(ut, buf);
				}
			} else {
				if (IsCommandPrefix(parms[0])) {
					ut->send_msg(ut, _("I am unable to find help on that topic... (hint: when asking for help on a command, do not put a command prefix like ! or @ or ? in front!)"));
				} else {
					ut->send_msg(ut, _("I am unable to find help on that topic..."));
				}
			}
		} else {
			snprintf(buf, sizeof(buf), _("Usage: %chelp [command]"), PrimaryCommandPrefix());
			ut->std_reply(ut, buf);
		}
		return 1;
	}

#if defined(WIN32)
	if (!stricmp(command, "unhide")) {
		ShowWindow(GetAPI()->hWnd, SW_SHOWNORMAL);
		ut->std_reply(ut, "Showing window on desktop...");
	}
	if (!stricmp(command, "hide")) {
		ShowWindow(GetAPI()->hWnd, SW_HIDE);
		ut->std_reply(ut, "Hiding window from desktop...");
	}
#endif

	return 0;
}

#if defined(IRCBOT_ENABLE_SS)
int RequestSystemCommands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
	if (!config.base.enable_requests) { return 0; }
	char buf[4096];

	if (!stricmp(command,"playlist")) {
		LockMutex(requestMutex);
		if (HAVE_DJ(config.base.req_mode)) {
			LoadMessage("PlaylistMSG1", buf, sizeof(buf));
			str_replaceA(buf,sizeof(buf),"%nick",ut->Nick);
			char * dj = GetRequestDJNick();
			str_replaceA(buf,sizeof(buf), "%dj", dj);
			zfree(dj);
			ProcText(buf,sizeof(buf));
			if (ut->Channel) {
				str_replaceA(buf, sizeof(buf), "%chan", ut->Channel);
				if (parms && !stricmp(parms,"public")) {
					ut->send_chan(ut, buf);
				} else {
					ut->send_notice(ut, buf);
				}
			} else {
				ut->send_msg(ut, buf);
			}
		} else {
			LoadMessage("PlaylistMSG0" ,buf, sizeof(buf));
			str_replaceA(buf,sizeof(buf),"%nick",ut->Nick);
			ProcText(buf,sizeof(buf));
			if (ut->Channel) {
				str_replaceA(buf, sizeof(buf), "%chan", ut->Channel);
				if (parms && !stricmp(parms,"public")) {
					ut->send_chan(ut, buf);
				} else {
					ut->send_notice(ut, buf);
				}
			} else {
				ut->send_msg(ut, buf);
			}
		}
		RelMutex(requestMutex);
		return 1;
	}

	if (!stricmp(command,"current")) {
		LockMutex(requestMutex);
		char * dj = GetRequestDJNick();
		switch(config.base.req_mode) {
			case REQ_MODE_OFF:
				sstrcpy(buf,_("Requests are OFF..."));
				break;
			case REQ_MODE_DJ_NOREQ:
				sprintf(buf,_("The current DJ is: %s on %s but is not currently taking requests..."),dj,config.req_user ? config.req_user->Desc:"Unknown");
				break;
			case REQ_MODE_NORMAL:
			case REQ_MODE_REMOTE:
				sprintf(buf,_("The current DJ is: %s on %s"),dj,config.req_user ? config.req_user->Desc:"Unknown");
				break;
			case REQ_MODE_HOOKED:
				sprintf(buf,_("Requests are on and hooked by %s"), dj);
				break;
		}
		zfree(dj);
		ut->std_reply(ut, buf);
		RelMutex(requestMutex);
		return 1;
	}

	if (!stricmp(command,"reqlogin")) {
		if (type & CM_FLAG_NOHANG) {
			ut->send_msg(ut, "You cannot log in to the request system from this interface...");
			return 1;
		}
		LockMutex(requestMutex);
		if (parms && !stricmp(parms, "off")) {
			RequestsOn(ut, REQ_MODE_DJ_NOREQ);
		} else {
			RequestsOn(ut, REQ_MODE_NORMAL);
		}
		RelMutex(requestMutex);
		if (!LoadMessage("ReqLoggedIn", buf, sizeof(buf))) {
			sstrcpy(buf,_("You are now the active DJ"));
		}
		str_replaceA(buf,sizeof(buf),"%nick",ut->Nick);
		ProcText(buf,sizeof(buf));
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command,"reqlogout")) {
		LockMutex(requestMutex);
		if ((config.req_user && ut->User == config.req_user->User) || uflag_have_any_of(ut->Flags, UFLAG_OP|UFLAG_MASTER)) {
			if (uflag_have_any_of(ut->Flags, UFLAG_OP|UFLAG_MASTER)) {
				sstrcpy(buf,_("OK (Admin Override)"));
			} else {
				sstrcpy(buf,_("OK, you are no longer the active DJ"));
			}
			RequestsOff();
			ut->std_reply(ut, buf);
		} else {
			ut->std_reply(ut, _("You are not the active DJ!"));
		}
		RelMutex(requestMutex);
		return 1;
	}

#if defined(IRCBOT_ENABLE_IRC)
	if (!stricmp(command,"sendreq") && (!parms || !stricmp(parms,""))) {
		snprintf(buf, sizeof(buf), _("Usage: %csendreq text"), PrimaryCommandPrefix());
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command,"sendreq")) {
		//LockMutex(requestMutex);
		//if ((config.req_user && config.req_user->User && ut->User == config.req_user->User) || uflag_have_any_of(ut->Flags, UFLAG_OP|UFLAG_MASTER)) {
			char buf2[2048];
			if (!LoadMessage("ReqToChans", buf2, sizeof(buf2))) {
				sstrcpy(buf,_("Error loading request message!"));
			} else {
				str_replaceA(buf2,sizeof(buf2),"%request",parms);
				ProcText(buf2,sizeof(buf2));
				BroadcastMsg(ut, buf2);
				sstrcpy(buf,_("Request Message Sent!"));
			}
			ut->std_reply(ut, buf);
			/*
		} else {
			ut->std_reply(ut, _("Access Denied"));
		}
		*/
		//RelMutex(requestMutex);
		return 1;
	}

	if (!stricmp(command,"dedicate") && (!parms || !stricmp(parms,""))) {
		snprintf(buf, sizeof(buf), _("Usage: %cdedicate text"), PrimaryCommandPrefix());
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command,"dedicate")) {
		//LockMutex(requestMutex);
		//if ((config.req_user && config.req_user->User && ut->User == config.req_user->User) || uflag_have_any_of(ut->Flags, UFLAG_OP|UFLAG_MASTER)) {
			char buf2[2048];
			if (!LoadMessage("DedToChans", buf2, sizeof(buf2))) {
				sstrcpy(buf,_("Error loading request message!"));
			} else {
				str_replaceA(buf2,sizeof(buf2),"%request",parms);
				ProcText(buf2,sizeof(buf2));
				BroadcastMsg(ut, buf2);
				sstrcpy(buf,_("Dedicated To Message Sent!"));
			}
			ut->std_reply(ut, buf);
			/*
		} else {
			ut->std_reply(ut, _("Access Denied"));
		}
		*/
		//RelMutex(requestMutex);
		return 1;
	}
#endif

	if (!stricmp(command,"request") && (!parms || !stricmp(parms,""))) {
		snprintf(buf, sizeof(buf), _("Usage: %crequest filename.mp3"), PrimaryCommandPrefix());
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command,"request")) {
		LockMutex(requestMutex);
		bool done = false;
		FIND_RESULTS * res = GetFindResult(GenFindID(ut).c_str());
		if (res) {
			if (is_request_number(parms)) {
				int num = atoi(parms)-1;
				if (num >= 0 && num < res->num_songs) {
					DeliverRequest(ut, &res->songs[num], buf, sizeof(buf));
					done = true;
				}
			} else {
				for (int i=0; i < res->num_songs; i++) {
					if (!stricmp(res->songs[i].fn, parms)) {
						DeliverRequest(ut, &res->songs[i], buf, sizeof(buf));
						done = true;
						break;
					}
				}
			}
		}
		if (!done) {
			DeliverRequest(ut, parms, buf, sizeof(buf));
		}
		ProcText(buf,sizeof(buf));
		str_replaceA(buf, sizeof(buf), "%nick", ut->Nick);
		ut->std_reply(ut, buf);
		RelMutex(requestMutex);
		return 1;
	}

	if (!stricmp(command, "find") && (!parms || !strlen(parms))) {
		LockMutex(requestMutex);
		if (REQUESTS_ON(config.base.req_mode) && config.req_iface) {
			if (config.req_iface->find) {
				snprintf(buf, sizeof(buf), _("Usage: %cfind pattern"), PrimaryCommandPrefix());
				ut->std_reply(ut, buf);
			} else {
				if (!LoadMessage("FindDisabled", buf, sizeof(buf))) {
					snprintf(buf, sizeof(buf), "Sorry, the current source plugin doesn't have %cfind enabled...", PrimaryCommandPrefix());
				}
				ProcText(buf, sizeof(buf));
				ut->std_reply(ut, buf);
			}
		} else {
			if (!LoadMessage("FindNoSource", buf, sizeof(buf))) {
				snprintf(buf, sizeof(buf), "Sorry, %cfind is only available when a source plugin is playing...", PrimaryCommandPrefix());
			}
			ProcText(buf, sizeof(buf));
			ut->std_reply(ut, buf);
		}
		RelMutex(requestMutex);
		return 1;
	}

	if (!stricmp(command,"find")) {
		if (strlen(parms) > 125) {
			ut->std_reply(ut, _("The maximum @find query length is 125 characters!"));
			return 1;
		}
		LockMutex(requestMutex);
		if (REQUESTS_ON(config.base.req_mode) && config.req_iface) {
			if (config.req_iface->find) {
				sprintf(buf,_("Matches for %s ..."),parms);
				ut->std_reply(ut, buf);

				int start = 0;
				int max = config.base.max_find_results;
				if ((type & CM_FLAG_SLOW) && (max > 3)) {
					max = 3;
				}

				FIND_RESULTS * res = GetFindResult(GenFindID(ut).c_str());
				if (res && !stricmp(res->query, parms) && res->have_more) {
					start = res->start + res->num_songs;
				}

				config.req_iface->find(parms, max, start, ut, find_finish);
			} else {
				if (!LoadMessage("FindDisabled", buf, sizeof(buf))) {
					snprintf(buf, sizeof(buf), "Sorry, the current source plugin doesn't have %cfind enabled...", PrimaryCommandPrefix());
				}
				ProcText(buf, sizeof(buf));
				ut->std_reply(ut, buf);
			}
		} else {
			if (!LoadMessage("FindNoSource", buf, sizeof(buf))) {
				snprintf(buf, sizeof(buf), "Sorry, %cfind is only available when a source plugin is playing...", PrimaryCommandPrefix());
			}
			ProcText(buf, sizeof(buf));
			ut->std_reply(ut, buf);
		}
		RelMutex(requestMutex);
		return 1;
	}

#if defined(IRCBOT_ENABLE_IRC)
	if (!stricmp(command,"topic") && (!parms || !stricmp(parms,""))) {
		snprintf(buf, sizeof(buf), _("Usage: %ctopic text"), PrimaryCommandPrefix());
		ut->std_reply(ut, buf);
		return 1;
	}

	if (!stricmp(command,"topic")) {
		if (!config.stats.online) {
			LockMutex(sesMutex);
			config.topicOverride = parms;
			RelMutex(sesMutex);
			ut->std_reply(ut, _("Topic set, it will be set until the radio is back online"));
		} else {
			ut->std_reply(ut, _("The radio is currently online, this would have no effect!"));
		}
		return 1;
	}
#endif

	return 0;
}

void find_finish(USER_PRESENCE * ut, FIND_RESULTS * qRes, int max) {
	char buf[4096];
	int match = 0;
	bool have_more = false;
	if (qRes != NULL) {
		match = qRes->num_songs;
		have_more = qRes->have_more;
		SaveFindResult(GenFindID(ut).c_str(), qRes);
		for (int ind=0; ind < max && ind < match; ind++) {
			sprintf(buf,_("%crequest %d: %s"), PrimaryCommandPrefix(), ind+1, nopath(qRes->songs[ind].fn));
			ut->std_reply(ut, buf);
		}
	}
	
	if (match) {
		char buf2[1024];
		sprintf(buf2, _(" (first %d shown)"), max);
		sprintf(buf,_("Found %s%d match%s%s... - Copy and Paste %crequest # or %crequest FILENAME into the channel to request a song.\n"),(have_more?"over ":""),match,(match == 1) ? "":"es",(match > max) ? buf2:"",PrimaryCommandPrefix(),PrimaryCommandPrefix());
	} else {
		sstrcpy(buf,_("No matches found!\n"));
	}
	ut->std_reply(ut, buf);
}
#endif

/**
 * Registers RadioBot internal commands
 */
void RegisterInternalCommands() {
	COMMAND_ACL acl;

	FillACL(&acl, UFLAG_MASTER, 0, 0);
	RegisterCommand("adduser",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE,						InternalCommands,		_("This command adds a user to the bot"));
	RegisterCommand("modunload",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE,						InternalCommands,		_("This command unloads a plugin from your config file"));
	RegisterCommand("modload",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE,						InternalCommands,		_("This command (re)loads a plugin from your config file"));
	RegisterCommand("memstat",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE,						InternalCommands,		_("This command displays dynamic memory information"));
	RegisterCommand("sendq-stats",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE,						InternalCommands,		_("This command displays SendQ statistics"));
	RegisterCommand("sendq-set",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE,						InternalCommands,		_("This command allows you to view or set the SendQ Max BPS"));
	RegisterCommand("cfgdump",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE,						InternalCommands,		_("This command will show you your config file as it sees it. This can help you find if you have any mismatched brackets, etc"));
#if defined(WIN32)
	if (config.base.Fork && GetAPI()->hWnd != NULL) {
		RegisterCommand("hide",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE,						InternalCommands,		_("This command will show my window on it's desktop"));
		RegisterCommand("unhide",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE,						InternalCommands,		_("This command will hide my window from it's desktop"));
	}
#endif

	FillACL(&acl, UFLAG_DIE, 0, 0);
	RegisterCommand("die",			&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE,						InternalCommands,		_("This command shuts down the bot"));
#if defined(IRCBOT_ENABLE_IRC)
	RegisterCommand("reload",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE,						InternalCommands,		_("This command reloads the IRC and Timer sections of your config"));
#endif
	RegisterCommand("restart",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE,						InternalCommands,		_("This command restarts the bot"));
	RegisterCommand("rehash",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE,						InternalCommands,		_("This command reloads your .text file. You an optionally specify a new .text file"));

	FillACL(&acl, 0, UFLAG_MASTER|UFLAG_OP, 0);
	RegisterCommand("modules",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE,						InternalCommands,		_("This command lists the plugins that are currently loaded"));
	RegisterCommand("modinfo",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE,						InternalCommands,		_("This command shows extended information on a plugin"));
	RegisterCommand("deluser",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE,						InternalCommands,		_("This command deletes a user from the bot"));
	COMMAND * c = RegisterCommand("viewuser",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE,		InternalCommands,		_("This command displays a user's information"));
	if (c) {
		c->help_proc = InternalCommandsHelp;
	}
	RegisterCommand("viewusers",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE,						InternalCommands,		_("This command lists the users in the system"));
#if defined(IRCBOT_ENABLE_IRC)
	RegisterCommand("raw",			&acl, CM_ALLOW_IRC_PRIVATE				  ,									InternalCommands,		_("This command sends a RAW command to the IRC server"));
#endif
	RegisterCommand("chflags",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE,						InternalCommands,		_("This command changes a user's flags"));

#if defined(IRCBOT_ENABLE_IRC)
	FillACL(&acl, 0, UFLAG_MASTER|UFLAG_OP|UFLAG_HOP, 0);
	RegisterCommand("dospam",		&acl, CM_ALLOW_ALL,	InternalCommands,		_("This command toggles global spamming"));
	RegisterCommand("dotopic",		&acl, CM_ALLOW_ALL,	InternalCommands,		_("This command toggles global topic management"));
	RegisterCommand("doonjoin",		&acl, CM_ALLOW_ALL,	InternalCommands,		_("This command toggles OnJoin welcome messages"));
#endif

	FillACL(&acl, 0, UFLAG_MASTER|UFLAG_OP|UFLAG_HOP|UFLAG_DJ, 0);
	RegisterCommand("+host",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE,						InternalCommands,		_("This command adds a hostmask to a user"));
	RegisterCommand("-host",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE,						InternalCommands,		_("This command removes a hostmask to a user"));
	RegisterCommand("chpass",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE,						InternalCommands,		_("This command sets a user's password"));
	RegisterCommand("bcast",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE|CM_FLAG_ASYNC,			InternalCommands,		_("This command will broadcast your message to all the channels the bot is on (respecting the channels' DoSpam flag)"));

	FillACL(&acl, 0, 0, 0);
	RegisterCommand("commands",		&acl, CM_ALLOW_ALL,															InternalCommands,		_("This command lists all commands available to you."));
	RegisterCommand("help",			&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE,						InternalCommands,		_("This command will show you the help and command documentation available to you"));
	RegisterCommand("version",		&acl, CM_ALLOW_ALL,															InternalCommands,		_("This command will show you the bot's version"));
	RegisterCommand("whoami",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE,						InternalCommands,		_("This command will show you how the bot sees you"));
	RegisterCommand("hello",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE,						InternalCommands,		_("This command will register your account if it is allowed to, or show you your level if you are recognized"));
	RegisterCommand("identify",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE,						InternalCommands,		_("This command will let you add a hostmask to your account if you are not recognized"));

#if defined(IRCBOT_ENABLE_SS)
	if (config.base.enable_requests) {
		FillACL(&acl, 0, UFLAG_MASTER|UFLAG_OP, 0);
		RegisterCommand("current",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE,					RequestSystemCommands,	_("This command will show you the person currently logged into the request system (if available)"));
#if defined(IRCBOT_ENABLE_IRC)
		RegisterCommand("topic",		&acl, CM_ALLOW_IRC_PRIVATE,												RequestSystemCommands,	_("This command will set a topic to be shown until your shoutcast is back online (you can use %variables)"));
#endif

		FillACL(&acl, 0, UFLAG_DJ, 0);
		RegisterCommand("reqlogout",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE,					RequestSystemCommands,	_("This command will log you out of the request system"));
#if defined(IRCBOT_ENABLE_IRC)
		RegisterCommand("sendreq",		&acl, CM_ALLOW_ALL|CM_FLAG_ASYNC,										RequestSystemCommands,	_("This command will send the message specified by ReqToChans with the text you specify"));
		RegisterCommand("dedicate",		&acl, CM_ALLOW_ALL|CM_FLAG_ASYNC,										RequestSystemCommands,	_("This command will send the message specified by DedToChans with the text you specify"));
#endif
		RegisterCommand("reqlogin",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE,					RequestSystemCommands,	_("This command will log you in to the request system and make you the active DJ (note: you DO NOT need to put your username and pass in here)"));
		uint32 flags = CM_ALLOW_IRC_PUBLIC|CM_ALLOW_CONSOLE_PUBLIC;
		if (config.base.allow_pm_requests) {
			flags |= CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE;
		}

		FillACL(&acl, UFLAG_REQUEST, 0, 0);
		RegisterCommand("request",		&acl, flags,	RequestSystemCommands,	_("This command allows users to request songs"));
		if (config.base.enable_find) {
			RegisterCommand("find",		&acl, flags,	RequestSystemCommands,	_("This will search for file(s) matching your pattern when a source plugin is playing"));
		}

		FillACL(&acl, 0, 0, 0);
		RegisterCommand("playlist",		&acl, CM_ALLOW_ALL,	RequestSystemCommands,	_("This command will show you the URL of the current DJ's playlist (if available)"));
	}
#endif
}
