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

#include "autodj.h"
#if !defined(WIN32)
//#include <libexplain/rename.h>
//#define HAVE_EXPLAIN
#endif

Resampler * AllocResampler() {
	if (ad_config.Resampler) {
		return ad_config.Resampler->create();
	}
	return new ResamplerSpeex;
}
void FreeResampler(Resampler * r) {
	if (ad_config.Resampler) {
		return ad_config.Resampler->destroy(r);
	} else {
		delete dynamic_cast<ResamplerSpeex *>(r);
	}
}

Deck::Deck(AUTODJ_DECKS ideck) {
	deck = ideck;
	resampler = NULL;
	move = false;
	dec = NULL;
	paused = false;
	selDec = NULL;
	fp = NULL;
	Queue = NULL;
	memset(&meta, 0, sizeof(meta));
	abuffer = AllocAudioBuffer();
	lastWhole = 0;
	didTimingReset = false;
	firstPlay = false;
	inChannels = 0;
	timer_init(&timer, ad_config.Server.Sample);
}
Deck::~Deck() {
	Stop();
	FreeAudioBuffer(abuffer);
}

int64 Deck::getOutputTime() { return timer_value(&timer); }

int32 Deck::GetBufferedSamples() {
	int chans = ad_config.Server.Channels;// >= 2 ? 2:1;
	return abuffer->len / chans;
}

bool Deck::LoadFile(const char * fn, QUEUE * Scan, TITLE_DATA * pMeta) {
	Stop();

	char buf2[1024];
	sprintf(buf2,_("AutoDJ -> [%s] Loaded File: '%s'\n"), GetDeckName(deck), fn);
	if (api->irc) { api->irc->LogToChan(buf2); }
	api->ib_printf2(pluginnum,buf2);
	timer_zero(&timer);
	lastWhole = 0;

	if (!pMeta || !strlen(pMeta->title)) {
		TITLE_DATA ttd;
		memset(&ttd, 0, sizeof(TITLE_DATA));
		ReadMetaData(fn, &ttd);
		if (strlen(ttd.title)) {
			pMeta = &ttd;
		}
	}

	//ad_config.resumePos = 0;
	READER_HANDLE * fp = OpenFile(fn, READER_RDONLY);

	for (int dsel=0; ad_config.Decoders[dsel] != NULL; dsel++) {
		if (fp && ad_config.Decoders[dsel]->is_my_file(fn, fp ? fp->mime_type : NULL)) {
			if (fp) { fp->seek(fp, 0, SEEK_SET); }
			if (ad_config.Decoders[dsel]->create) {
				dec = ad_config.Decoders[dsel]->create();
				dec->SetDeck(deck);
				if (dec->Open(fp, 0) || dec->Open_URL(fn, 0)) {
					selDec = ad_config.Decoders[dsel];
					Queue = CopyQueue(Scan);
					this->fp = fp;
					paused = true;
					firstPlay = true;
					didTimingReset = false;
					if (pMeta && strlen(pMeta->title)) {
						memcpy(&meta, pMeta, sizeof(TITLE_DATA));
					} else {
						memset(&meta, 0, sizeof(meta));
					}
					if (dec->GetLength() == 0 && (Scan && Scan->songlen > 0)) {
						dec->SetLength(Scan->songlen*1000);
					}
					return true;
				}
				ad_config.Decoders[dsel]->destroy(dec);
				dec = NULL;
			}
		}
	}

	if (fp) { fp->close(fp); fp = NULL; }
	sprintf(buf2,_("AutoDJ -> [%s] Could not find decoder for '%s'\n"), GetDeckName(deck), fn);
	if (api->irc) { api->irc->LogToChan(buf2); }
	api->ib_printf2(pluginnum,buf2);
	return false;
}

