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

#include "chanadmin.h"

BOTAPI_DEF * api = NULL;
int pluginnum = 0;
CHANADMIN_CONFIG chanadmin_config;

int init(int num, BOTAPI_DEF * BotAPI) {
	api = (BOTAPI_DEF *)BotAPI;
	pluginnum = num;

	if (api->irc == NULL) {
		api->ib_printf2(pluginnum,_("ChanAdmin -> This version of RadioBot is not supported!\n"));
		return 0;
	}

	memset(&chanadmin_config, 0, sizeof(chanadmin_config));
	char buf[4096];
	sstrcpy(buf, "CREATE TABLE IF NOT EXISTS chanadmin_oop(ID INTEGER PRIMARY KEY AUTOINCREMENT, Channel VARCHAR(255) COLLATE NOCASE, Nick VARCHAR(255) COLLATE NOCASE, TimeStamp INTEGER);");
	api->db->Query(buf, NULL, NULL, NULL);
	sstrcpy(buf, "CREATE TABLE IF NOT EXISTS chanadmin_pop(ID INTEGER PRIMARY KEY AUTOINCREMENT, Channel VARCHAR(255) COLLATE NOCASE, Nick VARCHAR(255) COLLATE NOCASE, TimeStamp INTEGER);");
	api->db->Query(buf, NULL, NULL, NULL);
	sstrcpy(buf, "CREATE TABLE IF NOT EXISTS chanadmin_aop(ID INTEGER PRIMARY KEY AUTOINCREMENT, Channel VARCHAR(255) COLLATE NOCASE, Nick VARCHAR(255) COLLATE NOCASE, TimeStamp INTEGER);");
	api->db->Query(buf, NULL, NULL, NULL);
	sstrcpy(buf, "CREATE TABLE IF NOT EXISTS chanadmin_hop(ID INTEGER PRIMARY KEY AUTOINCREMENT, Channel VARCHAR(255) COLLATE NOCASE, Nick VARCHAR(255) COLLATE NOCASE, TimeStamp INTEGER);");
	api->db->Query(buf, NULL, NULL, NULL);
	sstrcpy(buf, "CREATE TABLE IF NOT EXISTS chanadmin_vop(ID INTEGER PRIMARY KEY AUTOINCREMENT, Channel VARCHAR(255) COLLATE NOCASE, Nick VARCHAR(255) COLLATE NOCASE, TimeStamp INTEGER);");
	api->db->Query(buf, NULL, NULL, NULL);
//	strcpy(buf, "DROP TABLE chanadmin_bans;");
//	api->db->Query(buf, NULL, NULL, NULL);
	sstrcpy(buf, "CREATE TABLE IF NOT EXISTS chanadmin_bans(ID INTEGER PRIMARY KEY AUTOINCREMENT, NetNo INTEGER, Channel VARCHAR(255) COLLATE NOCASE, Hostmask VARCHAR(255) COLLATE NOCASE, SetBy VARCHAR(255) COLLATE NOCASE, Reason VARCHAR(255), TimeSet INTEGER, TimeExpire INTEGER);");
	api->db->Query(buf, NULL, NULL, NULL);
	sstrcpy(buf, "CREATE TABLE IF NOT EXISTS chanadmin_ignores(ID INTEGER PRIMARY KEY AUTOINCREMENT, Hostmask VARCHAR(255) COLLATE NOCASE, SetBy VARCHAR(255) COLLATE NOCASE, Reason VARCHAR(255) COLLATE NOCASE, TimeSet INTEGER, TimeExpire INTEGER);");
	api->db->Query(buf, NULL, NULL, NULL);
	sstrcpy(buf, "CREATE TABLE IF NOT EXISTS chanadmin_seen(ID INTEGER PRIMARY KEY AUTOINCREMENT, NetNo INTEGER, Channel VARCHAR(255) COLLATE NOCASE, Nick VARCHAR(255) COLLATE NOCASE, User VARCHAR(255) COLLATE NOCASE, TimeStamp INTEGER);");
	api->db->Query(buf, NULL, NULL, NULL);
	sstrcpy(buf, "CREATE UNIQUE INDEX IF NOT EXISTS chanadmin_seen_u ON chanadmin_seen(NetNo, Channel, Nick);");
	api->db->Query(buf, NULL, NULL, NULL);

	COMMAND_ACL acl;
	api->commands->FillACL(&acl, 0, UFLAG_CHANADMIN, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "vop",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE, AutoMode_Commands, _("This will let you manage the AutoVoice list"));
	api->commands->RegisterCommand_Proc(pluginnum, "hop",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE, AutoMode_Commands, _("This will let you manage the AutoHalfOp list"));
	api->commands->RegisterCommand_Proc(pluginnum, "aop",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE, AutoMode_Commands, _("This will let you manage the AutoOp list"));
	api->commands->RegisterCommand_Proc(pluginnum, "pop",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE, AutoMode_Commands, _("This will let you manage the AutoProtect list"));
	api->commands->RegisterCommand_Proc(pluginnum, "oop",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE, AutoMode_Commands, _("This will let you manage the AutoOwner list"));
	api->commands->RegisterCommand_Proc(pluginnum, "ban",	&acl, CM_ALLOW_IRC_PRIVATE, Bans_Commands, _("This will let you manage the ban list"));

	api->commands->FillACL(&acl, 0, UFLAG_MASTER|UFLAG_OP|UFLAG_CHANADMIN, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "ignore",	&acl, CM_ALLOW_IRC_PRIVATE, Ignores_Commands, _("This will let you manage the ignore list"));

	api->commands->FillACL(&acl, 0, 0, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "seen",	&acl, CM_ALLOW_IRC_PUBLIC, Seen_Commands, _("This will let you view the last time a user was seen"));

	chanadmin_config.BanExpiration = 86400;
	DS_CONFIG_SECTION * sec = api->config->GetConfigSection(NULL, "ChanAdmin");
	if (sec) {
		chanadmin_config.ProtectOps = api->config->GetConfigSectionLong(sec, "ProtectOps") > 0 ? true:false;
		chanadmin_config.BanOnKick = api->config->GetConfigSectionLong(sec, "BanOnKick") > 0 ? true:false;
		long x = api->config->GetConfigSectionLong(sec, "BanExpiration");
		if (x >= 0) {
			chanadmin_config.BanExpiration = x;
		}
	}

	return 1;
}

