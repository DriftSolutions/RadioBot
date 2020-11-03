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

#include "mp3sync.h"

#define MP3_BUFFER_SIZE 65536

mpg123_handle * dec_init(const char * src, long * rate) {
	int err = 0;
	mpg123_handle * mh = mpg123_new(NULL, &err);
	if (mh == NULL) {
		ds_printf(0, "Error creating audio handle: %s!\n", mpg123_plain_strerror(err));
		return NULL;
	}

#if defined(MPG123_SKIP_ID3V2)
	mpg123_param(mh, MPG123_ADD_FLAGS, MPG123_SKIP_ID3V2, 0.);
#endif
	mpg123_param(mh, MPG123_ADD_FLAGS, MPG123_QUIET, 0.);
	if (mpg123_open(mh, (char *)src) != MPG123_OK) {
		ds_printf(0, "Error creating audio handle: %s!\n", mpg123_strerror(mh));
		mpg123_delete(mh);
		return NULL;
	}

	mpg123_format_none(mh);
	int channels = config.channels > 1 ? MPG123_STEREO:MPG123_MONO;
	const long * list;
	size_t num;
	mpg123_rates(&list, &num);
	for (size_t i=0; i < num; i++) {
		mpg123_format(mh, list[i], channels, MPG123_ENC_SIGNED_16);
	}

	int a,encoding;
	if (mpg123_getformat(mh, rate, &a, &encoding) != MPG123_OK || encoding != MPG123_ENC_SIGNED_16) {
		ds_printf(0, "mp3sync only supports 16-bit signed samples!\n");
		mpg123_close(mh);
		mpg123_delete(mh);
		return NULL;
	}
	ds_printf(2, "MP3 parameters: %d/%d/%d!\n", channels, rate, encoding);
	return mh;
}

lame_global_flags * enc_init(long rate) {
	lame_global_flags *gfp = lame_init();
	if (gfp == NULL) {
		ds_printf(0, "Error creating encoder handle!\n");
		return NULL;
	}
	//ds_printf(0, Using LAME %d.%d\n"), lvp.major, lvp.minor);

	//set what will be fed into LAME
	lame_set_num_channels(gfp,config.channels > 1 ? 2:1);
	lame_set_in_samplerate(gfp,rate);

	//and what it should be outputting
	switch(config.channels) {
		case 2:
			lame_set_mode(gfp,JOINT_STEREO);
			break;
		case 3:
			lame_set_mode(gfp,STEREO);
			break;
		default:
			lame_set_mode(gfp,MONO);
			break;
	}
	lame_set_out_samplerate(gfp,config.samplerate);
	//lame_set_bWriteVbrTag(gfp,0);
	lame_set_copyright(gfp, 1);
	//lame_set_scale(gfp,scale);
	lame_set_brate(gfp, config.bitrate);
	/*
 quality=0..9.  0=best (very slow).  9=worst.
  recommended:  2     near-best quality, not too slow
                5     good quality, fast
                7     ok quality, really fast
	*/
	lame_set_quality(gfp,4);

	/*
				lame_set_VBR(gfp, vbr_default);
				api->botapi->config->GetConfigSectionLong(sec, "VBR_Quality");
				if (tmp < 0 || tmp > 9) {
					tmp = 5;
				}
				lame_set_VBR_q(gfp, tmp);
				lame_set_VBR_mean_bitrate_kbps(gfp, api->GetOutputBitrate());
				tmp = api->botapi->config->GetConfigSectionLong(sec, "MinBitrate");
				if (tmp) {
					if (tmp > api->GetOutputBitrate()) { tmp = api->GetOutputBitrate(); }
					lame_set_VBR_min_bitrate_kbps(gfp, tmp);
				} else {
					lame_set_VBR_min_bitrate_kbps(gfp, int(api->GetOutputBitrate()*0.25));
				}
				tmp = api->botapi->config->GetConfigSectionLong(sec, "MaxBitrate");
				if (tmp) {
					if (tmp < api->GetOutputBitrate()) { tmp = api->GetOutputBitrate(); }
					lame_set_VBR_max_bitrate_kbps(gfp, tmp);
				} else {
					lame_set_VBR_max_bitrate_kbps(gfp, int(api->GetOutputBitrate()*1.25));
				}
	*/

	if (lame_init_params(gfp) == -1) {
		ds_printf(0, "lame_init_params() has returned ERROR!\n");
		lame_close(gfp);
		return NULL;
	}
	return gfp;
}