bool Deck::Play() {
	if (fp && dec && selDec) {
		char buf[1024];
		snprintf(buf, sizeof(buf), _("AutoDJ -> [%s] Play()"), GetDeckName(deck));
		api->ib_printf2(pluginnum,"%s\n", buf);
		if (api->irc) { api->irc->LogToChan(buf); }
		if (firstPlay) {
			if (!stricmp(fp->type, "file")) {
				char * p = strrchr(fp->fn, '\\');
				if (!p) {
					p = strrchr(fp->fn, '/');
				}
				if (p) {
					sstrcpy(buf, p+1);
				} else {
					sstrcpy(buf, fp->fn);
				}
			} else {
				sstrcpy(buf, fp->fn);
			}

			if (!(Queue->flags & QUEUE_FLAG_NO_TITLE_UPDATE) && dec->WantTitleUpdate()) {
				uint32 tmpsonglen = 0;
				if (meta.title[0]) {
					api->ib_printf2(pluginnum, _("AutoDJ -> [%s] Meta Tag: Yes\n"), GetDeckName(deck));
					QueueTitleUpdate(&meta);
				} else {
					api->ib_printf2(pluginnum,_("AutoDJ -> [%s] Meta Tag: No\n"), GetDeckName(deck));
					TITLE_DATA *td = (TITLE_DATA *)zmalloc(sizeof(TITLE_DATA));
					memset(td,0,sizeof(TITLE_DATA));
					sstrcpy(td->title, buf);
					/*
					if (dec->GetLength() > 0 && Timing_IsDone()) {
						Timing_Reset(dec->GetLength());
						didTimingReset = true;
					}
					*/
					QueueTitleUpdate(td);
					zfree(td);
				}
			}

			if (Queue->isreq != NULL) {
				char * msgbuf = (char *)zmalloc(8192);
//				char * msgbuf2 = (char *)zmalloc(8192);
				if (Queue->isreq->nick[0] == 0) {
					api->ib_printf("AutoDJ -> WARNING: Request has empty nick!!!\n");
				} else if (api->irc && Queue->isreq->netno >= 0 && (api->LoadMessage("AutoDJ_ReqToChans",msgbuf, 8192) || api->LoadMessage("ReqToChans",msgbuf, 8192))) {
					if (meta.title[0]) {
						if (meta.artist[0]) {
							snprintf(buf, sizeof(buf), "%s - %s", meta.artist, meta.title);
						} else {
							strlcpy(buf, meta.title, sizeof(buf));
						}
					} //else title will be in buf from above
					str_replace(msgbuf,8192,"%nextsong%",buf);

					if (stristr(msgbuf, "%nextrating%") || stristr(msgbuf, "%nextvotes%")) {
						SONG_RATING r;
						api->ss->GetSongRatingByID(Queue->ID, &r);
						sprintf(buf, "%u", r.Rating);
						str_replaceA(msgbuf, 8192, "%nextrating%", buf);
						sprintf(buf, "%u", r.Votes);
						str_replaceA(msgbuf, 8192, "%nextvotes%", buf);
					}
					str_replace(msgbuf,8192,"%request",Queue->isreq->nick);
					api->ProcText(msgbuf,8192);
					//sprintf(msgbuf2,"PRIVMSG %s :%s\r\n",Queue->isreq->channel,msgbuf);
					//api->irc->SendIRC(Queue->isreq->netno,msgbuf2,strlen(msgbuf2));
					api->BroadcastMsg(NULL, msgbuf);
				}
				//zfree(msgbuf2);
				zfree(msgbuf);
				msgbuf = NULL;
				//msgbuf2 = NULL;
			}

			firstPlay = false;
		}
		ad_config.cursong = Queue;
		paused = false;
		if (ad_config.Queue->on_song_play) { ad_config.Queue->on_song_play(Queue); }
		if (ad_config.Rotation && ad_config.Rotation->on_song_play) { ad_config.Rotation->on_song_play(Queue); }
		return true;
	}

	return false;
}

