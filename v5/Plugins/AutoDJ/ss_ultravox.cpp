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
#include <stdint.h>
#include <assert.h>
#include <iostream>
#include <iomanip>

#ifndef TIXML_USE_STL
#define TIXML_USE_STL
#endif
#include "../../../Common/tinyxml/tinyxml.h"

#pragma pack(1)
struct UVOX_HEADER {
	uint8 sync;
	uint8 qos;
	uint16 type;
	uint16 length;
};
#pragma pack()

static string XTEA_encipher(const uint8* c_data,size_t c_data_cnt, const uint8* c_key,size_t c_key_cnt) throw();
//static string XTEA_decipher(const uint8* c_data,size_t c_data_cnt, const uint8* c_key,size_t c_key_cnt) throw();

bool SendUVoxPacket(T_SOCKET * sock, uint16 type, uint16 length, const uint8 * data=NULL) {
	uint8 * ptr = (uint8 *)zmalloc(sizeof(UVOX_HEADER)+length+1);
	UVOX_HEADER * h = (UVOX_HEADER *)ptr;
	memset(ptr, 0, sizeof(UVOX_HEADER)+length+1);
	h->sync = 0x5A;
	h->type = Get_UBE16(type);
	h->length = Get_UBE16(length);
	if (length && data) {
		memcpy(ptr+sizeof(UVOX_HEADER), data, length);
	}
	bool ret = (ad_config.sockets->Send(sock, (char *)ptr, sizeof(UVOX_HEADER)+length+1) == sizeof(UVOX_HEADER)+length+1) ? true:false;
	zfree(ptr);
	return ret;
}
inline void FreeUVoxPacket(uint8 * ptr) { zfree(ptr); }

bool RecvUVoxPacket(T_SOCKET * sock, UVOX_HEADER * h, const uint8 * data, uint16 bufSize) {
	if (ad_config.sockets->Recv(sock, (char *)h, sizeof(UVOX_HEADER)) == sizeof(UVOX_HEADER)) {
		h->length = Get_UBE16(h->length);
		h->type = Get_UBE16(h->type);
		int left = h->length + 1;
		while (left) {
			int n = ad_config.sockets->Recv(sock, (char *)data, left);
			if (n > 0) {
				left -= n;
				data += n;
			} else {
				return false;
			}
		}
		return true;
	}
	return false;
}

void DumpUVoxPacket(UVOX_HEADER * h, const uint8 * data) {
	api->ib_printf2(pluginnum,"UVox Packet -> Sync: %02X, QOS: %02X, Type: %04X, Length: %08X (%u)\n", h->sync, h->qos, h->type, h->length, h->length);
	PrintData(stdout, data, h->length <= 64 ? h->length:64);
}

bool SendRecvUVoxPacket(T_SOCKET * sock, uint16 type, uint16 length, const uint8 * data, UVOX_HEADER * h, const uint8 * buf, uint16 bufSize, int expectedReply) {
	if (!SendUVoxPacket(sock, type, length, data)) {
		api->ib_printf2(pluginnum,"AutoDJ (ss_shoutcast2) -> Error sending packet to server (%04X)!\n", type);
		return false;
	}
	if (!RecvUVoxPacket(sock, h, buf, bufSize)) {
		api->ib_printf2(pluginnum,"AutoDJ (ss_shoutcast2) -> Error receiving reply from server (%04X)!\n", type);
		return false;
	}
	if (h->type != expectedReply) {
		api->ib_printf2(pluginnum,"AutoDJ (ss_shoutcast2) -> Unexpected reply from server!\n", buf);
		DumpUVoxPacket(h, (uint8 *)buf);
		return false;
	}
	if (strnicmp((const char *)buf, "ACK", 3)) {
		api->ib_printf2(pluginnum,"AutoDJ (ss_shoutcast2) -> Received negative reply from server (%04X): %s!\n", h->type, buf);
		return false;
	}
	return true;
}

class Ultravox_Feeder: public AutoDJ_Feeder {
private:
	T_SOCKET * sock;
	uint16 payload_type;
	Titus_Mutex sockMutex;
	uint16 metadata_id;

	TITLE_DATA last_meta;
	string last_meta_string;
	bool last_was_error;
public:
	Ultravox_Feeder();
	~Ultravox_Feeder();
	virtual bool Connect();
	virtual bool IsConnected();
	virtual bool Send(unsigned char * buffer, int32 len);
	virtual bool SendTitleUpdate(const char * title, TITLE_DATA * td);
	virtual void Close();
};

