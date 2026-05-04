//@AUTOHEADER@BEGIN@
/**********************************************************************\
|                          ShoutIRC RadioBot                           |
|           Copyright 2004-2023 Drift Solutions / Indy Sams            |
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

#include "claude.h"

TT_DEFINE_THREAD(ChatThread) {
	TT_THREAD_START
	time_t lastPurge = 0;
	while (!claude_config.shutdown_now) {
		shared_ptr<ChatHistory> c;

		{
			AutoMutex(chatMutex);

			// Expire context for old convos
			if (time(NULL) - lastPurge >= 60) {
				time_t ts = time(NULL) - 1800;
				for (chatListType::iterator x = sessions.begin(); x != sessions.end();) {
					if (x->second->lastSeen < ts) {
						x = sessions.erase(x);
					} else {
						x++;
					}
				}
			}

			// Check if there are new messages to send
			if (new_messages.size()) {
				string nick = new_messages.front();
				new_messages.pop_front();
				auto x = sessions.find(nick);
				if (x != sessions.end() && x->second->new_lines.size()) {
					c = x->second;

					ClaudeMessage msg;
					msg.role = "user";
					msg.content = c->new_lines.front();
					c->new_lines.pop();
					c->messages.push_back(std::move(msg));
				}
			}
		}

		if (c) {
			list<ClaudeMessage> reply;
			if (claude_query(c->messages, reply)) {
				for (auto& x : reply) {
					if (!x.content.empty()) {
						char* tmp = strdup(x.content.c_str());
						str_replace(tmp, strlen(tmp) + 1, "\r", "");
						while (strstr(tmp, "\n\n") != NULL) {
							str_replace(tmp, strlen(tmp) + 1, "\n\n", "\n");
						}
						char* p2 = NULL;
						char* p = strtok_r(tmp, "\n", &p2);
						while (p != NULL) {
							char* tmp2 = strdup(p);
							strtrim(tmp2);
							if (tmp2[0]) {
								c->Pres->std_reply(c->Pres, tmp2);
							}
							free(tmp2);
							p = strtok_r(NULL, "\n", &p2);
						}
						free(tmp);
					}
				}
				AutoMutex(chatMutex);
				for (auto& x : reply) {
					c->messages.push_back(std::move(x));
				}

				while (c->messages.size() > 20) {
					c->messages.pop_front();
					c->messages.pop_front();
				}
			}
		}
	}
	chatMutex.Lock();
	sessions.clear();
	new_messages.clear();
	chatMutex.Release();
	TT_THREAD_END
}