int32 Deck::AddSamples(short * buffer, int32 samples) {
	//int chans = inChannels;
	Audio_Buffer * tmp = AllocAudioBuffer();
	tmp->realloc(tmp, samples * inChannels);
	memcpy(tmp->buf, buffer, samples * inChannels * sizeof(short));
	int32 ret = AddSamples(tmp);
	FreeAudioBuffer(tmp);
	return ret;
}

int32 Deck::AddSamples(Audio_Buffer * buffer) {
	int32 samples = buffer->len / inChannels;
	if (samples > 0) {

		int32 outChannels = ad_config.Server.Channels;
		short * buf = (short *)zmalloc(samples * outChannels * sizeof(short));
		if (inChannels == outChannels) {
			for (int j=0; j < (samples * outChannels); j++) {
				buf[j] = buffer->buf[j];
			}
		} else if (inChannels == 1 && outChannels == 2) {
			// mono to stereo
			for (int j=0; j < (samples * outChannels); j++) {
				buf[j] = buffer->buf[j/2];
			}
		} else if (inChannels == 2 && outChannels == 1) {
			// stereo to mono
			for (int j=0; j < (samples * outChannels); j++) {
				//buf[j] = 0.5 * ((float)buffer[j*2] + (float)buffer[(j*2)+1]);
				buf[j] = buffer->buf[j*2];
			}
		} else if (inChannels == 1 && outChannels > 2) {
			// mono to stereo+
			for (int j=0; j < (samples * outChannels); j++) {
				buf[j] = buffer->buf[j/inChannels];
			}
		} else if (inChannels >= 5 && outChannels == 2) {
			// 4.1/5.1/7.1 to stereo+
			int l=0;
			for (int j=0; j < (samples * outChannels); l += inChannels) {
				int tmp = (buffer->buf[l+0] + buffer->buf[l+2])/2;
				buf[j++] = tmp;
				tmp = (buffer->buf[l+1] + buffer->buf[l+2])/2;
				buf[j++] = tmp;
			}
		} else if (inChannels > 1 && outChannels >= 2) {
			// stereo+ to stereo+
			int l=0;
			for (int j=0; j < (samples * outChannels); l += inChannels) {
				for (int k=0; k < outChannels; k++) {
					if (k < inChannels) {
						buf[j++] = buffer->buf[l+k];
					} else {
						buf[j++] = 0;
					}
				}
			}
		} else {
			// multiple-channel to mono
			for (int j=0; j < (samples * outChannels); j++) {
				//buf[j] = 0.5 * ((float)buffer[j*2] + (float)buffer[(j*2)+1]);
				//stuff audio track 0 to mono
				buf[j] = buffer->buf[j*inChannels];
			}
		}

		buffer->realloc(buffer, samples * outChannels);
		memcpy(buffer->buf, buf, samples * outChannels * sizeof(short));
		zfree(buf);

		if (resampler) {
			Audio_Buffer * tmp = AllocAudioBuffer();
			samples = resampler->Resample(samples, buffer, tmp);
			buffer->realloc(buffer, tmp->len);
			if (tmp->len > 0) {
				memcpy(buffer->buf, tmp->buf, tmp->len * sizeof(short));
			}
			FreeAudioBuffer(tmp);
		}

		/*
		Audio_Buffer * ab = buffer;
		if (ab->len > 0) {
			FILE * ffp = fopen("out.raw", "ab");
			if (ffp) {
				fwrite(ab->buf, ab->len * sizeof(short), 1, ffp);
				fclose(ffp);
			}
		}
		*/

		if (samples > 0) {
			int ind = abuffer->len;
			abuffer->realloc(abuffer, ind + buffer->len);
			for (int i=0; i < buffer->len; i++) {
				abuffer->buf[ind+i] = buffer->buf[i];
			}
		}
		buffer->reset(buffer);
	}
	return samples;
}

