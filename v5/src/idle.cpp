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
#include "pcg/pcg_basic.h"

pcg32_random_t staticrng;
Titus_Mutex * getWellMutex() {
	static Titus_Mutex wellMutex;
	return &wellMutex;
}

void init_genrand(uint64_t s1, uint64_t s2) {
	AutoMutexPtr(getWellMutex());
	pcg32_srandom_r(&staticrng, s1, s2);
}
uint32 genrand_int32() {
	AutoMutexPtr(getWellMutex());
	return pcg32_random_r(&staticrng);
}
uint32 genrand_range(uint32 iMin, uint32 iMax) {
	AutoMutexPtr(getWellMutex());
	if (iMin > iMax) {
		uint32 tmp = iMin;
		iMin = iMax;
		iMax = tmp;
	}
	return pcg32_boundedrand_r(&staticrng, iMax - iMin + 1) + iMin;
}

// From Julienne Walker.
uint64_t seed_from_time()
{
    uint64_t now = time(NULL) ^ 0xDEADBEEB;
	if ((now & 0xFFFFFFFF00000000LL) != 0) {
		now ^= 0xFEEDBEEB00000000LL;
	} else {
		now |= 0xFEEDBEEB00000000LL;
	}

	unsigned char *p = (unsigned char*)&now;
    unsigned int seed = 0;
    // Knuth's method (TAOCP vol 2).
    for (size_t i = 0; i < sizeof(now); ++i)
    {
        seed = seed * (UCHAR_MAX + 2U) + p[i];
    }
    return seed;
}

/*
Titus_Mutex * getWellMutex();
#define TEST_NUM 10000

class WellTest {
public:
	int count;
	WellTest() { count = 0; }
};
*/

void seed_randomizer() {
	uint64_t lSeed1=0, lSeed2=0;
	fill_random_buffer((uint8 *)&lSeed1, sizeof(lSeed1));
	fill_random_buffer((uint8 *)&lSeed2, sizeof(lSeed2));
	if (lSeed1 == 0) {
		ib_printf(_("%s: Error getting random bytes for seed, falling back to time-based seed...\n"));
		lSeed1 = seed_from_time();
	}
	if (lSeed2 == 0) {
		ib_printf(_("%s: Error getting random bytes for sequence, falling back to time-based sequence...\n"));
		lSeed2 = seed_from_time() ^ (uint64_t)&lSeed1;
	}
	ib_printf(_("%s: Seeding RNG from " U64FMT "/" U64FMT "\n"), IRCBOT_NAME, lSeed1, lSeed2);
	init_genrand(lSeed1, lSeed2);

	/*
	getWellMutex()->Lock();
	int counts[TEST_NUM] = {0};
	for (int i=0; i < TEST_NUM; i++) {
		uint32 ind = genrand_max(TEST_NUM);
		counts[ind]++;
	}

	std::map<int, WellTest> values;

	FILE * fp = fopen("welltest.txt","wb");
	for (int i=0; i < TEST_NUM; i++) {
		fprintf(fp, "%04d: %u\r\n", i, counts[i]);
		values[counts[i]].count++;
	}
	fprintf(fp, "\r\n\r\n");
	for (std::map<int, WellTest>::const_iterator x = values.begin(); x != values.end(); x++) {
		fprintf(fp, "%d -> %d\r\n", x->first, x->second.count);
	}

	fclose(fp);
	getWellMutex()->Release();
	exit(0);
	*/
}

