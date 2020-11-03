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

#include "../../v4/src/plugins.h"

class PLUGIN {
	public:
		DL_HANDLE hHandle;
		PLUGIN_PUBLIC * plug;
};
typedef std::vector<PLUGIN> pluginList;
pluginList plugins;

typedef PLUGIN_PUBLIC * (*GetPluginType)();

bool LoadPlugin(const char * fn) {
	GetPluginType GetPtr;

	PLUGIN_PUBLIC * ret = NULL;
	PLUGIN Scan;

	printf("RadioBot: Attempting to load plugin %s ...\n",fn);

	DL_HANDLE hHandle = DL_Open(fn);
	if (hHandle != NULL) {
		GetPtr = (GetPluginType)DL_GetAddress(hHandle,"IRCBotPlugin");
#ifndef WIN32
		if (GetPtr == NULL) {
			GetPtr = (GetPluginType)DL_GetAddress(hHandle, "_IRCBotPlugin");
		}
		if (GetPtr == NULL) {
			GetPtr = (GetPluginType)DL_GetAddress(hHandle, "_Z12IRCBotPluginv");
		}
		if (GetPtr == NULL) {
			GetPtr = (GetPluginType)DL_GetAddress(hHandle, "IRCBotPlugin");
		}
#endif
		if (GetPtr != NULL) {
			Scan.hHandle = hHandle;
			Scan.plug = GetPtr();
			if (Scan.plug == NULL || Scan.plug->plugin_ver != IRCBOT_PLUGIN_VERSION) {
				printf("RadioBot: Error loading plugin: This plugin is not compatible with this version of RadioBot!\n");
				DL_Close(hHandle);
				Scan.hHandle = NULL;
				Scan.plug = NULL;
				return false;
			}
			Scan.plug->hInstance = hHandle;
			plugins.push_back(Scan);
			return true;
		} else {
			printf("RadioBot: Error loading plugin: Failed in DL_GetAddress()\nError: %s\n", DL_LastError());
			//load failed
			DL_Close(hHandle);
			return false;
		}
	} else {
		printf("RadioBot: Error loading plugin: Failed in DL_Open()\nError: %s\n", DL_LastError());
		return false;
	}
}

void UnloadPlugins() {
	for (pluginList::iterator x = plugins.begin(); x != plugins.end(); x++) {
		DL_Close(x->hHandle);
	}
	plugins.clear();
}

void print_guids(PLUGIN_PUBLIC * Scan) {
	printf("%s -> %s\n", Scan->plugin_name_short, Scan->guid);
}

void print_info(PLUGIN_PUBLIC * Scan) {
	printf("%s -> %s\n", Scan->plugin_name_short, Scan->plugin_author);
}

typedef void (*fe_callback)(PLUGIN_PUBLIC * Scan);
void ForEach(fe_callback cb) {
	for (pluginList::iterator x = plugins.begin(); x != plugins.end(); x++) {
		cb(x->plug);
	}
}

void CheckDupes() {
	typedef std::map<string, PLUGIN_PUBLIC *, ci_less> pluginMap;
	pluginMap p;
	for (pluginList::iterator x = plugins.begin(); x != plugins.end(); x++) {
		pluginMap::iterator z = p.find(x->plug->guid);
		if (z != p.end()) {
			printf("Found duplicate GUID: %s in %s\n", x->plug->guid, x->plug->plugin_name_short);
			printf("Previous user was: %s\n", z->second->plugin_name_short);
		} else {
			p[x->plug->guid] = x->plug;
		}
	}
}


int main(int argc, char * argv[]) {
	char buf[MAX_PATH];
	Directory dir("."PATH_SEPS"plugins");
	bool is_dir;
	while (dir.Read(buf, sizeof(buf), &is_dir)) {
		if (!is_dir) {
			char * ext = strrchr(buf, '.');
			if (ext && (!stricmp(ext, ".dll") || !stricmp(ext, ".so"))) {
				string ffn = "."PATH_SEPS"plugins"PATH_SEPS"";
				ffn += buf;
				LoadPlugin(ffn.c_str());
			}
		}
	}

	CheckDupes();
	ForEach(print_info);

	UnloadPlugins();
	return 0;
}