void Deck::Decode(int32 num) {
	int chans = ad_config.Server.Channels;// >= 2 ? 2:1;
	int samples = num * chans;
	try {
		while (abuffer->len < samples && dec && dec->Decode() > 0) {;}
	} catch(...) {
		api->ib_printf2(pluginnum,_("AutoDJ -> Warning: exception occured while decoding audio!\n"));
	}
}

int32 Deck::GetSamples(short * buffer, int32 samples) {
	int chans = ad_config.Server.Channels;// >= 2 ? 2:1;
	Audio_Buffer * tmp = AllocAudioBuffer();
	int32 ret = GetSamples(tmp, samples);
	memcpy(buffer, tmp->buf, ret * chans * sizeof(short));
	FreeAudioBuffer(tmp);
	return ret;
}

int32 Deck::GetSamples(Audio_Buffer * buffer, int32 num) {
	int chans = ad_config.Server.Channels;// >= 2 ? 2:1;
	int samples = num * chans;

	if (abuffer->len < samples) {
		Decode(num);
	}

	if (samples > abuffer->len) {
		samples = abuffer->len;
		num = samples / chans;
	}

	if (num > 0) {
		buffer->realloc(buffer, samples);
		memcpy(buffer->buf, abuffer->buf, samples*sizeof(short));
		if (abuffer->len > samples) {
			memmove(abuffer->buf, abuffer->buf + samples, (abuffer->len - samples)*sizeof(short));
			abuffer->realloc(abuffer, abuffer->len - samples);
		} else {
			abuffer->reset(abuffer);
		}

		if (!didTimingReset && dec && dec->GetLength() > 0 && Timing_IsDone()) {
			Timing_Reset(dec->GetLength());
			didTimingReset = true;
		}

		timer_add(&timer, num);
		if (deck == GetMixer()->curDeck && didTimingReset) {
			if (timer_whole_value(&timer) != lastWhole) {
				uint64 diff = timer_whole_value(&timer) - lastWhole;
				if (diff > 0) {
					Timing_Add(diff);
				}
				lastWhole = timer_whole_value(&timer);
			}
		}
	}

	return num;
}

bool my_move_file(const char * sfn, const char * tfn, char * buf, size_t bufSize) {
	bool ret = false;
	char data[1024];
#if WIN32
	if (MoveFileEx(sfn, tfn, MOVEFILE_COPY_ALLOWED|MOVEFILE_REPLACE_EXISTING)) {
		return true;
	}
#else
	if (rename(sfn, tfn) == 0) {
		return true;
	}
#endif
	FILE * fpi = fopen(sfn, "rb");
	if (fpi != NULL) {
		bool err = false;
		FILE * fpo = fopen(tfn, "wb");
		if (fpo != NULL) {
			while (!feof(fpi)) {
				size_t i = fread(data, 1, sizeof(data), fpi);
				if (i > 0 && fwrite(data, i, 1, fpo) != 1) {
					snprintf(buf,bufSize,_("AutoDJ -> Error writing data to output file while moving song out of main rotation! (error: '%s') (%s -> %s)"), strerror(errno), sfn, tfn);
					err = true;
					break;
				}
			}
			fclose(fpo);
			if (err) {
				unlink(tfn);
			}
		} else {
			snprintf(buf,bufSize,_("AutoDJ -> Error opening output file while moving song out of main rotation! (error: '%s') (%s -> %s)"), strerror(errno), sfn, tfn);
		}
		fclose(fpi);
		if (!err) {
			if (unlink(sfn) == 0) {
				ret = true;
			} else {
				snprintf(buf,bufSize,_("AutoDJ -> Error deleting file to move out of main rotation! (error: '%s') (%s -> %s)"), strerror(errno), sfn, tfn);
				unlink(tfn);
			}
		}
	} else {
		snprintf(buf,bufSize,_("AutoDJ -> Error opening source file to move out of main rotation! (error: '%s') (%s -> %s)"), strerror(errno), sfn, tfn);
	}
	return ret;
}

