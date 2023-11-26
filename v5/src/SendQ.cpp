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
#if defined(IRCBOT_ENABLE_IRC)
#include "SendQ.h"
#include <memory>

Titus_Mutex sqMutex;

SENDQ_STATS sq_stats;
SENDQ_STATS * SendQ_GetStats() { return &sq_stats; }

/** \struct SENDQ_ENTRY
 * An entry in the Send Queue (rate-limited data sender, to keep the bot from flooding off servers)
 */
class SENDQ_ENTRY {
public:
	time_t sendAt; ///< Don't send the data before this timestamp
	string data; ///< The data to send
};

class SendQueue {
public:
	DSL_BUFFER buffer;
	int netno = -1;
	time_t lastUsed = time(NULL);
	map<uint8, vector<SENDQ_ENTRY>, greater<uint8>> queue;

	void Queue(uint8 priority, int delay, const string& data) {
		AutoMutex(sqMutex);
		if (queue.find(priority) == queue.end()) {
			queue.insert({ priority, {} });
		}
		queue[priority].push_back({ delay + time(NULL), std::move(data) });
		sq_stats.queued_items++;
		lastUsed = time(NULL);
	}
	void FillBuffer() {
		// Fill 1 second of the highest priority data to send
		time_t tme = time(NULL);
		AutoMutex(sqMutex);
		if (buffer.len < config.base.sendq) {
			for (auto p = queue.begin(); p != queue.end() && buffer.len < config.base.sendq; p++) {
				for (auto x = p->second.begin(); x != p->second.end() && buffer.len < config.base.sendq;) {
					if (x->sendAt <= tme) {
#ifdef DEBUG
						if (netno >= 0) {
							ib_printf("SendQ[%p]: IRC[%d]: Added %zu bytes: %s", this, netno, x->data.length(), x->data.c_str());
						} else {
							ib_printf("SendQ[%p]: non-IRC: Added %zu bytes: %s", this, x->data.length(), x->data.c_str());
						}
#endif
						buffer_append(&buffer, x->data.c_str(), x->data.length());
						sq_stats.queued_items--;
						lastUsed = time(NULL);
						x = p->second.erase(x);
					} else {
						x++;
					}
				}
			}
		}
	}
	bool IsIdle() {
		AutoMutex(sqMutex);
		return ((time(NULL) - lastUsed) > 60);
	}
	bool IsEmpty() {
		AutoMutex(sqMutex);
		return (buffer.len == 0 && queue.size() == 0);
	}

	SendQueue() {
		AutoMutex(sqMutex);
		buffer_init(&buffer, false);
	}
	~SendQueue() {
		AutoMutex(sqMutex);
		buffer_free(&buffer);
		for (auto& p : queue) {
			sq_stats.queued_items -= p.second.size();
		}
		queue.clear();
	}
};
typedef map<T_SOCKET *, shared_ptr<SendQueue>> PriorityQueueType;
PriorityQueueType PriorityQueue;

/*
void Push_History(uint32 level, uint32 val) {
	switch(level) {
		case 1:
			memmove(&sq_stats.history_1[1], &sq_stats.history_1[0], sizeof(uint32)*(SENDQ_HISTORY_LENGTH-1));
			sq_stats.history_1[0] = val;
			break;
		case 30:
			memmove(&sq_stats.history_30[1], &sq_stats.history_30[0], sizeof(uint32)*(SENDQ_HISTORY_LENGTH-1));
			sq_stats.history_30[0] = val;
			break;
		case 60:
			memmove(&sq_stats.history_60[1], &sq_stats.history_60[0], sizeof(uint32)*(SENDQ_HISTORY_LENGTH-1));
			sq_stats.history_60[0] = val;
			break;
	}
}
*/

/**
 * Queues data in the SendQ to be sent.<br>
 * Returns the length of the data queued.
 * @param sock the socket to send the data on
 * @param buf the data to be sent
 * @param len the length of the data to send. If len <= 0 strlen() will be called on buf to determine the length
 * @param priority the priority to assign the data, default is PRIORITY_DEFAULT the higher the #, the faster the data will be sent
 * @param delay the number of seconds to to wait before sending the data
 */
int bSend(T_SOCKET * sock, const char * buf, int32 len, uint8 priority, uint32 delay) {
	if (len <= 0) { len = strlen(buf); }
	
	AutoMutex(sqMutex);
	shared_ptr<SendQueue> q;
	auto x = PriorityQueue.find(sock);
	if (x != PriorityQueue.end()) {
		q = x->second;
	} else {
		q = make_shared<SendQueue>();
		PriorityQueue.insert({ sock, q });
	}

	string data;
	data.assign(buf, len);
#if defined(IRCBOT_ENABLE_IRC)
	if (q->netno == -1) {
		for (int i = 0; i < config.num_irc; i++) {
			if (config.ircnets[i].sock == sock) {
				q->netno = i;
				break;
			}
		}
	}
	if (q->netno >= 0 && config.ircnets[q->netno].sock == sock && config.ircnets[q->netno].log_fp != NULL) {
		char durbuf[32];
		fprintf(config.ircnets[q->netno].log_fp, "%s > %s", ircbot_cycles_ts(durbuf, sizeof(durbuf)), data.c_str());
		fflush(config.ircnets[q->netno].log_fp);
	}
#endif
	q->Queue(priority, delay, data);

	return len;
}