Ultravox_Feeder::Ultravox_Feeder() {
	sock = NULL;
	metadata_id = 1;
	payload_type = 0;
	memset(&last_meta, 0, sizeof(last_meta));
	last_was_error = false;
}
Ultravox_Feeder::~Ultravox_Feeder() {
	if (sock) {
		Close();
	}
}

bool Ultravox_Feeder::IsConnected() {
	return sock ? true:false;
}

#define UVOX_VERSION "2.1"

bool Ultravox_Feeder::Connect() {
	char buf[1024];
	API_SS sc;
	api->ss->GetSSInfo(0,&sc);
	metadata_id = 1;
	//printf("AutoDJ -> First SC: %s:%d\n",sc.host,sc.port);
	sock = ad_config.sockets->Create();
	sockMutex.Lock();
	if (ad_config.sockets->ConnectWithTimeout(sock,ad_config.Server.SourceIP,ad_config.Server.SourcePort ? ad_config.Server.SourcePort:sc.port,10000)) {
		ad_config.sockets->SetSendTimeout(sock, 10000);
		ad_config.sockets->SetRecvTimeout(sock, 10000);

		SendUVoxPacket(sock, 0x1009, strlen(UVOX_VERSION), (const uint8 *)UVOX_VERSION);
		UVOX_HEADER h;
		char ckey[24];
		if (RecvUVoxPacket(sock, &h, (uint8 *)buf, sizeof(buf))) {
			if (h.type == 0x1009) {
				if (!strnicmp(buf, "ACK:", 4)) {
					strcpy(ckey, &buf[4]);
				} else {
					api->ib_printf2(pluginnum,"AutoDJ (ss_shoutcast2) -> Error in cipher negotiation: %s!\n", buf);
					goto onerror;
				}
			} else {
				api->ib_printf2(pluginnum,"AutoDJ (ss_shoutcast2) -> Unknown reply from server!\n", buf);
				DumpUVoxPacket(&h, (uint8 *)buf);
				goto onerror;
			}
		} else {
			api->ib_printf2(pluginnum,"AutoDJ (ss_shoutcast2) -> Error receiving reply from server (1009)!\n", buf);
			DumpUVoxPacket(&h, (uint8 *)buf);
			goto onerror;
		}

		string user,pass;
		StrTokenizer st(ad_config.Server.Pass, ':');
		if (st.NumTok() == 2) {
			user = XTEA_encipher((uint8 *)st.stdGetSingleTok(1).c_str(), st.stdGetSingleTok(1).length(), (uint8 *)ckey, strlen(ckey));
			pass = XTEA_encipher((uint8 *)st.stdGetSingleTok(2).c_str(), st.stdGetSingleTok(2).length(), (uint8 *)ckey, strlen(ckey));
		} else {
			pass = XTEA_encipher((uint8 *)st.stdGetSingleTok(1).c_str(), st.stdGetSingleTok(1).length(), (uint8 *)ckey, strlen(ckey));
		}
		sprintf(buf, "" UVOX_VERSION ":%d:%s:%s", sc.streamid, user.c_str(), pass.c_str());
		if (!SendRecvUVoxPacket(sock, 0x1001, strlen(buf), (uint8 *)buf, &h, (uint8 *)buf, sizeof(buf), 0x1001)) {
			goto onerror;
		}

		sstrcpy(buf, ad_config.Server.MimeType);
		//str_replace(buf, sizeof(buf), "application/", "audio/");
		//api->ib_printf2(pluginnum,"AutoDJ (ss_shoutcast2) -> Mime Type: %s\n", buf);
		if (!SendRecvUVoxPacket(sock, 0x1040, strlen(buf), (uint8 *)buf, &h, (uint8 *)buf, sizeof(buf), 0x1040)) {
			goto onerror;
		}

		//sprintf(buf, "%d:320", ad_config.Server.Bitrate);
		sprintf(buf, "%d:%d", ad_config.Server.Bitrate*1000, ad_config.Server.Bitrate*1000);
		if (!SendRecvUVoxPacket(sock, 0x1002, strlen(buf), (uint8 *)buf, &h, (uint8 *)buf, sizeof(buf), 0x1002)) {
			goto onerror;
		}

		sprintf(buf, "2048:256");
		if (!SendRecvUVoxPacket(sock, 0x1003, strlen(buf), (uint8 *)buf, &h, (uint8 *)buf, sizeof(buf), 0x1003)) {
			goto onerror;
		}

		if (!SendRecvUVoxPacket(sock, 0x1100, strlen(ad_config.Server.Name), (uint8 *)ad_config.Server.Name, &h, (uint8 *)buf, sizeof(buf), 0x1100)) {
			goto onerror;
		}
		if (!SendRecvUVoxPacket(sock, 0x1101, strlen(ad_config.Server.Genre), (uint8 *)ad_config.Server.Genre, &h, (uint8 *)buf, sizeof(buf), 0x1101)) {
			goto onerror;
		}
		if (!SendRecvUVoxPacket(sock, 0x1102, strlen(ad_config.Server.URL), (uint8 *)ad_config.Server.URL, &h, (uint8 *)buf, sizeof(buf), 0x1102)) {
			goto onerror;
		}
		sprintf(buf, "%s", ad_config.Server.Public ? "1":"0");
		if (!SendRecvUVoxPacket(sock, 0x1103, strlen(buf), (uint8 *)buf, &h, (uint8 *)buf, sizeof(buf), 0x1103)) {
			goto onerror;
		}

		//flush cached metadata
		if (!SendRecvUVoxPacket(sock, 0x1006, 0, NULL, &h, (uint8 *)buf, sizeof(buf), 0x1006)) {
			goto onerror;
		}


	/*
	0x4 	0x0xx 	Station logo (see 'Image Notes' for xx details)
When these are received, the xx of the type is used to specify the image mime type where:
        00 	image/jpeg
        01 	image/png
        02 	image/bmp
        03 	image/gif
	*/
		//server to standby
		if (!SendRecvUVoxPacket(sock, 0x1004, 0, NULL, &h, (uint8 *)buf, sizeof(buf), 0x1004)) {
			goto onerror;
		}

		/*
		FILE * fp = fopen("sclogo.png", "rb");
		if (fp) {
			fseek(fp, 0, SEEK_END);
			long len = ftell(fp);
			fseek(fp, 0, SEEK_SET);
			char * data = (char *)zmalloc(len);
			fread(data, len, 1, fp);
			fclose(fp);
			if (len <= 65535) {
				if (!SendUVoxPacket(sock, 0x4001, len, (uint8 *)data)) {
					zfree(data);
					goto onerror;
				}
				if (!SendUVoxPacket(sock, 0x4101, len, (uint8 *)data)) {
					zfree(data);
					goto onerror;
				}
			} else {
				api->ib_printf2(pluginnum,"AutoDJ (ss_shoutcast2) -> Station logo is too big! Max size: 64K (65535 bytes)\n");
			}
			zfree(data);
		} else {
			api->ib_printf2(pluginnum,"AutoDJ (ss_shoutcast2) -> Error opening station logo for read: %s\n", "sclogo.png");
		}
		*/

		if (!stricmp(ad_config.Server.MimeType, "audio/mpeg")) {
			payload_type = 0x7000;
		} else if (!stricmp(ad_config.Server.MimeType, "audio/vlb")) {
			payload_type = 0x8000;
		} else if (!stricmp(ad_config.Server.MimeType, "audio/aac")) {
			payload_type = 0x8001;
		} else if (!stricmp(ad_config.Server.MimeType, "audio/aacp")) {
			payload_type = 0x8003;
		} else if (!stricmp(ad_config.Server.MimeType, "application/ogg")) {
			payload_type = 0x8004;
		} else {
			api->ib_printf2(pluginnum,"AutoDJ (ss_shoutcast2) -> Unkown mime type, using default MP3 message type 0x7000\n");
			payload_type = 0x7000;
		}
		api->ib_printf2(pluginnum,"AutoDJ -> Connected as source on stream id %d ...\n", sc.streamid);
		/*
		sprintf(buf,"icy-description:%s\r\n",ad_config.Server.Desc);
		ad_config.sockets->Send(sock,buf);
		sprintf(buf,"icy-icq:%s\r\n",ad_config.Server.ICQ);
		ad_config.sockets->Send(sock,buf);
		sprintf(buf,"icy-irc:%s\r\n",ad_config.Server.IRC);
		ad_config.sockets->Send(sock,buf);
		sprintf(buf,"icy-aim:%s\r\n",ad_config.Server.AIM);
		ad_config.sockets->Send(sock,buf);
		ad_config.sockets->Send(sock,"User-Agent: RadioBot AutoDJ\r\n");
		*/

		if (last_was_error) {
			if (last_meta_string.length() > 0) {
				//SendTitleUpdate(last_meta_string.c_str(), last_meta.title[0] ? &last_meta:NULL);
			}
			last_was_error = false;
		}


		DRIFT_DIGITAL_SIGNATURE();
	} else {
		api->ib_printf2(pluginnum,"AutoDJ (ss_shoutcast2) -> Error connecting to SHOUTCast: connection attempt timed out (is the server running?)\n");
		goto onerror;
	}
	sockMutex.Release();
	return true;
onerror:
	if (sock) { ad_config.sockets->Close(sock); }
	sock = NULL;
	sockMutex.Release();
	return false;
}

