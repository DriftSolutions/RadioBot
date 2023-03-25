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
#define SENDQ_MEMLEAK

/** \struct SENDQ_QUEUE
 * An entry in the Send Queue (rate-limited data sender, to keep the bot from flooding off servers)
 */
struct SENDQ_QUEUE {
	T_SOCKET * sock; ///< The socket to send data to.
	char * data; ///< The data to send
	int netno;
	uint8 priority; ///< The send priority
	int32 len; ///< The length of data to send
	time_t sendAt;
};

#define CLEANUP_AFTER 10
class QueueBuffer {
public:
	DSL_BUFFER buffer;
	int ref_cnt;
	int netno;

	QueueBuffer() {
		buffer_init(&buffer, false);
		ref_cnt = CLEANUP_AFTER;
		netno = -1;
	}
	~QueueBuffer() {
		buffer_free(&buffer);
	}
};

typedef std::vector<SENDQ_QUEUE *> SendQueueType;
typedef std::map<T_SOCKET *, SendQueueType> PriorityQueueType;

// larger queue
Titus_Mutex sqMutex;
PriorityQueueType PriorityQueue;
// immediate buffers
Titus_Mutex sqBufferMutex;
std::map<T_SOCKET *, shared_ptr<QueueBuffer>> buffers;

SENDQ_STATS sq_stats;
SENDQ_STATS * SendQ_GetStats() { return &sq_stats; }

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
	sqMutex.Lock();
#if defined(SENDQ_MEMLEAK)
	SENDQ_QUEUE * Scan = znew(SENDQ_QUEUE);
#else
	SENDQ_QUEUE * Scan = new SENDQ_QUEUE;
#endif
	memset(Scan, 0, sizeof(SENDQ_QUEUE));
	Scan->netno = -1;
#if defined(IRCBOT_ENABLE_IRC)
	for (int i=0; i < config.num_irc; i++) {
		if (config.ircnets[i].sock == sock) {
			Scan->netno = i;
			if (config.ircnets[i].log_fp) {
				char durbuf[32];
				fprintf(config.ircnets[i].log_fp, "%s > %s", ircbot_cycles_ts(durbuf,sizeof(durbuf)), buf);
				fflush(config.ircnets[i].log_fp);
			}
			break;
		}
	}
#endif
	sq_stats.queued_items++;
	Scan->len = len;
	Scan->priority = priority;
	Scan->sock = sock;
	Scan->sendAt = time(NULL)+delay;
#if defined(SENDQ_MEMLEAK)
	Scan->data = (char *)zmalloc(len+1);
#else
	Scan->data = new char[len+1];
#endif
	Scan->data[len] = 0;
	memcpy(Scan->data, buf, len);

	PriorityQueue[sock].push_back(Scan);

	sqMutex.Release();
	return len;
}

/**
 * Deletes a queue entry (data and structure).
 * @param q the queue entry to free
 */
void SendQ_Delete(SENDQ_QUEUE * q) {
	sqMutex.Lock();
#if defined(SENDQ_MEMLEAK)
	zfree(q->data);
	zfree(q);
#else
	delete [] q->data;
	delete q;
#endif
	sqMutex.Release();
}

/**
 * Deletes a queue entry from PriorityQueue and optionally deletes the data and structure.
 * @param q the queue entry to free
 */
/*
void SendQ_RemoveFromQueue(SENDQ_QUEUE * q, bool also_delete=false) {
	sqMutex.Lock();

	auto z = PriorityQueue.find(q->sock);
	if (z != PriorityQueue.end()) {
		for (auto y = z->second.queue.begin(); y != z->second.queue.end(); y++) {
			if (*y == q) {
				if (also_delete) {
					SendQ_Delete(q);
				}
				z->second.queue.erase(y);
				if (z->second.buffer.len == 0 && z->second.queue.size() == 0) { PriorityQueue.erase(z); }
				sq_stats.queued_items--;
				sqMutex.Release();
				return;
			}
		}
	}

	// the pointer should never be found in this search, but I do it anyway just in case
	for (auto x = PriorityQueue.begin(); x != PriorityQueue.end(); x++) {
		for (auto y = x->second.queue.begin(); y != x->second.queue.end(); y++) {
			if (*y == q) {
				if (also_delete) {
					SendQ_Delete(q);
				}
				x->second.queue.erase(y);
				if (x->second.buffer.len == 0 && x->second.queue.size() == 0) { PriorityQueue.erase(x); }
				sq_stats.queued_items--;
				sqMutex.Release();
				return;
			}
		}
	}

	sqMutex.Release();
}
*/

/**
 * Deletes all queue entries that are using a certain socket. This is useful for cases where the socket gets an error or is closed remotely and you need to clear out the SendQ of it's entries
 * @param sock the socket to match in the queue entries
 */
void SendQ_ClearSockEntries(T_SOCKET * sock) {
	AutoMutex(sqMutex);
	auto z = PriorityQueue.find(sock);
	if (z != PriorityQueue.end()) {
		for (auto y = z->second.begin(); y != z->second.end(); y++) {
			SendQ_Delete(*y);
		}
		z->second.clear();
		PriorityQueue.erase(z);
	}

	AutoMutex(sqBufferMutex);
	auto x = buffers.find(sock);
	if (x != buffers.end()) {
		buffer_clear(&x->second->buffer);
		x->second->ref_cnt = 0;
	}
}