/**
 * Deletes all queue entries that are using a certain socket. This is useful for cases where the socket gets an error or is closed remotely and you need to clear out the SendQ of it's entries
 * @param sock the socket to match in the queue entries
 */
void SendQ_ClearSockEntries(T_SOCKET * sock) {
	AutoMutex(sqMutex);
	auto z = PriorityQueue.find(sock);
	if (z != PriorityQueue.end()) {
		PriorityQueue.erase(z);
	}
}

bool SendQ_HaveQueueItems() {
	AutoMutex(sqMutex);
	for (auto& x : PriorityQueue) {
		if (!x.second->IsEmpty()) {
			return true;
		}
	}
	return false;
}

/**
* Returns true if there are any SendQ queue entries that are for a certain socket.
* @param sock the socket to match in the queue entries
*/
bool SendQ_HaveSockEntries(T_SOCKET * sock) {
	AutoMutex(sqMutex);
	auto x = PriorityQueue.find(sock);
	if (x != PriorityQueue.end()) {
		return !x->second->IsEmpty();
	}
	return false;
}

/**
 * The SendQ thread, it does all of the sending
 * @param lpData a pointer to a TT_THREAD_INFO structure
 */
THREADTYPE SendQ_Thread(void * lpData) {
	TT_THREAD_START

	//int irc_sent[MAX_IRC_SERVERS + 1];

	memset(&sq_stats, 0, sizeof(sq_stats));
	//memset(&irc_sent, 0, sizeof(irc_sent));

	if (config.base.sendq <= 0) {
		config.base.sendq = 300;
	}
	ib_printf(_("%s: SendQ set to maximum of %d bps\n"), IRCBOT_NAME, config.base.sendq);

	time_t nextSend = 0;
	while (!config.shutdown_sendq || SendQ_HaveQueueItems()) {// || PriorityQueue.size()
		time_t cur = time(NULL);
		if (cur >= nextSend) {
			AutoMutex(sqMutex);

			for (auto cursock = PriorityQueue.begin(); cursock != PriorityQueue.end();) {
				if (!config.sockets->IsKnownSocket(cursock->first)) {
					ib_printf("SendQ[%p]: Deleting entries for unknown socket...\n", cursock->first);
					cursock = PriorityQueue.erase(cursock);
				} else if ((cursock->second->IsIdle() || config.shutdown_sendq) && cursock->second->IsEmpty()) {
#ifdef DEBUG
					ib_printf("SendQ[%p]: Deleting buffer...\n", cursock->first);
#endif
					cursock = PriorityQueue.erase(cursock);
				} else {
					cursock->second->FillBuffer();
					if (cursock->second->buffer.len > 0) {
						if (config.sockets->Select_Write(cursock->first, uint32(0)) == 1) {
							int toSend = config.base.sendq;
							if (cursock->second->buffer.len < toSend) { toSend = cursock->second->buffer.len; }
#ifdef DEBUG
							printf("SendQ[%p]: Sending %d bytes of " I64FMT "...\n", cursock->first, toSend, cursock->second->buffer.len);
#endif
							toSend = config.sockets->Send(cursock->first, cursock->second->buffer.data, toSend, false);
							if (toSend <= 0) {
								ib_printf("SendQ[%p]: Send returned %d\n", cursock->first, toSend);
								cursock = PriorityQueue.erase(cursock);
								continue;
							}

							if (cursock->second->netno >= 0) {
								sq_stats.bytesSentIRC += toSend;
							} else {
								sq_stats.bytesSent += toSend;
							}

							buffer_remove_front(&cursock->second->buffer, toSend);

#ifdef DEBUG
							printf("SendQ[%p]: Sent %d bytes.\n", cursock->first, toSend);
#endif
						}
					}
					cursock++;
				}
			}

			if ((cur - config.start_time) > 0) {
				sq_stats.avgBps = (sq_stats.bytesSentIRC + sq_stats.bytesSent) / (cur - config.start_time);
			}
			nextSend = time(NULL) + 1;
		} else {
			safe_sleep(100, true);
		}
	}

	// these should be clear already
	sqMutex.Lock();
	PriorityQueue.clear();
	sqMutex.Release();

	TT_THREAD_END
}

#endif // defined(IRCBOT_ENABLE_IRC)