bool Ultravox_Feeder::Send(unsigned char * buffer, int32 left) {
	if (sock == NULL) { return false; }
	if (left == 0) { return true; }

	sockMutex.Lock();
	while (left) {
		int toSend = (left > 16376) ? 16376:left;
		if (!SendUVoxPacket(sock, payload_type, toSend, buffer)) {
			api->ib_printf2(pluginnum,"AutoDJ (ss_shoutcast2) -> Socket error in feeder! (errno: %d, desc: %s)\n",ad_config.sockets->GetLastError(),ad_config.sockets->GetLastErrorString());
			ad_config.sockets->Close(sock);
			last_was_error = true;
			sock = NULL;
			sockMutex.Release();
			return false;
		}
		left -= toSend;
		buffer += toSend;
	}
	if (ad_config.sockets->Select_Read(sock, uint32(0))) {
		UVOX_HEADER h;
		uint8 buf[4096];
		if (RecvUVoxPacket(sock, &h, buf, sizeof(buf))) {
			DumpUVoxPacket(&h, buf);
		}
	}
	sockMutex.Release();

	/*
	int sret = ad_config.sockets->Send(sock,(char *)buffer,len);
	if (sret == -1) {
		api->ib_printf2(pluginnum,"AutoDJ (ss_shoutcast2) -> Socket error in feeder! (errno: %d, desc: %s)\n",ad_config.sockets->GetLastError(),ad_config.sockets->GetLastErrorString());
		ad_config.sockets->Close(sock);
		sock = NULL;
		return false;
	}
	if (sret == 0) {
		api->ib_printf2(pluginnum,"AutoDJ (ss_shoutcast2) -> Server closed socket! (errno: %d, desc: %s)\n",ad_config.sockets->GetLastError(),ad_config.sockets->GetLastErrorString());
		ad_config.sockets->Close(sock);
		sock = NULL;
		return false;
	}
	*/
	return true;
}

