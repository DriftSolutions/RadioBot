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

BOTAPI_DEF * api = NULL;
int pluginnum = 0;

char NetNames[MAX_SOUND_SERVERS][32] = {
	"Net0",
	"Net1",
	"Net2",
	"Net3",
	"Net4",
	"Net5",
	"Net6",
	"Net7",
	"Net8",
	"Net9",
	"Net10",
	"Net11",
	"Net12",
	"Net13",
	"Net14",
	"Net15"
};

sql_rows GetTable(char * query, char **errmsg) {
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
	}
	if (errmsg == NULL && *errmsg2) {
		api->db->Free(*errmsg2);
	}
	return rows;
}

int ChannelLinking_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {
	char buf[1024];
	if (!stricmp(command, "addclink") && (!parms || !strlen(parms))) {
		ut->std_reply(ut, _("Usage: addclink src-network-number #src-channel dest-network-number #dest-channel"));
		return 1;
	}

	if (!stricmp(command, "addclink")) {
		StrTokenizer st((char *)parms, ' ');
		if (st.NumTok() == 4) {
			int net0 = atoi(st.stdGetSingleTok(1).c_str());
			int net1 = atoi(st.stdGetSingleTok(3).c_str());
			sprintf(buf, "INSERT INTO chanlinks (Network0, Channel0, Network1, Channel1) VALUES (%d, '%s', %d, '%s');", net0, st.stdGetSingleTok(2).c_str(), net1, st.stdGetSingleTok(4).c_str());
			if (api->db->Query(buf, NULL, NULL, NULL) == SQLITE_OK) {
				ut->std_reply(ut, _("Added channel link."));
			} else {
				ut->std_reply(ut, _("Error adding channel link!"));
			}
		} else {
			ut->std_reply(ut, _("Usage: addclink src-network-number #src-channel dest-network-number #dest-channel"));
		}
		return 1;
	}

	if (!stricmp(command, "listclinks")) {
		sprintf(buf, "SELECT * FROM chanlinks ORDER By Network0, Channel0, Network1, Channel1");
		sql_rows res = GetTable(buf, NULL);
		if (res.size()) {
			for (sql_rows::iterator x = res.begin(); x != res.end(); x++) {
				sql_row row = x->second;
				sprintf(buf, _("Link %s: between %s on network %s and %s on network %s"), row["ID"].c_str(), row["Channel0"].c_str(), row["Network0"].c_str(), row["Channel1"].c_str(), row["Network1"].c_str());
				ut->std_reply(ut, buf);
			}
		} else {
			ut->std_reply(ut, _("No channel links found!"));
		}
		return 1;
	}

	if (!stricmp(command, "delclink") && (!parms || !strlen(parms))) {
		ut->std_reply(ut, _("Usage: delclink link# (use listclinks to get the # to use)"));
		return 1;
	}

	if (!stricmp(command, "delclink")) {
		int net = atoi(parms);
		sprintf(buf, "DELETE FROM chanlinks WHERE ID=%d", net);
		if (api->db->Query(buf, NULL, NULL, NULL) == SQLITE_OK) {
			ut->std_reply(ut, _("Deleted channel link."));
		} else {
			ut->std_reply(ut, _("Error deleting channel link!"));
		}
		return 1;
	}

	return 0;
}

void DoNetReplace(char * buf, size_t bufSize) {
	char buf2[16];
	for (int i=0; i < MAX_SOUND_SERVERS; i++) {
		sprintf(buf2, "%%net%d%%", i);
		str_replace(buf, bufSize, buf2, NetNames[i]);
	}
}

void SendToLinkedChannels(int srcnet, const char * srcchan, const char * text) {
	char buf[2048];
	sprintf(buf, "SELECT * FROM chanlinks WHERE Network0=%d AND Channel0 LIKE '%s'", srcnet, srcchan);
	sql_rows res = GetTable(buf, NULL);
	if (res.size()) {
		for (sql_rows::iterator x = res.begin(); x != res.end(); x++) {
			sql_row row = x->second;
			sprintf(buf, "PRIVMSG %s :%s\r\n", row["Channel1"].c_str(), text);
			DoNetReplace(buf, sizeof(buf));
			api->irc->SendIRC(atoi(row["Network1"].c_str()),buf,strlen(buf));
			//sprintf(buf, "Link %s: between %s on network %s and %s on network %s", row["ID"].c_str(), row["Channel0"].c_str(), row["Network0"].c_str(), row["Channel1"].c_str(), row["Network1"].c_str());
			//ut->std_reply(ut, buf);
		}
	}
	sprintf(buf, "SELECT * FROM chanlinks WHERE Network1=%d AND Channel1 LIKE '%s'", srcnet, srcchan);
	res = GetTable(buf, NULL);
	if (res.size()) {
		for (sql_rows::iterator x = res.begin(); x != res.end(); x++) {
			sql_row row = x->second;
			sprintf(buf, "PRIVMSG %s :%s\r\n", row["Channel0"].c_str(), text);
			DoNetReplace(buf, sizeof(buf));
			api->irc->SendIRC(atoi(row["Network0"].c_str()),buf,strlen(buf));
			//sprintf(buf, "Link %s: between %s on network %s and %s on network %s", row["ID"].c_str(), row["Channel0"].c_str(), row["Network0"].c_str(), row["Channel1"].c_str(), row["Network1"].c_str());
			//ut->std_reply(ut, buf);
		}
	}
}

