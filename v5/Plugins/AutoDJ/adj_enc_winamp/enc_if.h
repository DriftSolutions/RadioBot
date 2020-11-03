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

/*
** enc_if.h - common encoder interface
**
** Copyright (C) 2001-2003 Nullsoft, Inc.
**
** This software is provided 'as-is', without any express or implied warranty.
** In no event will the authors be held liable for any damages arising from the use of this software.
**
** Permission is granted to anyone to use this software for any purpose, including commercial
** applications, and to alter it and redistribute it freely, subject to the following restrictions:
**  1. The origin of this software must not be misrepresented; you must not claim that you wrote the
**     original software. If you use this software in a product, an acknowledgment in the product
**     documentation would be appreciated but is not required.
**  2. Altered source versions must be plainly marked as such, and must not be misrepresented as
**     being the original software.
**  3. This notice may not be removed or altered from any source distribution.
*/

#ifndef _NSV_ENC_IF_H_
#define _NSV_ENC_IF_H_
class VideoCoder
{
  public:
    VideoCoder() { }
    virtual int Encode(void *in, void *out, int *iskf)=0; // returns bytes in out
    virtual ~VideoCoder() { };
};

class AudioCoder
{
  public:
    AudioCoder() { }
    virtual int Encode(int framepos, void *in, int in_avail, int *in_used, void *out, int out_avail)=0; //returns bytes in out
    virtual ~AudioCoder() { };
};

// unsigned int GetAudioTypes3(int idx, char *desc);
// unsigned int GetVideoTypes3(int idx, char *desc);
// AudioCoder *CreateAudio3(int nch, int srate, int bps, unsigned int srct, unsigned int *outt, char *configfile);
// VideoCoder *CreateVideo3(int w, int h, double frt, unsigned int pixt, unsigned int *outt, char *configfile);
// HWND ConfigAudio3(HWND hwndParent, HINSTANCE hinst, unsigned int outt, char *configfile);
// HWND ConfigVideo3(HWND hwndParent, HINSTANCE hinst, unsigned int outt, char *configfile);

#endif //_NSV_ENC_IF_H_
