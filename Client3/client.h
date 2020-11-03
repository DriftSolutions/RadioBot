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

#include "../Common/libremote/libremote.h"
#include "../v5/src/user_flags.h"

// For compilers that support precompilation, includes "wx/wx.h".
#include <wx/wxprec.h>
#ifndef WX_PRECOMP
#include <wx/wx.h>
#endif
#include <wx/aboutdlg.h>
#include <wx/intl.h>
#include <wx/xrc/xmlres.h>


#if defined(WIN32)
#include "resource.h"
#endif
#include "client_wdr.h"
#include "libdebug.h"

#define MAX_CONNECTIONS 32
#define VERSION "3.0.4"

#if (wxMAJOR_VERSION < 2) || (wxMAJOR_VERSION == 2 && wxMINOR_VERSION < 8)
#define SetInitialSize SetBestFittingSize
#endif


#undef WX_OLD
#if (wxMAJOR_VERSION < 2 || (wxMAJOR_VERSION == 2 && wxMINOR_VERSION < 9))
#define WX_OLD 1
#endif

typedef struct {
	char name[128];
	char host[128];
	uint16 port, mode;
	char username[64];
	char pass[64];
} CONFIG_HOSTS;

typedef struct {
	char revision;
	int index;
	T_SOCKET * sock;
	CONFIG_HOSTS hosts[MAX_CONNECTIONS];
	bool beep_on_req;
	bool keep_on_top;
} CONFIG;

extern CONFIG config;
extern RemoteClient rc;

wxDialog * GetMainDlg();
int debug_printf(char * fmt, ...);
void SchedConnect(int newindex);
void DoConnect(int newindex);
bool IsConnected();
void DoDisconnect();

bool LoadConfig();
void SaveConfig();
wxBitmap StatusPics( size_t index );

bool OpenConnManager(wxDialog * parent);
bool OpenAboutBox(wxDialog * parent);
bool OpenOptions(wxDialog * parent);

void CL3_InitXmlResource();

/* Old Stuff */

typedef struct {
	char host[256];
	char username[256];
	char pass[256];
	int port;
	SOCKET sock;
} CONFIG_OLD3;

typedef struct {
	char host[128];
	int port;
} CONFIG_HOSTS_OLD2;

typedef struct {
	char revision;
	char username[64];
	char pass[64];
	int index;
	T_SOCKET * sock;
	CONFIG_HOSTS_OLD2 hosts[16];
	bool ssl;
} CONFIG_OLD2;

typedef struct {
	char revision;
	int index;
	T_SOCKET * sock;
	CONFIG_HOSTS hosts[MAX_CONNECTIONS];
} CONFIG_OLD;
