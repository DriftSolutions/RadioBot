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

typedef std::vector<std::string> stringList;
typedef std::map<std::string, stringList, ci_less> chanList;
typedef std::map<int, chanList> channelUpdates;

size_t buffer_write(void *ptr, size_t size, size_t count, void *stream) {
	BUFFER * buf = (BUFFER *)stream;
	uint32 len = size * count;
	if (len > 0) {
		buf->data = (char *)zrealloc(buf->data, buf->len + len + 1);
		memcpy(buf->data+buf->len, ptr, len);
		buf->len += len;
		buf->data[buf->len] = 0;
	}
	return count;
}

#if defined(IRCBOT_ENABLE_SS)

struct SS_PARSER {
	SS_TYPES type;
	bool (*Prepare)(int num, BUFFER * buf);
	bool (*Parse)(int num, BUFFER * buf, STATS * stats);
};

SS_PARSER ss_parsers[] = {
	{ SS_TYPE_SHOUTCAST, Prepare_SC, Parse_SC },
	{ SS_TYPE_SHOUTCAST2, Prepare_SC2, Parse_SC2 },
	{ SS_TYPE_ICECAST, Prepare_ICE, Parse_ICE },
	{ SS_TYPE_STEAMCAST, Prepare_Steamcast, Parse_Steamcast },
	{ SS_TYPE_SP2P, NULL, NULL }
};

void DoScrapes(STATS * tmp_stats) {
	BUFFER bufs[MAX_SOUND_SERVERS];
	memset(&bufs, 0, sizeof(bufs));
	CURLM * mh = curl_multi_init();
	int i;
	char buf[128];
	for (i=0; i<config.num_ss && !config.shutdown_now; i++) {
		bufs[i].handle = curl_easy_init();
		if (bufs[i].handle) {
			bool ret = false;
			for (int pi=0; ss_parsers[pi].Prepare != NULL; pi++) {
				if (ss_parsers[pi].type == config.s_servers[i].type) {
					ret = ss_parsers[pi].Prepare(i, &bufs[i]);
					break;
				}
			}
			if (ret) {
				curl_easy_setopt(bufs[i].handle, CURLOPT_TIMEOUT, 60);
				curl_easy_setopt(bufs[i].handle, CURLOPT_CONNECTTIMEOUT, 30);
				curl_easy_setopt(bufs[i].handle, CURLOPT_NOPROGRESS, 1);
				curl_easy_setopt(bufs[i].handle, CURLOPT_NOSIGNAL, 1 );
				curl_easy_setopt(bufs[i].handle, CURLOPT_WRITEFUNCTION, buffer_write);
				curl_easy_setopt(bufs[i].handle, CURLOPT_WRITEDATA, &bufs[i]);
				snprintf(buf, sizeof(buf), "RadioBot v%s - www.shoutirc.com (Mozilla)", GetBotVersionString());
				curl_easy_setopt(bufs[i].handle, CURLOPT_USERAGENT, buf);
				curl_multi_add_handle(mh, bufs[i].handle);
			}
		}
	}
	while (i > 0) {
		curl_multi_perform(mh, &i);
		safe_sleep(10, true);
	}
	CURLMsg * msg = curl_multi_info_read(mh, &i);
	while (msg) {
		if (msg->msg == CURLMSG_DONE) {
			for (i=0; i<config.num_ss; i++) {
				if (bufs[i].handle == msg->easy_handle) {
					bufs[i].result = msg->data.result;
					break;
				}
			}
		}
		msg = curl_multi_info_read(mh, &i);
	}
	for (i=0; i<config.num_ss && !config.shutdown_now; i++) {
		if (bufs[i].handle) {
			if (bufs[i].result == CURLE_OK) {
				STATS mStats;
				memset(&mStats,0,sizeof(STATS));
				bool ret = false;
				for (int pi=0; ss_parsers[pi].Prepare != NULL; pi++) {
					if (ss_parsers[pi].type == config.s_servers[i].type) {
						ret = ss_parsers[pi].Parse(i, &bufs[i], &mStats);
						break;
					}
				}
				if (ret) {
					if (stricmp(config.s_stats[i].cursong, mStats.cursong)) {
						mStats.title_changed = true;
					}
					memcpy(&config.s_stats[i], &mStats, sizeof(mStats));
					if (i == 0 || (!tmp_stats->online && config.base.anyserver)) {
						tmp_stats->online = mStats.online;
						sstrcpy(tmp_stats->curdj, mStats.curdj);
						sstrcpy(tmp_stats->cursong, mStats.cursong);
						tmp_stats->bitrate = mStats.bitrate;
					}
					tmp_stats->listeners += mStats.listeners;
					tmp_stats->maxusers += mStats.maxusers;
					tmp_stats->peak += mStats.peak;
				} else {
					config.s_stats[i].online = false;
				}
			} else {
				ib_printf(_("SS Scraper -> Error while querying server [%d]: %d -> %s\n"), i, bufs[i].result, curl_easy_strerror(bufs[i].result));
				config.s_stats[i].online = false;
			}
			curl_multi_remove_handle(mh, bufs[i].handle);
			curl_easy_cleanup(bufs[i].handle);
		}
		if (bufs[i].data) { zfree(bufs[i].data); }
#if defined(OLD_CURL)
		if (bufs[i].url) { zfree(bufs[i].url); }
		if (bufs[i].userpwd) { zfree(bufs[i].userpwd); }
#endif
	}
	curl_multi_cleanup(mh);
}