bool Ultravox_Feeder::SendTitleUpdate(const char * title, TITLE_DATA * td) {
	if (td == NULL) { return false; }
	/*
	0x4 	0x0xx 	Station logo (see 'Image Notes' for xx details)
0x4 	0x1xx 	Album art (see 'Image Notes' for xx details)
When these are received, the xx of the type is used to specify the image mime type where:

        00 	image/jpeg
        01 	image/png
        02 	image/bmp
        03 	image/gif
	*/

	TiXmlDocument * doc = new TiXmlDocument();
	//TiXmlDeclaration * decl = new TiXmlDeclaration("1.0", "UTF-8", "yes");
	//doc->LinkEndChild(decl);
	TiXmlElement * root = new TiXmlElement("metadata");
	TiXmlElement * itm = new TiXmlElement("TIT2");
	TiXmlText * txt = new TiXmlText(td->title);
	txt->SetCDATA(true);
	itm->LinkEndChild(txt);
	root->LinkEndChild(itm);
	itm = new TiXmlElement("TPE1");
	itm->LinkEndChild(new TiXmlText(strlen(td->artist) ? td->artist:""));
	root->LinkEndChild(itm);
	itm = new TiXmlElement("TALB");
	itm->LinkEndChild(new TiXmlText(strlen(td->album) ? td->album:""));
	root->LinkEndChild(itm);
	itm = new TiXmlElement("TCON");
	itm->LinkEndChild(new TiXmlText(strlen(td->genre) ? td->genre:""));
	root->LinkEndChild(itm);
	itm = new TiXmlElement("TURL");
	itm->LinkEndChild(new TiXmlText(strlen(td->url) ? td->url:""));
	root->LinkEndChild(itm);
	itm = new TiXmlElement("TENC");
	itm->LinkEndChild(new TiXmlText("ShoutIRC Auto DJ Bot - www.shoutirc.com"));
	root->LinkEndChild(itm);
	itm = new TiXmlElement("TRSN");
	itm->LinkEndChild(new TiXmlText(ad_config.Server.Name));
	root->LinkEndChild(itm);
	itm = new TiXmlElement("WORS");
	itm->LinkEndChild(new TiXmlText(ad_config.Server.URL));
	root->LinkEndChild(itm);
	/*
	itm = new TiXmlElement("TFLT");
	itm->LinkEndChild(new TiXmlText("aacp"));
	root->LinkEndChild(itm);
	*/

	TiXmlElement * ext = new TiXmlElement("extension");
	itm = new TiXmlElement("title");
	itm->SetAttribute("seq", 1);
	itm->LinkEndChild(new TiXmlText(td->title));
	ext->LinkEndChild(itm);
	if (td->artist[0]) {
		itm = new TiXmlElement("title");
		itm->SetAttribute("seq", 2);
		itm->LinkEndChild(new TiXmlText(td->artist));
		ext->LinkEndChild(itm);
	}
	root->LinkEndChild(ext);

	doc->LinkEndChild(root);

	TiXmlPrinter printer;
	printer.SetStreamPrinting();
	//printer.SetLineBreak("\n");
	doc->Accept(&printer);
	std::string str = printer.CStr();
	str = "<?xml version=\"1.0\" encoding=\"UTF-8\" ?>\n";
	str += printer.CStr();
#if defined(DEBUG)
	api->ib_printf2(pluginnum,"XML: %s\n", str.c_str());
#endif

	DRIFT_DIGITAL_SIGNATURE();

	stringstream sstr;
	int len = str.length() + 6;
	char * data = new char[len+1];
	uint16 x = Get_UBE16(1);
	strcpy(data+6, str.c_str());
	*((uint16 *)&data[0]) = Get_UBE16(metadata_id++);//metadata_id
	*((uint16 *)&data[2]) = x;
	*((uint16 *)&data[4]) = x;

	bool ret = true;
	sockMutex.Lock();
	if (sock != NULL) {
		ret = SendUVoxPacket(sock, 0x3902, len, (uint8 *)data);
	}
	//PrintData(stdout, data, len);
	//SendRecvUVoxPacket(sock, 0x3902, str.length(), (uint8 *)str.c_str(), &h, (uint8 *)buf, sizeof(buf), 0x3902);
	//DumpUVoxPacket(&h, (uint8 *)buf);
	sockMutex.Release();
	delete [] data;
	delete doc;

	last_meta_string = title;
	if (td && td->title[0]) {
		memcpy(&last_meta, td, sizeof(last_meta));
	} else {
		memset(&last_meta, 0, sizeof(last_meta));
	}

	/*
	FILE * fp = fopen("sclogo.png", "rb");
	if (fp) {
		fseek(fp, 0, SEEK_END);
		long len = ftell(fp);
		fseek(fp, 0, SEEK_SET);
		char * data = (char *)zmalloc(len);
		fread(data, len, 1, fp);
		fclose(fp);
		if (len <= 65535) {
			SendUVoxPacket(sock, 0x4001, len, (uint8 *)data);
		} else {
			api->ib_printf2(pluginnum,"AutoDJ (ss_shoutcast2) -> Station logo is too big! Max size: 64K (65535 bytes)\n");
		}
		zfree(data);
	} else {
		api->ib_printf2(pluginnum,"AutoDJ (ss_shoutcast2) -> Error opening station logo for read: %s\n", "sclogo.png");
	}
	*/
	return ret;
}