void show_pb(int64 pos, int64 size, int * last_x) {
	double per = pos/1024;
	per /= size/1024;
	if (per < 0) { per = 0; }
	if (per > 1) { per = 1; }
	int x = per * 77;
	if (x != *last_x) {
		*last_x = x;
		char buf[80];
		memset(buf, 0x20, sizeof(buf));
		buf[0] = '[';
		if (x > 0) {
			memset(&buf[1], '#', x);
		}
		buf[78] = ']';
		buf[79] = 0;
		fprintf(stderr, "\r%s", buf);
	}
}

bool copy_mp3(const char * src, const char * dest, int64 size) {

	long rate;
	mpg123_handle * mh = dec_init(src, &rate);
	if (mh == NULL) {
		return false;
	}
	//int read_size = mpg123_outblock(mh);

	lame_global_flags *gfp = enc_init(rate);
	int last_x = -1;
	bool ret = false;
	if (gfp != NULL) {

		FILE * fo = fopen(dest, "wb");
		if (fo != NULL) {
			short * buffer = (short *)zmalloc(MP3_BUFFER_SIZE);

			ret = true;
			while (1) {
				size_t done;
				int n = mpg123_read(mh, (unsigned char *)buffer, MP3_BUFFER_SIZE, &done);
				if (n == MPG123_NEW_FORMAT) {
					long rate;
					int b;
					int channels;
					mpg123_getformat(mh, &rate, &channels, &b);
					ds_printf(1, "\n");
					ds_printf(0, "Error: MP3 parameter change: %d channels, %dkHz samplerate!\n", channels, rate);
					ret = false;
					break;
				} else if (n == MPG123_OK) {
					int32 samples = (config.channels == 1) ? done/2:done/4;
					long bsize = (long)((1.25 * samples) + 7200);
					unsigned char * outbuf = (unsigned char *)zmalloc(bsize);
					long len=0;
					if (config.channels == 1) { // lame_encode_buffer_interleaved doesn't seem to be able to handle mono in any way, as an input or output
						len = lame_encode_buffer(gfp,(short *)buffer,(short *)buffer,samples,outbuf,bsize);
					} else {
						len = lame_encode_buffer_interleaved(gfp,(short *)buffer,samples,outbuf,bsize);
					}
					if (len > 0) {
						if (config.verbosity > 0) {
							show_pb(mpg123_tell_stream(mh), size, &last_x);
						}
						if (fwrite(outbuf, len, 1, fo) != 1) {
							int err = errno;
							ds_printf(1, "\n");
							ds_printf(0, "Error writing output file! (err: %s)\n", strerror(err));
							ret = false;
							zfree(outbuf);
							break;
						}
					}
					zfree(outbuf);
				} else if (n == MPG123_DONE || n == MPG123_NEED_MORE) {
					break;
				} else {
					ds_printf(1, "\n");
					ds_printf(0, "Decoder returned: %d (err: %s)\n", n, mpg123_strerror(mh));
					ret = false;
					break;
				}
			}

			zfree(buffer);
			if (ret) {
				unsigned char mp3buf[7200];
				int len = lame_encode_flush(gfp,mp3buf,sizeof(mp3buf));
				if (len > 0) {
					if (fwrite(mp3buf, len, 1, fo) == 1) {
						if (config.verbosity > 0) {
							show_pb(size, size, &last_x);
						}
					} else {
						int err = errno;
						ds_printf(1, "\n");
						ds_printf(0, "Error writing output file! (err: %s)\n", strerror(err));
						ret = false;
					}
				}
			}

			fclose(fo);
		}

		lame_close(gfp);
	}

	mpg123_close(mh);
	mpg123_delete(mh);
	return ret;
}