THREADTYPE IdleThread(VOID *lpData) {
	TT_THREAD_START

	char buf[1024];
	//int i=0;

	DRIFT_DIGITAL_SIGNATURE();

	time_t last_user_backup = 0;
	//time_t last_user_save = 0;
	time_t last_idle_check = 0;
	time_t last_channel_check = 0;
	time_t last_seed = time(NULL);

	while (!config.shutdown_now) {
		if (config.unload_plugin != -1) {
			UnloadPlugin(config.unload_plugin);
			config.unload_plugin = -1;
		}

		if (config.rehash) {
			config.rehash = false;
			UnregisterCommandAliases();
			if (LoadText(config.base.fnText.c_str())) {
				ib_printf(_("%s: Reloaded %s ...\n"), IRCBOT_NAME, nopath(config.base.fnText.c_str()));
			} else {
				ib_printf(_("%s: Error reloading %s ...\n"), IRCBOT_NAME, nopath(config.base.fnText.c_str()));
			}
			RegisterCommandAliases();
			SendMessage(-1, IB_REHASH, NULL, 0);
		}

		if ((time(NULL) - last_channel_check) >= 10) {
#if defined(IRCBOT_ENABLE_IRC)
			LockMutex(sesMutex);
			for (int netno=0; netno < config.num_irc && !config.shutdown_now; netno++) {
				if (config.ircnets[netno].ready == true && (time(NULL) - config.ircnets[netno].readyTime) > 30) {
					if (netno == 0 && config.base.log_chan.length()) {
						LockMutex(config.ircnets[netno].ial->hMutex);
						IAL_CHANNEL * chan = config.ircnets[netno].ial->GetChan(config.base.log_chan.c_str(), true);
						if (chan == NULL) {
							ib_printf(_("[irc-%d] Trying to rejoin log channel...\n"), netno);
							JoinChannel(0, config.base.log_chan.c_str(), config.base.log_chan_key.c_str());
						}
						RelMutex(config.ircnets[netno].ial->hMutex);
					}
					for (int chans=0; chans < config.ircnets[netno].num_chans; chans++) {
						if (!config.ircnets[netno].channels[chans].joined) {
							CHANNELS * c = &config.ircnets[netno].channels[chans];
							//char * buf = NULL;
							ib_printf(_("[irc-%d] Trying to rejoin channel '%s'...\n"), netno, c->channel.c_str());
							JoinChannel(netno, c);
						}
					}
				}
			}
			RelMutex(sesMutex);
#endif
			last_channel_check = time(NULL);
#if defined(IRCBOT_ENABLE_SS)
			remote_req_timeout();
#endif
		}

		if ((time(NULL) - last_idle_check) >= 60) {
#if defined(IRCBOT_ENABLE_IRC)
			for (int i=0; i < config.num_irc && !config.shutdown_now; i++) {
				if (config.ircnets[i].ready == true) {
					sprintf(buf,"IRCBOT_IDLE_" I64FMT "\r\n",ircbot_get_cycles());
					//sprintf(buf,"PING %s :"I64FMT"\r\n", config.ircnets[i].server.c_str(), ircbot_get_cycles());
					bSend(config.ircnets[i].sock,buf,0,PRIORITY_IMMEDIATE+1);
					if (config.ircnets[i].regainNick && stricmp(config.ircnets[i].base_nick.c_str(),config.ircnets[i].cur_nick.c_str())) {
						ib_printf(_("[irc-%d] Trying to retake main nickname...\n"), i);
						sprintf(buf,"NICK %s\r\n", config.ircnets[i].base_nick.c_str());
						bSend(config.ircnets[i].sock,buf,0,PRIORITY_DEFAULT);
					}
				}
			}
#endif
			last_idle_check = time(NULL);
#if defined(IRCBOT_ENABLE_SS)
			ExpireFindResults();
#endif
		}

		/*
		if ((time(NULL) - last_user_save) >= 120) {
			ib_printf("IRCBot: Saving user file...\n");
			SaveUsers();
			last_user_save = time(NULL);
		}
		*/

		if ((time(NULL) - last_user_backup) >= (60*60*2)) {
			if (config.base.backup_days > 0) {
				Directory dir("./backup");
				time_t expire = time(NULL) - (60*60*24*config.base.backup_days);
				while (dir.Read(buf, sizeof(buf))) {
					std::string ffn = "./backup/";
					ffn += buf;
					struct stat st;
					if (stat(ffn.c_str(), &st) == 0) {
						if (!S_ISDIR(st.st_mode) && st.st_mtime < expire) {
							ib_printf(_("%s: Expiring backup file: %s\n"), IRCBOT_NAME, ffn.c_str());
							if (remove(ffn.c_str()) != 0) {
								ib_printf(_("%s: Error removing backup file: %s (%s)\n"), IRCBOT_NAME, ffn.c_str(), strerror(errno));
							}
						}
					} else {
						ib_printf(_("%s: Error stat()ing file: %s\n"), IRCBOT_NAME, ffn.c_str());
					}
				}
			}
		}

		if ((time(NULL) - last_seed) >= 86400) {
			seed_randomizer();
			last_seed = time(NULL);
		}

		safe_sleep(1);
	}

	TT_THREAD_END
}