void quit() {
	api->commands->UnregisterCommandsByPlugin(pluginnum);
}

bool do_invite(int netno, const char * chan, const char * nick) {
	stringstream str;
	str << "INVITE " << nick << " " << chan << "\r\n";
	api->irc->SendIRC_Priority(netno, str.str().c_str(), str.str().length(), PRIORITY_IMMEDIATE);
	return true;
}

CHANADMIN_INTERFACE ca_interface = {
	add_ban,
	del_ban,
	do_kick,
	do_ban,
	do_unban
};

bool onbancb(USER * user, void * ptr) {
	IBM_ON_BAN * ibm = (IBM_ON_BAN *)ptr;
	USER_PRESENCE * up = ibm->banner;
	if (api->user->uflag_have_any_of(user->Flags, UFLAG_MASTER|UFLAG_OP)) {
		char buf[256];

		do_unban(up->NetNo, up->Channel, ibm->banmask);

		int64 exp = time(NULL);
		if (chanadmin_config.BanExpiration > 0) {
			exp += chanadmin_config.BanExpiration;
		} else {
			exp = 0;
		}

		do_kick(up->NetNo, up->Channel, up->Nick, "Banned a registered master or op");
		if (api->user->Mask(up->Hostmask, buf, sizeof(buf), 0)) {
			add_ban(up->NetNo, up->Channel, buf, api->irc->GetDefaultNick(up->NetNo), exp, "Banned a registered master or op");
		} else {
			add_ban(up->NetNo, up->Channel, up->Hostmask, api->irc->GetDefaultNick(up->NetNo), exp, "Banned a registered master or op");
		}
		/*
		if (up->User) {
			for (int i=0; i < up->User->NumHostmasks; i++) {
				add_ban(up->NetNo, up->Channel, up->User->Hostmasks[i], api->GetDefaultNick(up->NetNo), exp, "Banned a registered master or op");
			}
		}
		*/

		snprintf(buf, sizeof(buf), "You can not ban %s!", user->Nick);
		up->send_msg(up, buf);
		return false;
	}
	return true;
}

