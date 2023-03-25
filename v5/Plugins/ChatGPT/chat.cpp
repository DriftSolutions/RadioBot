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

#include "chatgpt.h"

TT_DEFINE_THREAD(ChatThread) {
	TT_THREAD_START
	time_t lastPurge = 0;
	vector<string> stop = { "You:" };
	while (!chatgpt_config.shutdown_now) {
		ChatHistory *c = NULL;
		string line;
		stringstream prompt;

		{
			AutoMutex(chatMutex);
			if (time(NULL) - lastPurge >= 60) {
				time_t ts = time(NULL) - 1800;
				for (chatListType::const_iterator x = sessions.begin(); x != sessions.end();) {
					if (x->second->lastSeen < ts) {
						delete x->second;
						x = sessions.erase(x);
					} else {
						x++;
					}
				}
			}
			if (new_messages.size()) {
				string nick = new_messages.front();
				new_messages.pop();
				auto x = sessions.find(nick);
				if (x != sessions.end() && x->second->new_lines.size()) {
					c = x->second;
					line = x->second->new_lines.front();
					x->second->new_lines.pop();

					for (auto& x : c->lines) {
						prompt << x;
					}
					prompt << "You: " << line << "\n";
					prompt << "Friend:";
				}
			}
		}

		if (c != NULL && line.length()) {
			string reply;
			if (chatgpt_query(prompt.str(), reply, &stop)) {
				c->Pres->std_reply(c->Pres, reply.c_str());
				AutoMutex(chatMutex);
				c->lines.push_back("You: " + line + "\n");
				c->lines.push_back("Friend: " + reply + "\n");
				while (c->lines.size() > 20) {
					c->lines.pop_front();
					c->lines.pop_front();
				}
			}
		}
	}
	chatMutex.Lock();
	for (chatListType::iterator x = sessions.begin(); x != sessions.end(); x = sessions.begin()) {
		delete x->second;
		sessions.erase(x);
	}
	chatMutex.Release();
	TT_THREAD_END
}