/**
 * The sound server scraper/info grabber/channel updater thread.
 * @param lpData a ptr to a TT_THREAD_INFO structure
 */
THREADTYPE scThread(VOID *lpData) {
	TT_THREAD_START

	memset(&config.stats,0,sizeof(STATS));
	char buf[1024*8],buf2[1024*8];

	for (int icc=0; icc < config.base.updinterval && !config.shutdown_now; icc++) {
		safe_sleep(1);
	}

	ib_printf(_("Sound Server Info Thread Started\n"));
	bool last_doonlineoffline=false;

	STATS tmp_stats;

//	DRIFT_DIGITAL_SIGNATURE();

	while (!config.shutdown_now) {
		bool do_dj   = false;
		//bool do_song = false;
		bool do_onlineoffline = false;
		/*
		bool ret     = false;
		int i		 = 0;
		*/

		memset(&tmp_stats,0,sizeof(STATS));
		DoScrapes(&tmp_stats);

		/*
		for (i=0; i<config.num_ss && !config.shutdown_now; i++) {
			if (config.s_servers[i].type == SS_TYPE_SC) {
				ret = Scrape_SC(i, &mStats);
			} else if (config.s_servers[i].type == SS_TYPE_ICE) { // icecast
				ret = Scrape_ICE(i, &mStats);
			}

			if (ret) {
				if (stricmp(config.s_stats[i].cursong, mStats.cursong)) {
					mStats.title_changed = true;
				}
				memcpy(&config.s_stats[i], &mStats, sizeof(mStats));
				if (i == 0 || (!tmp_stats.online && config.base.anyserver)) {
					tmp_stats.online = mStats.online;
					strcpy(tmp_stats.curdj, mStats.curdj);
					strcpy(tmp_stats.cursong, mStats.cursong);
					tmp_stats.bitrate = mStats.bitrate;
				}
				tmp_stats.listeners += mStats.listeners;
				tmp_stats.maxusers += mStats.maxusers;
				tmp_stats.peak += mStats.peak;
			} else {
				config.s_stats[i].online = false;
			}
		}
		*/

		if (strcmp(tmp_stats.curdj,config.stats.curdj) && strlen(tmp_stats.curdj)) {
			ib_printf(_("SS Scraper -> do_dj by change\n"));
			do_dj=true;
		}
		/*
		if (strcmp(tmp_stats.cursong,config.stats.cursong)) {
			ib_printf(_("SS Scraper -> do_song by change\n"));
			do_song=true;
		}
		*/

#if defined(IRCBOT_ENABLE_IRC)
		if (tmp_stats.online == true && last_doonlineoffline == false) {
			last_doonlineoffline = true; do_onlineoffline=true;
			LockMutex(sesMutex);
			config.topicOverride = "";
			RelMutex(sesMutex);
		}
		if ((config.stats.online == false && tmp_stats.online == false) && last_doonlineoffline == true) {
			last_doonlineoffline = false; do_onlineoffline=true;
		}
#endif

		if (tmp_stats.online && tmp_stats.cursong[0] == 0 && config.base.anyserver) {
			IBM_SONGINFO si;
			memset(&si, 0, sizeof(si));
			if (SendMessage(-1, SOURCE_GET_SONGINFO, (char *)&si, sizeof(si)) != 0) {
				if (si.artist[0] && si.title[0]) {
					snprintf(tmp_stats.cursong, sizeof(tmp_stats.cursong), "%s - %s", si.artist, si.title);
				} else if (si.title[0]) {
					snprintf(tmp_stats.cursong, sizeof(tmp_stats.cursong), "%s", si.title);
				} else {
					char * p = nopathA(si.fn);
					snprintf(tmp_stats.cursong, sizeof(tmp_stats.cursong), "%s", p);
				}
				ib_printf("SS Scraper -> Using source fallback '%s', got empty title from your sound server...\n", tmp_stats.cursong);
			}
		}

		tmp_stats.title_changed = strcmp(tmp_stats.cursong,config.stats.cursong) ? true:false;
		sstrcpy(tmp_stats.lastdj, config.stats.curdj);
		memcpy(&config.stats,&tmp_stats,sizeof(STATS));

		if (config.shutdown_now) {
			break;
		}

#if defined(IRCBOT_ENABLE_IRC)
		channelUpdates cu;
		if (config.dotopic) {
			int	net=0;

//			bool source_playing = SendMessage(-1, SOURCE_IS_PLAYING, NULL, 0) > 0 ? true:false;

			for (net=0; net < config.num_irc; net++) {
				for (int chan=0; config.ircnets[net].ready == true && chan<config.ircnets[net].num_chans; chan++) {
					if (config.ircnets[net].channels[chan].dotopic) {
						if (config.topicOverride.length()) {
							LockMutex(sesMutex);
							sstrcpy(buf2, config.topicOverride.c_str());
							RelMutex(sesMutex);
						} else if (do_dj && last_doonlineoffline) {
							LoadMessageChannel("TopicDjChange", net, config.ircnets[net].channels[chan].channel.c_str(), buf2, sizeof(buf2));
						} else if (last_doonlineoffline == false) {
							LoadMessageChannel("TopicOffline", net, config.ircnets[net].channels[chan].channel.c_str(), buf2, sizeof(buf2));
						} else {
							LoadMessageChannel("TopicOnline", net, config.ircnets[net].channels[chan].channel.c_str(), buf2, sizeof(buf2));
						}
						ProcText(buf2, sizeof(buf2));
						str_replaceA(buf2, sizeof(buf2), "%me", config.ircnets[net].cur_nick.c_str());
						str_replaceA(buf2, sizeof(buf2), "%chan", config.ircnets[net].channels[chan].channel.c_str());
						if ((!config.ircnets[net].channels[chan].noTopicCheck && strcmp(config.ircnets[net].channels[chan].topic.c_str(),buf2)) || do_onlineoffline || do_dj) {
							if (config.ircnets[net].channels[chan].altTopicCommand.length()) {
								sstrcpy(buf, config.ircnets[net].channels[chan].altTopicCommand.c_str());
								strlcat(buf, "\r\n", sizeof(buf));
								str_replaceA(buf, sizeof(buf), "%chan", config.ircnets[net].channels[chan].channel.c_str());
								str_replaceA(buf, sizeof(buf), "%topic", buf2);
							} else {
								snprintf(buf, sizeof(buf), "TOPIC %s :%s\r\n",config.ircnets[net].channels[chan].channel.c_str(),buf2);
							}
							cu[net][config.ircnets[net].channels[chan].channel].push_back(buf);
						}
					}
				}
			}
		}

		if (config.dospam) {
			int	net=0;

			bool source_playing = SendMessage(-1, SOURCE_IS_PLAYING, NULL, 0) > 0 ? true:false;

			if (do_onlineoffline) {
				for (net=0; net<config.num_irc; net++) {
					for (int chan=0; config.ircnets[net].ready == true && chan<config.ircnets[net].num_chans; chan++) {
						if (config.ircnets[net].channels[chan].dospam) {
							if (config.stats.online == false) {
								LoadMessageChannel("RadioOff", net, config.ircnets[net].channels[chan].channel.c_str(), buf2, sizeof(buf2));
							} else {
								LoadMessageChannel("RadioOn", net, config.ircnets[net].channels[chan].channel.c_str(), buf2, sizeof(buf2));
							}
							ProcText(buf2,sizeof(buf2));
							str_replaceA(buf2,sizeof(buf2),"%chan",config.ircnets[net].channels[chan].channel.c_str());
							str_replaceA(buf2, sizeof(buf2),"%me", config.ircnets[net].cur_nick.c_str());
							snprintf(buf, sizeof(buf), "PRIVMSG %s :%s\r\n",config.ircnets[net].channels[chan].channel.c_str(),buf2);
							cu[net][config.ircnets[net].channels[chan].channel].push_back(buf);
						}
					}
				}
			}

			if (do_dj && config.stats.online) {
				for (net=0; net<config.num_irc; net++) {
					for (int chan=0; config.ircnets[net].ready == true && chan<config.ircnets[net].num_chans; chan++) {
						if (config.ircnets[net].channels[chan].dospam) {
							if (do_onlineoffline || strlen(config.stats.lastdj) == 0) {
								LoadMessageChannel("DJNew", net, config.ircnets[net].channels[chan].channel.c_str(), buf2, sizeof(buf2));
							} else {
								LoadMessageChannel("DJChange", net, config.ircnets[net].channels[chan].channel.c_str(), buf2, sizeof(buf2));
							}
							ProcText(buf2,sizeof(buf2));
							str_replaceA(buf2,sizeof(buf2),"%chan",config.ircnets[net].channels[chan].channel.c_str());
							str_replaceA(buf2, sizeof(buf2),"%me", config.ircnets[net].cur_nick.c_str());
							snprintf(buf, sizeof(buf), "PRIVMSG %s :%s\r\n",config.ircnets[net].channels[chan].channel.c_str(),buf2);
							cu[net][config.ircnets[net].channels[chan].channel].push_back(buf);
						}
					}
				}
			}

			if (config.base.multiserver == 0) {
				if (config.stats.title_changed && config.stats.online) {
					ib_printf(_("SS Scraper -> do_song by change\n"));
					for (net=0; net<config.num_irc; net++) {
						for (int chan=0; config.ircnets[net].ready == true && chan<config.ircnets[net].num_chans; chan++) {
							if (config.ircnets[net].channels[chan].dospam) {
								int si = config.ircnets[net].channels[chan].songinterval;
								if (source_playing && config.ircnets[net].channels[chan].songintervalsource >= 0) {
									si = config.ircnets[net].channels[chan].songintervalsource;
								}
								if (si > 0) {
									if ((config.ircnets[net].channels[chan].lastsong + (si * 60)) > time(NULL) ) {
										continue;
									}
								}
								if (LoadMessageChannel("Song", net, config.ircnets[net].channels[chan].channel.c_str(), buf2, sizeof(buf2))) {
									config.ircnets[net].channels[chan].lastsong = time(NULL);
									ProcText(buf2, sizeof(buf2));
									str_replaceA(buf2,sizeof(buf2), "%chan", config.ircnets[net].channels[chan].channel.c_str());
									str_replaceA(buf2, sizeof(buf2), "%me", config.ircnets[net].cur_nick.c_str());
									snprintf(buf, sizeof(buf), "PRIVMSG %s :%s\r\n", config.ircnets[net].channels[chan].channel.c_str(),buf2);
									cu[net][config.ircnets[net].channels[chan].channel].push_back(buf);
								} else {
									ib_printf("SS Scraper -> Warning: no 'Song' message set in your %s!\n", IRCBOT_TEXT);
									ib_printf("SS Scraper -> If you don't want song announcements you should set DoSpam 0 instead!\n");
								}
								if (LoadMessageChannel("SongPost", net, config.ircnets[net].channels[chan].channel.c_str(), buf2, sizeof(buf2))) {
									ProcText(buf2, sizeof(buf2));
									str_replaceA(buf2,sizeof(buf2), "%chan", config.ircnets[net].channels[chan].channel.c_str());
									str_replaceA(buf2, sizeof(buf2), "%me", config.ircnets[net].cur_nick.c_str());
									snprintf(buf, sizeof(buf), "PRIVMSG %s :%s\r\n", config.ircnets[net].channels[chan].channel.c_str(),buf2);
									bSend(config.ircnets[net].sock,buf,strlen(buf), PRIORITY_SPAM, 60);
								}
							}
						}
					}
				}
			} else {
				for (int i=0; i < config.num_ss; i++) {
					if ((config.base.multiserver == 1 && config.stats.title_changed) || config.s_stats[i].title_changed) {
						if (strlen(config.s_stats[i].cursong) && config.s_stats[i].online) {
							ib_printf(_("SS Scraper -> do_song by change on server %d\n"), i);
							for (net=0; net<config.num_irc; net++) {
								for (int chan=0; config.ircnets[net].ready == true && chan<config.ircnets[net].num_chans; chan++) {
									if (config.ircnets[net].channels[chan].dospam) {
										int si = config.ircnets[net].channels[chan].songinterval;
										if (source_playing && config.ircnets[net].channels[chan].songintervalsource >= 0) {
											si = config.ircnets[net].channels[chan].songintervalsource;
										}
										if (si > 0) {
											if ((config.ircnets[net].channels[chan].lastsong + (si * 60)) > time(NULL) ) {
												continue;
											}
										}
										if (LoadMessageChannel("Song", net, config.ircnets[net].channels[chan].channel.c_str(), buf2, sizeof(buf2))) {
											config.ircnets[net].channels[chan].lastsong = time(NULL);
											str_replaceA(buf2,sizeof(buf2), "%song", config.s_stats[i].cursong);
											str_replaceA(buf2,sizeof(buf2), "%dj", config.s_stats[i].curdj);

											if (i > 0) {
												str_replaceA(buf2,sizeof(buf2),"%req", "Off");
												str_replaceA(buf2,sizeof(buf2), "%ratings%", "Off");
												if (config.base.EnableRatings) {
													if (stristr(buf2, "%rating%") || stristr(buf2, "%votes%")) {
														SONG_RATING r;
														GetSongRating(config.s_stats[i].cursong, &r);
														sprintf(buf, "%u", r.Rating);
														str_replaceA(buf2, sizeof(buf2), "%rating%", buf);
														sprintf(buf, "%u", r.Votes);
														str_replaceA(buf2, sizeof(buf2), "%votes%", buf);
													}
												} else {
													str_replaceA(buf2, sizeof(buf2), "%rating%", "0");
													str_replaceA(buf2, sizeof(buf2), "%votes%", "0");
												}
											}

											sprintf(buf, "%d", i);
											str_replaceA(buf2,sizeof(buf2),"%num",buf);
											sprintf(buf, "%d", config.s_stats[i].listeners);
											str_replaceA(buf2,sizeof(buf2),"%clients",buf);
											sprintf(buf, "%d", config.s_stats[i].peak);
											str_replaceA(buf2,sizeof(buf2),"%peak",buf);
											sprintf(buf, "%d", config.s_stats[i].maxusers);
											str_replaceA(buf2,sizeof(buf2),"%max",buf);
											ProcText(buf2,sizeof(buf2));
											str_replaceA(buf2,sizeof(buf2),"%chan",config.ircnets[net].channels[chan].channel.c_str());
											str_replaceA(buf2, sizeof(buf2),"%me", config.ircnets[net].cur_nick.c_str());
											snprintf(buf, sizeof(buf), "PRIVMSG %s :%s\r\n",config.ircnets[net].channels[chan].channel.c_str(),buf2);
											cu[net][config.ircnets[net].channels[chan].channel].push_back(buf);
										} else {
											ib_printf("SS Scraper -> Warning: no 'Song' message set in your %s!\n", IRCBOT_TEXT);
											ib_printf("SS Scraper -> If you don't want song announcements you should set DoSpam 0 instead!\n");
										}
									}
								}
							}
						}
					}
				}
			}
		}

		for (channelUpdates::const_iterator z = cu.begin(); z != cu.end(); z++) {
			int net = z->first;
			for (chanList::const_iterator y = z->second.begin(); y != z->second.end(); y++) {
				if (y->second.size() > 0) {
					CHANNELS * chan = GetChannel(net, y->first.c_str());
					if (chan && chan->moderateOnUpdate) {
						sprintf(buf, "MODE %s +m\r\n", chan->channel.c_str());
						bSend(config.ircnets[net].sock,buf,strlen(buf),PRIORITY_SPAM);
					}
					for (stringList::const_iterator x = y->second.begin(); x != y->second.end(); x++) {
						bSend(config.ircnets[net].sock,x->c_str(),x->length(),PRIORITY_SPAM);
					}
					if (chan && chan->moderateOnUpdate) {
						sprintf(buf, "MODE %s -m\r\n", chan->channel.c_str());
						bSend(config.ircnets[net].sock,buf,strlen(buf),PRIORITY_SPAM);
					}
				}
			}
		}
#endif // defined(IRCBOT_ENABLE_IRC)

		//SendMessage(-1,IB_SS_DRAGCOMPLETE,(char *)&do_song,sizeof(do_song));
		SendMessage(-1,IB_SS_DRAGCOMPLETE,(char *)&config.stats.title_changed,sizeof(config.stats.title_changed));


		ib_printf(_("SS Scraper -> Info Update Complete, Next Update in %d Seconds\n\n"),config.base.updinterval);
		for (int s_ind=0; s_ind < config.base.updinterval && !config.shutdown_now; s_ind++) {
			safe_sleep(1);
		}
	}

	ib_printf(_("Sound Server Info Thread Ended\n"));
	TT_THREAD_END
}

#endif // defined(IRCBOT_ENABLE_SS)