bool MoveFromQueue(const char * fn) {
	char buf[1024];
	if (strlen(ad_config.Options.MoveDir) == 0) {
		sstrcpy(buf, _("AutoDJ -> !move used but no MoveDir set..."));
		if (api->irc) { api->irc->LogToChan(buf); }
		api->ib_printf2(pluginnum,"%s\n",buf);
		return false;
	}
	if (access(ad_config.Options.MoveDir, 0) != 0) {
		dsl_mkdir(ad_config.Options.MoveDir, 0777);
	}
	if (access(fn, 0) != 0) {
		sprintf(buf, _("AutoDJ -> File '%s' does not exist or we don't have permissions to access it..."), fn);
		if (api->irc) { api->irc->LogToChan(buf); }
		api->ib_printf2(pluginnum,"%s\n",buf);
		return false;
	}
	string tfn = ad_config.Options.MoveDir;
	tfn += PATH_SEPS;
	tfn += nopath(fn);
	bool ret = false;
	//rename(fn, tfn.c_str()) == 0 ||
	if (my_move_file(fn, tfn.c_str(), buf, sizeof(buf))) {
		sprintf(buf, _("AutoDJ -> File '%s' moved out of main rotation..."), nopath(fn));
		ret = true;
	}
	if (api->irc) { api->irc->LogToChan(buf); }
	api->ib_printf2(pluginnum,"%s\n",buf);
	return ret;
}

void Deck::Stop() {
	char buf[1024];
	if (fp) {
		snprintf(buf, sizeof(buf), _("AutoDJ -> [%s] Stop()"), GetDeckName(deck));
		api->ib_printf2(pluginnum,"%s\n", buf);
		if (api->irc) { api->irc->LogToChan(buf); }
	}

	if (resampler) {
		resampler->Close();
		FreeResampler(resampler);
		resampler = NULL;
	}
	if (dec) {
		dec->Close();
		selDec->destroy(dec);
		dec = NULL;
		if (this == ad_config.Decks[GetMixer()->curDeck]) {// && didTimingReset
			Timing_Done();
		}
	}

	abuffer->reset(abuffer);
	paused = false;
	selDec = NULL;
	if (fp) {
		fp->close(fp);
		fp = NULL;
	}
	if (Queue) {
		if (ad_config.Queue->on_song_play_over) { ad_config.Queue->on_song_play_over(Queue); }
		if (ad_config.cursong == Queue) {
			ad_config.cursong = NULL;
		}
		if (move) {
			string sfn = Queue->path;
			sfn += Queue->fn;
			MoveFromQueue(sfn.c_str());
		} else if ((Queue->flags & QUEUE_FLAG_DELETE_AFTER) != 0) {
			string sfn = Queue->path;
			sfn += Queue->fn;
			if (remove(sfn.c_str()) != 0) {
				snprintf(buf, sizeof(buf), _("AutoDJ -> Error deleting file '%s': %s"), sfn.c_str(), strerror(errno));
				if (api->irc) { api->irc->LogToChan(buf); }
				api->ib_printf2(pluginnum, "%s\n", buf);
			}
		}
		FreeQueue(Queue);
		Queue = NULL;
	}
	move = false;
	memset(&meta, 0, sizeof(meta));
}

bool Deck::SetInAudioParams(int channels, int samplerate) {
	if (resampler) {
		resampler->Close();
		FreeResampler(resampler);
		resampler = NULL;
	}
	inChannels = channels;// >= 2 ? 2:1;
	if (samplerate != ad_config.Server.Sample) {
		resampler = AllocResampler();
		bool ret = resampler->Init(deck, ad_config.Server.Channels, samplerate, ad_config.Server.Sample);
		if (!ret) {
			FreeResampler(resampler);
			resampler = NULL;
		}
		return ret;
	}
	return true;
}
