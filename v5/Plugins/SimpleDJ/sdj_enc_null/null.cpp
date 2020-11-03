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

#include "../adj_plugin.h"

SD_PLUGIN_API * api=NULL;

class NULL_Encoder: public AutoDJ_Encoder {
private:
	int server;
	int inChannels, inSample;
public:
	NULL_Encoder();
	~NULL_Encoder();
	virtual bool Init(int32 num, int32 channels, int32 samplerate, float scale);
	virtual bool Encode(int32 num_samples, const short buffer[]);
	virtual bool Raw(unsigned char * data, int32 len);
	virtual void Close();
};

NULL_Encoder::NULL_Encoder() {
	inChannels = inSample = 0;
	server = -1;
}

NULL_Encoder::~NULL_Encoder() {
}

bool NULL_Encoder::Init(int num, int channels, int samplerate, float scale) {
	server = num;
	inChannels = channels;
	inSample = samplerate;
	return true;
}

bool NULL_Encoder::Raw(unsigned char * data, int32 len) {
	return api->GetFeeder(server)->Send(data, len);
}

bool NULL_Encoder::Encode(int32 num_samples, const short buffer[]) {
	return api->GetFeeder(server)->Send((unsigned char *)buffer, num_samples*sizeof(short));
}

void NULL_Encoder::Close() {
}

AutoDJ_Encoder * null_enc_create() { return new NULL_Encoder; }
void null_enc_destroy(AutoDJ_Encoder * enc) {
	delete dynamic_cast<NULL_Encoder *>(enc);
}

ENCODER null_encoder = {
	"null",
	"application/data",

	null_enc_create,
	null_enc_destroy
};

bool init(SD_PLUGIN_API * pApi) {
	api = pApi;
	api->RegisterEncoder(&null_encoder);
	return true;
};

void quit() {
};

SD_PLUGIN plugin = {
	SD_PLUGIN_VERSION,
	"NULL Encoder",
	init,
	NULL,
	quit
};

PLUGIN_EXPORT SD_PLUGIN * GetSDPlugin() { return &plugin; }