int init(int num, BOTAPI_DEF * BotAPI) {
	api = BotAPI;
	pluginnum = num;

	if (api->irc == NULL) {
		api->ib_printf2(pluginnum,_("ChannelLinking -> This version of RadioBot is not supported!\n"));
		return 0;
	}

	DS_CONFIG_SECTION * sec = api->config->GetConfigSection(NULL, "ChannelLinking");
	if (sec) {
		char buf[32];
		for (int i=0; i < MAX_SOUND_SERVERS; i++) {
			sprintf(buf, "NetName%d", i);
			api->config->GetConfigSectionValueBuf(sec, buf, NetNames[i], sizeof(NetNames[i]));
		}
	}
	//char buf[4096];
	//strcpy(buf, );
	api->db->Query("CREATE TABLE IF NOT EXISTS chanlinks(ID INTEGER PRIMARY KEY AUTOINCREMENT, Channel0 VARCHAR(255), Network0 INTEGER, Channel1 VARCHAR(255), Network1 INTEGER);", NULL, NULL, NULL);

	COMMAND_ACL acl;
	api->commands->FillACL(&acl, 0, UFLAG_MASTER|UFLAG_OP, 0);
	api->commands->RegisterCommand_Proc(pluginnum, "addclink",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE, ChannelLinking_Commands, _("This will let you add a channel link"));
	api->commands->RegisterCommand_Proc(pluginnum, "listclinks",	&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE, ChannelLinking_Commands, _("This will let you list your current channel links"));
	api->commands->RegisterCommand_Proc(pluginnum, "delclink",		&acl, CM_ALLOW_IRC_PRIVATE|CM_ALLOW_CONSOLE_PRIVATE, ChannelLinking_Commands, _("This will let you delete an existing channel link"));

	return true;
}

void quit() {
	api->commands->UnregisterCommandsByPlugin(pluginnum);
}

