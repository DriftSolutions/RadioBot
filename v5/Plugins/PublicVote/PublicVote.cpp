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
#include "../ChanAdmin/chanadminapi.h"

BOTAPI_DEF *api=NULL;
int pluginnum; // the number we were assigned

Titus_Mutex VoteMutex;
class VOTEINFO {
public:
	time_t lastVote;
	bool did_vote;
	int warned;

	VOTEINFO() {
		lastVote = 0;
		warned = 0;
		did_vote = false;
	}
};
typedef std::map<string, VOTEINFO, ci_less> voteList;
voteList voters;

struct PV_CONFIG {
	char next_vote_command[64];
	uint32 cursong;
	CHANADMIN_INTERFACE ca;
	int num_votes_next;
	int votes_needed_to_next;
	int next_min_time_per_user;
	int next_min_time_per_user_kick;
	int next_min_time_per_user_ban_time;
};

PV_CONFIG pvc;

void LoadConfig() {
	memset(&pvc, 0, sizeof(pvc));
	pvc.votes_needed_to_next = 3;
	pvc.next_min_time_per_user_ban_time = 300;
	DS_CONFIG_SECTION * sec = api->config->GetConfigSection(NULL, "PublicVote");
	if (sec) {
		int x = api->config->GetConfigSectionLong(sec, "NextVotes");
		if (x >= 1) {
			pvc.votes_needed_to_next = x;
		}
		x = api->config->GetConfigSectionLong(sec, "NextMinTimePerUser");
		if (x >= 0) {
			pvc.next_min_time_per_user = x;
		}
		x = api->config->GetConfigSectionLong(sec, "NextMinTimePerUserKick");
		if (x >= 0) {
			pvc.next_min_time_per_user_kick = x;
		}
		x = api->config->GetConfigSectionLong(sec, "NextMinTimePerUserBanTime");
		if (x >= 0) {
			pvc.next_min_time_per_user_ban_time = x;
		}
		api->config->GetConfigSectionValueBuf(sec, "NextVoteTrigger", pvc.next_vote_command, sizeof(pvc.next_vote_command));

		if (pvc.next_min_time_per_user_kick && api->SendMessage(-1, CHANADMIN_IS_LOADED, NULL, 0)) {
			api->SendMessage(-1, CHANADMIN_GET_INTERFACE, (char *)&pvc.ca, sizeof(pvc.ca));
		} else {
			api->ib_printf(_("Public Vote Plugin -> The ChanAdmin plugin must be loaded to use NextMinTimePerUserKick, disabling...\n"));
			pvc.next_min_time_per_user_kick = 0;
		}

	}

	if (pvc.next_vote_command[0] == 0) {
		strcpy(pvc.next_vote_command, "next-vote");
	}
};

void StdReplace(char * buf, int bufSize, USER_PRESENCE * ut) {
	char buf2[32];
	str_replaceA(buf, bufSize, "%chan", ut->Channel);
	str_replaceA(buf, bufSize, "%nick", ut->Nick);

	sprintf(buf2, "%d", pvc.votes_needed_to_next - pvc.num_votes_next);
	str_replaceA(buf, bufSize, "%votes_needed%", buf2);

	sprintf(buf2, "%d", pvc.votes_needed_to_next);
	str_replaceA(buf, bufSize, "%votes_total%", buf2);

	sprintf(buf2, "%d", pvc.num_votes_next);
	str_replaceA(buf, bufSize, "%votes_have%", buf2);

	api->ProcText(buf, bufSize);
}

