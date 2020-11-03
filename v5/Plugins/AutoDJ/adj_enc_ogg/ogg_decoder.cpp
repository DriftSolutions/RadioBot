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

#include "ogg_codec.h"

bool ogg_is_my_file(const char * fn, const char * mime_type) {
	if (mime_type) {
		if (!stricmp(mime_type, "application/ogg") || !stricmp(mime_type, "audio/vorbis") || !stricmp(mime_type, "audio/ogg")) {
			return true;
		}
	}
	if (fn && strrchr(fn,'.') && !stricmp(strrchr(fn,'.'),".ogg")) {
		return true;
	}
	return false;
}

static int _fseek64_wrap_title_data(FILE *fp, ogg_int64_t off, int whence) {
	if(fp==NULL)return(-1);
	return fseek(fp,(long)off,whence);
}

bool ogg_get_title_data(const char * fn, TITLE_DATA * td, uint32 * songlen) {
	OggVorbis_File vf;
	memset(&vf,0,sizeof(vf));
	memset(td, 0, sizeof(TITLE_DATA));
	if (songlen) { *songlen=0; }

	FILE * fp = fopen(fn, "rb");
	if (!fp) {
		return false;
	}

	ov_callbacks callbacks = {
		(size_t (*)(void *, size_t, size_t, void *))  fread,
		(int (*)(void *, ogg_int64_t, int))           _fseek64_wrap_title_data,
		(int (*)(void *))                             fclose,
		(long (*)(void *))                            ftell
	};

	if(ov_test_callbacks(fp, &vf, NULL, 0, callbacks) < 0) {
		ov_clear(&vf);
		//fclose(fp);
		adapi->botapi->ib_printf(_("AutoDJ (ogg_get_title_data) -> Input does not appear to be an Ogg bitstream. (ov_test_callbacks())\n"));
		return false;
	}
	if(ov_test_open(&vf) < 0) {
		ov_clear(&vf);
		//fclose(fp);
		adapi->botapi->ib_printf(_("AutoDJ (ogg_get_title_data) -> Input does not appear to be an Ogg bitstream. (ov_test_open())\n"));
		return false;
	}

	//double songlentmp = ov_time_total(&vf, -1);
	//songlentmp /= 1000;
	if (songlen) {
		*songlen = ov_time_total(&vf, -1);// * 1000;
	}

	char **ptr=ov_comment(&vf,-1)->user_comments;
	//vorbis_info *vii=ov_info(&vf,-1);
	while(*ptr){
		char * p = strchr(*ptr,'=');
		if (p) {
			//printf("Meta: %s\n", *ptr);
			//p[0]=0;
			p++;
			if (!strnicmp(*ptr,"title=",6)) {
				sstrcpy(td->title,p);
			}
			if (!strnicmp(*ptr,"artist=",7)) {
				sstrcpy(td->artist,p);
			}
			if (!strnicmp(*ptr,"album=",6)) {
				sstrcpy(td->album,p);
			}
			if (!strnicmp(*ptr,"genre=",6) && !strlen(td->genre)) {
				sstrcpy(td->genre,p);
			}
			if (!strnicmp(*ptr,"url=",4) && !strlen(td->url)) {
				sstrcpy(td->url,p);
			}
			if (!strnicmp(*ptr,"DESCRIPTION=",12) && !strlen(td->comment)) {
				strlcpy(td->comment,p,sizeof(td->comment));
			}
			if (!strnicmp(*ptr,"TRACKNUMBER=",12) && td->track_no == 0) {
				td->track_no = atoi(p);
			}
			if (!strnicmp(*ptr,"year=",5) && td->year == 0) {
				td->year = atoi(p);
			}
		}
		++ptr;
    }

	ov_clear(&vf);
	//fclose(fp);
	return true;
}

static int wrap_fseek64(READER_HANDLE * fp, ogg_int64_t off, int whence) {
	if(fp==NULL || !fp->can_seek)return(-1);
	return (fp->seek(fp, long(off), whence) != -1) ? 0:-1;
}

static size_t wrap_read(void * ptr, size_t size, size_t nmemb, READER_HANDLE * fp) {
	if(fp==NULL)return(-1);
	int32 tmp = fp->read(ptr, size*nmemb, fp);
	if (tmp <= 0) { return tmp; }
	size_t ret = tmp / size;
	/*
	if (size != 1) {
		tmp = tmp;
	}
	*/
	/*
	if (fp->can_seek && (tmp%size) != 0) {
		tmp = tmp;
	}
	*/
	return ret;
}