void Ultravox_Feeder::Close() {
	if (sock != NULL) {
		//send stream termination signal
		SendUVoxPacket(sock, 0x1005, 0);
		ad_config.sockets->Close(sock);
		sock = NULL;
	}
}

AutoDJ_Feeder * ultravox_create() { return new Ultravox_Feeder; }
void ultravox_destroy(AutoDJ_Feeder * feeder) {
	delete dynamic_cast<Ultravox_Feeder *>(feeder);
}


// from wikipedia. Slightly modified to be 32/64 bit clean
static void XTEA_encipher(uint32* v, uint32* k,unsigned int num_rounds = 32)
{
  uint32 v0=v[0], v1=v[1], i;
  uint32 sum=0, delta=0x9E3779B9;
  for(i=0; i<num_rounds; i++)
  {
    v0 += (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + k[sum & 3]);
    sum += delta;
    v1 += (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + k[(sum>>11) & 3]);
  }
  v[0]=v0; v[1]=v1;
}

/*
static void XTEA_decipher(uint32* v, uint32* k,unsigned int num_rounds = 32)
{
  uint32 v0=v[0], v1=v[1], i;
  uint32 delta=0x9E3779B9, sum=delta*num_rounds;

  for(i=0; i<num_rounds; i++)
  {
    v1 -= (((v0 << 4) ^ (v0 >> 5)) + v0) ^ (sum + k[(sum>>11) & 3]);
    sum -= delta;
    v0 -= (((v1 << 4) ^ (v1 >> 5)) + v1) ^ (sum + k[sum & 3]);
  }
  v[0]=v0; v[1]=v1;
}
*/