int message(unsigned int msg, char * data, int datalen) {
	switch (msg) {
		case IB_ONMODE:
		case IB_ONTOPIC:
		case IB_ONNOTICE:
		case IB_ONTEXT:
			IBM_USERTEXT * ut;
			ut = (IBM_USERTEXT *)data;
			if (ut->type == IBM_UTT_CHANNEL && ut->userpres->Channel) {
				handle_onjoin_bans(ut->userpres);
				update_channel_seen(ut->userpres);
			}
			if ((msg == IB_ONTEXT || msg == IB_ONNOTICE) && (ut->userpres->Flags & UFLAG_IGNORE)) {
				return 1;
			}
			int ret;
			ret = is_ignored(ut->userpres);
			if (ret && ut->userpres->User) {
				api->user->AddUserFlags(ut->userpres->User, UFLAG_IGNORE);
			}
			return ret;
			break;

		case IB_ONJOIN:{
				USER_PRESENCE * ui = (USER_PRESENCE *)data;
				if (!handle_onjoin_bans(ui)) {
					handle_onjoin_automodes(ui);
					update_channel_seen(ui);
				}
				if (ui->Flags & UFLAG_IGNORE) {
					return 1;
				}
				int ret = is_ignored(ui);
				if (ret && ui->User) {
					api->user->AddUserFlags(ui->User, UFLAG_IGNORE);
				}
				return ret;
			}
			break;

		case IB_ONBAN:{
				IBM_ON_BAN * ibm = (IBM_ON_BAN *)data;
				if (ibm->banner->Channel == NULL) { return 0; }
				if (chanadmin_config.ProtectOps && stricmp(ibm->banner->Nick, api->irc->GetCurrentNick(ibm->banner->NetNo))) {
					if (!api->user->uflag_have_any_of(ibm->banner->Flags, UFLAG_MASTER)) {
						api->user->EnumUsersByLastHostmask(onbancb, ibm->banmask, ibm);
					}
				}
				return 0;
			}
			break;

		case IB_ONKICK:{
				IBM_ON_KICK * ibm = (IBM_ON_KICK *)data;
				if (ibm->kicker->Channel == NULL) { return 0; }
				if (chanadmin_config.BanOnKick && stricmp(ibm->kicker->Nick, api->irc->GetCurrentNick(ibm->kicker->NetNo))) {
					if (!api->user->uflag_have_any_of(ibm->kicker->Flags, UFLAG_MASTER)) {
						if (ibm->kickee_hostmask) {
							USER * user = api->user->GetUser(ibm->kickee_hostmask);
							if (user) {
								if (api->user->uflag_have_any_of(user->Flags, UFLAG_MASTER|UFLAG_OP)) {

									char buf[256];
									int64 exp = time(NULL);
									if (chanadmin_config.BanExpiration > 0) {
										exp += chanadmin_config.BanExpiration;
									} else {
										exp = 0;
									}
									if (api->user->Mask(ibm->kicker->Hostmask, buf, sizeof(buf), 0)) {
										add_ban(ibm->kicker->NetNo, ibm->kicker->Channel, buf, api->irc->GetDefaultNick(ibm->kicker->NetNo), exp, "Kicked a registered master or op");
									} else {
										add_ban(ibm->kicker->NetNo, ibm->kicker->Channel, ibm->kicker->Hostmask, api->irc->GetDefaultNick(ibm->kicker->NetNo), exp, "Kicked a registered master or op");
									}

									snprintf(buf, sizeof(buf), "You can not kick %s!", ibm->kickee);
									ibm->kicker->send_msg(ibm->kicker, buf);
								}
								UnrefUser(user);
							}
						} else {
							api->ib_printf("ChanAdmin -> %s kicked from %s, but don't have full hostmask to check flags...\n", ibm->kickee, ibm->kicker->Channel);
						}
					}
				}
				return 0;
			}
			break;

#if defined(IRCBOT_LITE)
		case IB_LOG:
			static time_t caLastCheck = 0;
			if (caLastCheck <= time(NULL)) {
#else
		case IB_SS_DRAGCOMPLETE:
			//as good as any of a timer for this
#endif
			del_expired_bans();
			del_expired_ignores();
#if defined(IRCBOT_LITE)
				caLastCheck = time(NULL)+30;
			}
#endif
			break;

		case CHANADMIN_IS_LOADED:
			return 1;

		case CHANADMIN_GET_INTERFACE:
			memcpy(data, &ca_interface, sizeof(ca_interface));
			return 1;
	}
	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{E2A85670-8EF1-429a-BA08-692B0DC6712F}",
	"ChanAdmin",
	"Channel Administration Plugin 1.0",
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

sql_rows GetTable(const char * query, char **errmsg) {
//int GetTable(char * query, char ***resultp, int *nrow, int *ncolumn, char **errmsg) {
	sql_rows rows;
	int ret = 0, tries = 0;
	char ** errmsg2 = errmsg;
	if (errmsg == NULL) {
		static char * errmsg3 = NULL;
		errmsg2 = &errmsg3;
	}
	int nrow=0, ncol=0;
	char ** resultp;
	while ((ret = api->db->GetTable(query, &resultp, &nrow, &ncol, errmsg2)) == SQLITE_BUSY && tries < 5) { tries++; }
	if (ret != SQLITE_OK) {
		api->ib_printf(_("GetTable: %s -> %d (%s)\n"), query, ret, *errmsg2);
	} else {
		if (ncol && nrow) {
			for (int i=0; i < nrow; i++) {
				sql_row row;
				for (int s=0; s < ncol; s++) {
					char * p = resultp[(i*ncol)+ncol+s];
					if (!p) { p=""; }
					row[resultp[s]] = p;
				}
				rows[i] = row;
			}
		}
		api->db->FreeTable(resultp);
		//azResult0 = "Name";
		//azResult1 = "Age";
	}
	if (errmsg == NULL && *errmsg2) {
		api->db->Free(*errmsg2);
	}
	return rows;
}