static int wrap_close(void *) {return 0;} /// The handle must not be closed by the decoder plugin, so this is just a null function

class OGG_Decoder: public AutoDJ_Decoder {
private:
	READER_HANDLE * oggf;
	OggVorbis_File vf;
	//int32 song_len;
	int32 channels;
	int64 ogg_start;
	double ogg_cp;
public:
	virtual ~OGG_Decoder() {

	}

	virtual bool Open(READER_HANDLE * fnIn, int64 startpos) {
		memset(&vf,0,sizeof(vf));

		oggf = fnIn;
		channels = 0;
		ogg_start = 0;
		ogg_cp = 0.00f;

		ov_callbacks callbacks = {
			(size_t (*)(void *, size_t, size_t, void *))  wrap_read,
			(int (*)(void *, ogg_int64_t, int))           wrap_fseek64,
			(int (*)(void *))                             wrap_close,
			(long (*)(void *))                            fnIn->tell
		};

		if(ov_test_callbacks(oggf, &vf, NULL, 0, callbacks) < 0) {
			ov_clear(&vf);
			adapi->botapi->ib_printf(_("AutoDJ (ogg_decoder) -> Input does not appear to be an Ogg bitstream. (ov_test_callbacks())\n"));
			return false;
		}
		if(ov_test_open(&vf) < 0) {
			ov_clear(&vf);
			adapi->botapi->ib_printf(_("AutoDJ (ogg_decoder) -> Input does not appear to be an Ogg bitstream. (ov_test_open())\n"));
			return false;
		}

		vorbis_info *vii=ov_info(&vf,-1);
		song_length = ov_time_total(&vf, -1);// * 1000;
		ogg_cp=0.00f;

		adapi->botapi->ib_printf(_("AutoDJ (ogg_decoder) -> Bitstream is %d channel, %dHz\n"),vii->channels,vii->rate);
		adapi->botapi->ib_printf(_("AutoDJ (ogg_decoder) -> Decoded length: %ld samples\n"),(long)ov_pcm_total(&vf,-1));
		adapi->botapi->ib_printf(_("AutoDJ (ogg_decoder) -> Encoded by: %s\n"),ov_comment(&vf,-1)->vendor);
		char timebuf[32];
		adapi->PrettyTime(song_length/1000, timebuf);
		adapi->botapi->ib_printf(_("AutoDJ (ogg_decoder) -> Total playing time: %s\n"),timebuf);

		channels = vii->channels;
		ogg_start=time(NULL);
		return adapi->GetDeck(deck)->SetInAudioParams(channels,vii->rate);
	}

	virtual bool Open_URL(const char * url, int64 startpos) {
		return false;
	}

	virtual int64 GetPosition() {
		return fp ? fp->tell(fp):0;
	}

	virtual int32 Decode() {
		short buf[4096];
		int current_section = 0;

		long bread = ov_read(&vf, (char *)buf, sizeof(buf),0,2,1,&current_section);

		if (bread == OV_HOLE) {
			adapi->botapi->ib_printf(_("AutoDJ (ogg_decoder) -> OV_HOLE @ %f seconds\n"), ov_time_tell(&vf));
			return 0;
		}

		if (bread == OV_EBADLINK) { // corrupt file
			adapi->botapi->ib_printf(_("AutoDJ (ogg_decoder) -> OV_EBADLINK @ %f seconds\n"), ov_time_tell(&vf));
			return 0;
		}

		if (bread == 0) { // EOF
			return 0;
		}

		if (bread > 0) {
			int32 samples = (channels == 1) ? bread/2:bread/4;
			//int32 rsamples = bread/2;
			//buffer->realloc(buffer, rsamples);
			//memcpy(buffer->buf, buf, rsamples*sizeof(short));
			return adapi->GetDeck(deck)->AddSamples(buf, samples);
		}

		adapi->botapi->ib_printf(_("ogg_decode() -> nothing to do??? (bread: %d)\n"),bread);
		return 0;
	}

	virtual void Close() {
		ov_clear(&vf);
		if (oggf) {
			oggf=NULL;
		}
	}
};

AutoDJ_Decoder * ogg_create() { return new OGG_Decoder; }
void ogg_destroy(AutoDJ_Decoder * dec) { delete dynamic_cast<OGG_Decoder *>(dec); }

DECODER ogg_decoder = {
	1,
	ogg_is_my_file,
	ogg_get_title_data,
	ogg_create,
	ogg_destroy
};