static uint32 fourCharsToLong(uint8 *s)
{
  uint32 l = 0;
  l |= s[0]; l <<= 8;
  l |= s[1]; l <<= 8;
  l |= s[2]; l <<= 8;
  l |= s[3];
  return l;
}

/*
static void longToFourChars(uint32 l,uint8 *r)
{
  r[3] = l & 0xff; l >>= 8;
  r[2] = l & 0xff; l >>= 8;
  r[1] = l & 0xff; l >>= 8;
  r[0] = l & 0xff; l >>= 8;
}
*/

#define XTEA_KEY_PAD 0
#define XTEA_DATA_PAD 0

static string XTEA_encipher(const uint8* c_data,size_t c_data_cnt, const uint8* c_key,size_t c_key_cnt) throw()
{
  ostringstream oss;

  vector<uint8> key(c_key,c_key + c_key_cnt);
  vector<uint8> data(c_data,c_data + c_data_cnt);

  // key is always 128 bits
  while(key.size() < 16) key.push_back(XTEA_KEY_PAD); // pad key with zero
  uint32 k[4] = { fourCharsToLong(&key[0]),fourCharsToLong(&key[4]),
                    fourCharsToLong(&key[8]),fourCharsToLong(&key[12])
  };

  // data is multiple of 64 bits
  size_t siz = data.size();
  while(siz % 8) { siz+=1; data.push_back(XTEA_DATA_PAD);} // pad data with zero

  for(size_t x = 0; x < siz; x+=8)
  {
    uint32 v[2];
    v[0] = fourCharsToLong(&data[x]);
    v[1] = fourCharsToLong(&data[x+4]);
    XTEA_encipher(v,k);
    oss << setw(8) << setfill('0') << hex << v[0];
    oss << setw(8) << setfill('0') << hex << v[1]; // hex values.
    // uvox uses colon as seperator so we can't use chars for fear of collision
  }
  return oss.str();
}

/*
static string XTEA_decipher(const uint8* c_data,size_t c_data_cnt, const uint8* c_key,size_t c_key_cnt) throw()
{
  string result;

  vector<uint8> key(c_key,c_key + c_key_cnt);
  vector<uint8> data(c_data,c_data + c_data_cnt);

  // key is always 128 bits
  while(key.size() < 16) key.push_back(XTEA_KEY_PAD); // pad key with zero
  uint32 k[4] = { fourCharsToLong(&key[0]),fourCharsToLong(&key[4]),
                    fourCharsToLong(&key[8]),fourCharsToLong(&key[12])
  };

  // data is multiple of 16 hex digits
  size_t siz = data.size();
  assert(!(siz % 16)); // should never happen if data is good
  while(siz % 16) { siz+=1; data.push_back('0');} // pad data with zero

  for(size_t x = 0; x < siz; x+=16)
  {
    uint32 v[2];

    sscanf((const char *)&data[x],"%8x",&v[0]);
    sscanf((const char *)&data[x+8],"%8x",&v[1]);
    XTEA_decipher(v,k);
    uint8 ur[5] = {0,0,0,0,0 };
    longToFourChars(v[0],ur);
    result += (char)ur;
    longToFourChars(v[1],ur);
    result += (char)ur;
  }
  return result;
}
*/
