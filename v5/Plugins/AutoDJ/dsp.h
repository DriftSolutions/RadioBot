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

/***

	File Name:   dsp.h
	Description: AutoDJ DSP plugin interface
	License:     GPL
	Website:     www.shoutirc.com

	Copyright 2008-2012 Indy Sams/Drift Solutions

***/

#ifndef __AUTODJ_DSP_H__
#define __AUTODJ_DSP_H__

#if defined(WIN32)
#include <winsock2.h>
#include <windows.h>
#endif

#ifndef __cplusplus
typedef unsigned char bool;
#endif
#ifndef TRUE
#define TRUE 1
#endif
#ifndef FALSE
#define FALSE 0
#endif

#define DSP_VERSION 0x01

typedef struct {
	unsigned char version;//set to DSP_VERSION
	char * description;	// plugin description
#if defined(WIN32)
	HINSTANCE hInstance;// handle to your DLL (filled in by AutoDJ, from LoadLibrary)
#else
	void * hInstance;// handle to your DLL (filled in by AutoDJ, from dlopen)
#endif

	bool (*init)(int channels, int samplerate); //called when the plugin is loaded, return false for failure / to cancel plugin load.
	int (*modify_samples)(int numsamples, short * samples, int channels, int samplerate);//this will match the channels/samplerate that were in init(), if they change quit/init will always be called. Return value is the # of samples returned in the array. Currently plugins may not return more samples than went in because that could cause a buffer overrun.
	void (*quit)(); //called on unload if init() succeeded.

} AUTODJ_DSP;

// exported symbols
typedef AUTODJ_DSP * (*GetAD_DSP_Type)();
//export function as GetAD_DSP();

#endif