int PublicVote_Commands(const char * command, const char * parms, USER_PRESENCE * ut, uint32 type) {

	if (type == CM_ALLOW_IRC_PUBLIC && !stricmp(command, pvc.next_vote_command)) {
		char buf[1024];
		if (api->SendMessage(-1, SOURCE_IS_PLAYING, NULL, 0)) {
			uint32 songid = 0;
			if (api->SendMessage(-1, SOURCE_GET_SONGID, (char *)&songid, sizeof(songid)) == 1) {
				LockMutex(VoteMutex);
				if (songid != pvc.cursong) {
					pvc.cursong = songid;
					pvc.num_votes_next = 0;
					for (voteList::iterator x = voters.begin(); x != voters.end(); x++) {
						x->second.did_vote = false;
					}
				}
				string nick = ut->User ? ut->User->Nick:ut->Nick;
				voteList::iterator x = voters.find(nick);
				if (x != voters.end()) {
					if (pvc.next_min_time_per_user) {
						if ((time(NULL) - x->second.lastVote) < pvc.next_min_time_per_user) {
							x->second.warned++;
							if (pvc.next_min_time_per_user_kick == 0 || x->second.warned <= pvc.next_min_time_per_user_kick) {
								int64 tme = pvc.next_min_time_per_user - (time(NULL) - x->second.lastVote);
								sprintf(buf, "You may vote again in " I64FMT " seconds...", tme);
								ut->send_notice(ut, buf);
								if (pvc.next_min_time_per_user_kick && x->second.warned == pvc.next_min_time_per_user_kick) {
									sstrcpy(buf, "Warning: further use of this command before your time limit has expired will result in a kick/ban!");
									ut->send_notice(ut, buf);
								}
							} else if (x->second.warned > pvc.next_min_time_per_user_kick) {
								sprintf(buf, "Abusing !%s", pvc.next_vote_command);
								if (pvc.next_min_time_per_user_ban_time) {
									pvc.ca.add_ban(ut->NetNo, ut->Channel, ut->Hostmask, api->irc->GetCurrentNick(ut->NetNo), time(NULL)+pvc.next_min_time_per_user_ban_time, buf);
								} else {
									pvc.ca.do_kick(ut->NetNo, ut->Channel, ut->Nick, buf);
								}
							}
							RelMutex(VoteMutex);
							return 1;
						} else {
							x->second.warned = 0;
						}
					}

					if (x->second.did_vote) {
						if (!api->LoadMessage("VoteNextAlreadyVoted", buf, sizeof(buf))) {
							strcpy(buf, "You have already voted, %nick! You cannot vote on the same song more than once...");
						}
						StdReplace(buf, sizeof(buf), ut);
						ut->send_notice(ut, buf);
						RelMutex(VoteMutex);
						return 1;
					}
				}
				voters[nick].did_vote = true;
				voters[nick].lastVote = time(NULL);
				pvc.num_votes_next++;
				RelMutex(VoteMutex);

				if (pvc.num_votes_next >= pvc.votes_needed_to_next) {
					if (!api->LoadMessage("VoteNextPassed", buf, sizeof(buf))) {
						strcpy(buf, "Vote passed! Skipping to next song...");
					}
					StdReplace(buf, sizeof(buf), ut);
					ut->send_chan(ut, buf);
					api->SendMessage(-1, SOURCE_C_NEXT, NULL, 0);
				} else {
					if (!api->LoadMessage("VoteNextThanks", buf, sizeof(buf))) {
						strcpy(buf, "Thank you for voting %nick, %votes_needed% more votes needed to skip this song...");
					}
					StdReplace(buf, sizeof(buf), ut);
					ut->send_chan(ut, buf);
				}
				return 1;
			}
		}
		if (!api->LoadMessage("VoteNextNoSrc", buf, sizeof(buf))) {
			strcpy(buf, "Sorry, Auto DJ is not playing right now...");
		}
		StdReplace(buf, sizeof(buf), ut);
		ut->send_notice(ut, buf);
		return 1;
	}

	return 0;
}

int init(int num, BOTAPI_DEF * BotAPI) {
	pluginnum=num;
	api=BotAPI;
	if (api->irc == NULL) {
		api->ib_printf2(pluginnum,_("Public Vote Plugin -> This version of RadioBot is not supported!\n"));
		return 0;
	}
	LoadConfig();
	COMMAND_ACL acl;
	api->commands->FillACL(&acl, 0, 0, 0);
	api->commands->RegisterCommand_Proc(pluginnum, pvc.next_vote_command, &acl, CM_ALLOW_IRC_PUBLIC, PublicVote_Commands, "This command will let you vote to skip to the next song");
	return 1;
}

void quit() {
	api->ib_printf(_("Public Vote Plugin -> Shutting down...\n"));
	api->commands->UnregisterCommandByName(pvc.next_vote_command);
	LockMutex(VoteMutex);
	voters.clear();
	RelMutex(VoteMutex);
}

int message(unsigned int MsgID, char * data, int datalen) {
	switch(MsgID) {

		case IB_ONNICK:{
				IBM_NICKCHANGE * in = (IBM_NICKCHANGE *)data;
				LockMutex(VoteMutex);
				voteList::iterator x = voters.find(in->old_nick);
				if (x != voters.end()) {
					printf("Public Vote Plugin -> Updating record for new nickname: %s -> %s ...\n", in->old_nick, in->new_nick);
					//VOTEINFO tmp = x->second;
					//voters.erase(x);
					voters[in->new_nick] = x->second;
				}
				RelMutex(VoteMutex);
				return 0;
			}
			break;
	}
	return 0;
}

PLUGIN_PUBLIC plugin = {
	IRCBOT_PLUGIN_VERSION,
	"{524615E5-D7A6-43C8-B066-EF9C267F818B}",
	"Public Vote",
	"Public Vote Plugin 1.0",
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