bool SendQ_HaveQueueItems() {
	AutoMutex(sqMutex);
	for (auto x = PriorityQueue.begin(); x != PriorityQueue.end(); x++) {
		if (x->second.size() > 0) {
			return true;
		}
	}

	AutoMutex(sqBufferMutex);
	if (buffers.size()) {
		return true;
	}

	return false;
}

/**
* Returns true if there are any SendQ queue entries that are for a certain socket.
* @param sock the socket to match in the queue entries
*/
bool SendQ_HaveSockEntries(T_SOCKET * sock) {
	AutoMutex(sqMutex);
	auto z = PriorityQueue.find(sock);
	if (z != PriorityQueue.end()) {
		if (z->second.size()) {
			return true;
		}
	}

	AutoMutex(sqBufferMutex);
	auto x = buffers.find(sock);
	if (x != buffers.end()) {
		return true;
	}

	return false;
}

/**
 * Returns the number of SendQ queue entries that are for a certain socket.
 * @param sock the socket to match in the queue entries
 */
uint32 SendQ_CountSockEntries(T_SOCKET * sock) {
	uint32 ret = 0;
	AutoMutex(sqMutex);
	auto z = PriorityQueue.find(sock);
	if (z != PriorityQueue.end()) {
		ret += z->second.size();
	}

	AutoMutex(sqBufferMutex);
	auto x = buffers.find(sock);
	if (x != buffers.end()) {
		ret++;
	}

	return ret;
}

/**
 * Grabs the queue entry from the SendQ that has the highest priority to send.
 */
SENDQ_QUEUE * SendQ_GetNextEntry(T_SOCKET * sock, bool also_remove) {
	AutoMutex(sqMutex);
	time_t tme = time(NULL);
	auto x = PriorityQueue.find(sock);
	if (x != PriorityQueue.end()) {
		auto highest = x->second.begin();
		for (auto y = x->second.begin(); y != x->second.end(); y++) {
			if ((*y)->sendAt <= tme && (*y)->priority > (*highest)->priority) {
				highest = y;
			}
		}
		if (highest != x->second.end()) {
			SENDQ_QUEUE * ret = *highest;
			if (also_remove) {
				x->second.erase(highest);
				if (x->second.size() == 0) {
					PriorityQueue.erase(x);
				}
				sq_stats.queued_items--;
			}
			return ret;
		}
	}
	return NULL;
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
			sqMutex.Lock();
			sqBufferMutex.Lock();
			// make sure there is a buffer for all queued items
			for (auto curque = PriorityQueue.begin(); curque != PriorityQueue.end(); curque++) {
				auto curbuf = buffers.find(curque->first);
				if (curbuf == buffers.end()) {
#ifdef DEBUG
					ib_printf("SendQ[%p]: Creating buffer...\n", curque->first);
#endif
					buffers[curque->first] = make_shared<QueueBuffer>();
				}
			}
			sqMutex.Release();

			for (auto cursock = buffers.begin(); cursock != buffers.end(); cursock++) {
				if (!config.sockets->IsKnownSocket(cursock->first)) {
					ib_printf("SendQ[%p]: Deleting entries for unknown socket...\n", cursock->first);
					SendQ_ClearSockEntries(cursock->first);
					//buffer_clear(&cursock->second.buffer);
					continue;
				}

				while (cursock->second->buffer.len < config.base.sendq) {
					SENDQ_QUEUE * Scan = SendQ_GetNextEntry(cursock->first, true);
					if (Scan == NULL) {
						// no more data for this socket, quit looking
						break;
					}

					cursock->second->netno = Scan->netno;
					buffer_append(&cursock->second->buffer, Scan->data, Scan->len);
#ifdef DEBUG
					if (Scan->netno >= 0) {
						ib_printf("SendQ[%p]: IRC[%d]: Added %d bytes: %s", cursock->first, Scan->netno, Scan->len, Scan->data);
					} else {
						ib_printf("SendQ[%p]: non-IRC: Added %d bytes: %s\n", cursock->first, Scan->len, Scan->data);
					}
#endif
					SendQ_Delete(Scan);
				}

				if (cursock->second->buffer.len > 0) {
					cursock->second->ref_cnt = CLEANUP_AFTER;
					if (config.sockets->IsKnownSocket(cursock->first) && config.sockets->Select_Write(cursock->first, uint32(0)) == 1) {
						int toSend = config.base.sendq;
						if (cursock->second->buffer.len < toSend) { toSend = cursock->second->buffer.len; }
#ifdef DEBUG
						printf("SendQ[%p]: Sending %d bytes of " I64FMT "...\n", cursock->first, toSend, cursock->second->buffer.len);
#endif
						toSend = config.sockets->Send(cursock->first, cursock->second->buffer.data, toSend, false);
						if (toSend <= 0) {
							ib_printf("SendQ[%p]: Send returned %d\n", cursock->first, toSend);
							SendQ_ClearSockEntries(cursock->first);
							//buffer_clear(&cursock->second.buffer);
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
			}

			for (auto x = buffers.begin(); x != buffers.end();) {
				if (x->second->buffer.len == 0 && (config.shutdown_sendq || x->second->ref_cnt-- <= 0)) {
#ifdef DEBUG
					ib_printf("SendQ[%p]: Deleting buffer...\n", x->first);
#endif
					buffers.erase(x++);
				} else {
					x++;
				}
			}
			sqBufferMutex.Release();

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
	sqBufferMutex.Lock();
	buffers.clear();
	PriorityQueue.clear();
	sqBufferMutex.Release();
	sqMutex.Release();

	TT_THREAD_END
}

#endif // defined(IRCBOT_ENABLE_IRC)