int message(unsigned int msg, char * data, int datalen) {
	char buf[2048];
	switch (msg) {
		case IB_ONJOIN:{
				USER_PRESENCE * ui = (USER_PRESENCE *)data;
				//if (stricmp(ui->nick, api->GetBotNick(ui->netno))) {
				if (ui->Channel) {
					sprintf(buf, _("* %s has joined %s on %%net%d%%"), ui->Nick, ui->Channel, ui->NetNo);
					SendToLinkedChannels(ui->NetNo, ui->Channel, buf);
				}
			}
			break;

		case IB_ONNICK:{
				IBM_NICKCHANGE * ui = (IBM_NICKCHANGE *)data;
				//if (ui->channel && stricmp(ui->old_nick, api->GetBotNick(ui->netno)) && stricmp(ui->new_nick, api->GetBotNick(ui->netno))) {
				if (ui->channel) {
					sprintf(buf, _("* %s is now known as %s on %%net%d%%"), ui->old_nick, ui->new_nick, ui->netno);
					SendToLinkedChannels(ui->netno, ui->channel, buf);
				}
			}
			break;

		case IB_ONPART:{
				IBM_USERTEXT * ui = (IBM_USERTEXT *)data;
				if (ui->type == IBM_UTT_CHANNEL && ui->userpres->Channel && strlen(ui->userpres->Channel)) {// && stricmp(ui->userinfo->Nick, api->GetBotNick(ui->userinfo->NetNo))) {
					sprintf(buf, _("* %s has left %s on %%net%d%% (%s)"), ui->userpres->Nick, ui->userpres->Channel, ui->userpres->NetNo, ui->text ? ui->text:"");
					SendToLinkedChannels(ui->userpres->NetNo, ui->userpres->Channel, buf);
				}
			}
			break;

		case IB_ONQUIT:{
				IBM_USERTEXT * ui = (IBM_USERTEXT *)data;
				//ui = ui;
				if (ui->type == IBM_UTT_CHANNEL && ui->userpres->Channel && strlen(ui->userpres->Channel)) {// && stricmp(ui->userinfo->Nick, api->GetBotNick(ui->userinfo->NetNo))) {
					sprintf(buf, _("* %s has quit IRC on %%net%d%% (%s)"), ui->userpres->Nick, ui->userpres->NetNo, ui->text ? ui->text:"");
					SendToLinkedChannels(ui->userpres->NetNo, ui->userpres->Channel, buf);
				}
			}
			break;

		case IB_ONTEXT:{
				IBM_USERTEXT * ui = (IBM_USERTEXT *)data;
				if (ui->type == IBM_UTT_CHANNEL && ui->userpres->Channel && strlen(ui->userpres->Channel) && stricmp(ui->userpres->Nick, api->irc->GetCurrentNick(ui->userpres->NetNo))) {
					if (!strnicmp(ui->text, "\001ACTION ", 8)) {
						sprintf(buf, _("* %s@%%net%d%% %s"), ui->userpres->Nick, ui->userpres->NetNo, ui->text+8);
						if (buf[strlen(buf)-1] == 0x01) { buf[strlen(buf)-1] = 0; }
					} else {
						sprintf(buf, _("<%s@%%net%d%%> %s"), ui->userpres->Nick, ui->userpres->NetNo, ui->text);
					}
					SendToLinkedChannels(ui->userpres->NetNo, ui->userpres->Channel, buf);
				}
			}
			break;

		case IB_ONKICK:{
				IBM_ON_KICK * ui = (IBM_ON_KICK *)data;
				// && stricmp(ui->userinfo->Nick, api->GetBotNick(ui->userinfo->NetNo)) && stricmp(ui->kicked, api->GetBotNick(ui->userinfo->NetNo))
				if (ui->kicker->Channel && strlen(ui->kicker->Channel)) {
					sprintf(buf, _("* %s@%%net%d%% kicked %s from %s (%s)"), ui->kicker->Nick, ui->kicker->NetNo, ui->kickee, ui->kicker->Channel, (ui->reason && strlen(ui->reason)) ? ui->reason:"No reason");
					SendToLinkedChannels(ui->kicker->NetNo, ui->kicker->Channel, buf);
				}
			}
			break;

		case IB_ONNOTICE:{
				IBM_USERTEXT * ui = (IBM_USERTEXT *)data;
				if (ui->type == IBM_UTT_CHANNEL && ui->userpres->Channel && strlen(ui->userpres->Channel) && stricmp(ui->userpres->Nick, api->irc->GetCurrentNick(ui->userpres->NetNo))) {
					sprintf(buf, _("-%s@%%net%d%%- %s"), ui->userpres->Nick, ui->userpres->NetNo, ui->text);
					SendToLinkedChannels(ui->userpres->NetNo, ui->userpres->Channel, buf);
				}
			}
			break;

		case IB_ONTOPIC:{
				IBM_USERTEXT * ui = (IBM_USERTEXT *)data;
				if (ui->type == IBM_UTT_CHANNEL && ui->userpres->Channel && strlen(ui->userpres->Channel) && stricmp(ui->userpres->Nick, api->irc->GetCurrentNick(ui->userpres->NetNo))) {
					sprintf(buf, _("%s on %%net%d%% changed channel topic in %s to '%s'"), ui->userpres->Nick, ui->userpres->NetNo, ui->userpres->Channel, ui->text);
					SendToLinkedChannels(ui->userpres->NetNo, ui->userpres->Channel, buf);
				}
			}
			break;

		case IB_ONMODE:{
				IBM_USERTEXT * ui = (IBM_USERTEXT *)data;
				// && stricmp(ui->userpres->Nick, api->GetBotNick(ui->userpres->NetNo))
				if (ui->type == IBM_UTT_CHANNEL) {
					sprintf(buf, _("%s on %%net%d%% sets mode in %s: %s"), ui->userpres->Nick, ui->userpres->NetNo, ui->userpres->Channel, ui->text);
					SendToLinkedChannels(ui->userpres->NetNo, ui->userpres->Channel, buf);
				}
			}
			break;
	}
	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{8248028E-8C3B-4478-BF2A-8CFFD91DAF2E}",
	"Channel Linking",
	"Channel Linking Plugin 1.0",
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
