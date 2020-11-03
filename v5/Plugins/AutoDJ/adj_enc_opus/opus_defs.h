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

typedef struct {
   int version;
   int channels; /* Number of channels: 1..255 */
   int preskip;
   ogg_uint32_t input_sample_rate;
   int gain; /* in dB S7.8 should be zero whenever possible */
   int channel_mapping;
   /* The rest is only used if channel_mapping != 0 */
   int nb_streams;
   int nb_coupled;
   unsigned char stream_map[255];
} OpusHeader;

int opus_header_parse(const unsigned char *header, int len, OpusHeader *h);
int opus_header_to_packet(const OpusHeader *h, unsigned char *packet, int len);

void comment_init(char **comments, int* length, const char *vendor_string);
void comment_add(char **comments, int* length, char *tag, char *val);
