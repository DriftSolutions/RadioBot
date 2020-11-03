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

#include "client.h"

INT_PTR CALLBACK TabFindProc(HWND, UINT, WPARAM, LPARAM);

HWND CreateTabFind(HWND hParent) {
	return CreateDialog(config.hInst, MAKEINTRESOURCE(IDD_FIND), hParent, TabFindProc);
}

void LoadSAMInfo(HWND hWnd) {
	SetDlgItemText(hWnd, IDC_FIND_SAM_HOST, config.sam.mysql_host);
	SetDlgItemText(hWnd, IDC_FIND_SAM_USER, config.sam.mysql_user);
	SetDlgItemText(hWnd, IDC_FIND_SAM_PASS, config.sam.mysql_pass);
	SetDlgItemText(hWnd, IDC_FIND_SAM_DB, config.sam.mysql_db);
	SetDlgItemInt(hWnd, IDC_FIND_SAM_PORT, config.sam.mysql_port, FALSE);
}

bool ReadSAMConfig(HWND hWnd, const char * fn) {
	bool ret = false;
	if (access(fn, 0) == 0) {
		TiXmlDocument doc;
		if (doc.LoadFile(fn)) {
			TiXmlElement * root = doc.FirstChildElement("CONFIG");
			if (root) {
				TiXmlElement * db = root->FirstChildElement("Database");
				if (db) {
					TiXmlElement * type = db->FirstChildElement("Driver");
					if (type) {
						if (!stricmp(type->GetText(), "mysql")) {
							for (TiXmlElement * cur = db->FirstChildElement(); cur != NULL; cur = cur->NextSiblingElement()) {
								if (!stricmp(cur->Value(), "Host")) {
									SetDlgItemText(hWnd, IDC_FIND_SAM_HOST, cur->GetText());
								} else if (!stricmp(cur->Value(), "Username")) {
									SetDlgItemText(hWnd, IDC_FIND_SAM_USER, cur->GetText());
								} else if (!stricmp(cur->Value(), "Password")) {
									SetDlgItemText(hWnd, IDC_FIND_SAM_PASS, cur->GetText());
								} else if (!stricmp(cur->Value(), "Database")) {
									SetDlgItemText(hWnd, IDC_FIND_SAM_DB, cur->GetText());
								} else if (!stricmp(cur->Value(), "Port")) {
									SetDlgItemInt(hWnd, IDC_FIND_SAM_PORT, atoi(cur->GetText()), FALSE);
								}
							}
							ret = true;
						} else {
							ostringstream str;
							str << "Error: SAM is not set to use MySQL!\r\nIt is currently set to: " << type->GetText();
							MessageBox(hWnd, str.str().c_str(), "Error", MB_ICONERROR);
						}
					}
				}
			}
		} else {
			ostringstream str;
			str << "Error reading XML file: " << fn << "\r\nError: '" << doc.ErrorDesc() << "' at " << doc.ErrorRow() << ":" << doc.ErrorCol();
			MessageBox(hWnd, str.str().c_str(), "Error", MB_ICONERROR);
		}
	}
	return ret;
}

WM_HANDLER(Find_OnCommand) {
	if (LOWORD(wParam) == IDC_FIND_SAM_READ) {
		char fn[1024];
		uint32 ids[] = { CSIDL_LOCAL_APPDATA, CSIDL_APPDATA, CSIDL_PROGRAM_FILES, -1 };
		bool done = false;
		for (int i=0; ids[i] != -1; i++) {
			memset(fn, 0, sizeof(fn));
			if (SHGetFolderPath(NULL, ids[i], NULL, SHGFP_TYPE_CURRENT, fn) == S_OK && fn[0]) {
				if (fn[strlen(fn)-1] != PATH_SEP) {
					strlcat(fn, PATH_SEPS, sizeof(fn));
				}
				strlcat(fn, "SpacialAudio\\SAMBC\\SAMBC.core.xml", sizeof(fn));
				if (ReadSAMConfig(hWnd, fn)) {
					done = true;
					//LoadSAMInfo(hWnd);
				}
			}
		}
		if (!done) {
			MessageBox(hWnd, "Error locating, or error reading SAM config file!\r\nSorry, you'll have to input your configuration manually...", "Error", MB_ICONERROR);
		}
	} else if (LOWORD(wParam) == IDC_FIND_SAM_TEST) {
		GetDlgItemText(hWnd, IDC_FIND_SAM_HOST, config.sam.mysql_host, sizeof(config.sam.mysql_host));
		GetDlgItemText(hWnd, IDC_FIND_SAM_USER, config.sam.mysql_user, sizeof(config.sam.mysql_user));
		GetDlgItemText(hWnd, IDC_FIND_SAM_PASS, config.sam.mysql_pass, sizeof(config.sam.mysql_pass));
		GetDlgItemText(hWnd, IDC_FIND_SAM_DB, config.sam.mysql_db, sizeof(config.sam.mysql_db));
		config.sam.mysql_port = GetDlgItemInt(hWnd, IDC_FIND_SAM_PORT, NULL, FALSE);
		if (config.sam.mysql_port <= 0) { config.sam.mysql_port = 3306; }

		if (TT_NumThreadsWithID(2) == 0) {
			TT_StartThread(SAM_Import, NULL, "SAM Import Thread", 2);
		} else {
			MessageBox(hWnd, "There is already a database operation in progress!", "Operation In Progress", MB_ICONINFORMATION);
		}
	} else if (LOWORD(wParam) == IDC_FIND_DB_PICK) {
		CoInitialize(NULL);
		BROWSEINFO bi;
		memset(&bi, 0, sizeof(bi));
		bi.hwndOwner = hWnd;
		bi.ulFlags = BIF_NEWDIALOGSTYLE;
		LPITEMIDLIST hID = SHBrowseForFolder(&bi);
		if (hID != NULL) {
			char buf[1024];
			if (SHGetPathFromIDList(hID, buf)) {
				sstrcpy(config.music_db.folder, buf);
				if (config.music_db.folder[strlen(config.music_db.folder)-1] != PATH_SEP) {
					strlcat(config.music_db.folder, PATH_SEPS, sizeof(config.music_db.folder));
				}
				SetDlgItemText(hWnd, IDC_FIND_DB_FOLDER, config.music_db.folder);
			}
			CoTaskMemFree(hID);
		}
		CoUninitialize();
	} else if (LOWORD(wParam) == IDC_FIND_DB_SCAN) {
		if (TT_NumThreadsWithID(2) == 0) {
			TT_StartThread(Folder_Import, NULL, "Folder Scan Thread", 2);
		} else {
			MessageBox(hWnd, "There is already a database operation in progress!", "Operation In Progress", MB_ICONINFORMATION);
		}
	} else if (LOWORD(wParam) == IDC_FIND_DB_CLEAR) {
		if (TT_NumThreadsWithID(2) == 0) {
			MusicDB_Clear();
			SetDlgItemText(hWnd, IDC_FIND_DB_STATUS, "Song database cleared!");
		} else {
			MessageBox(hWnd, "There is already a database operation in progress!", "Operation In Progress", MB_ICONINFORMATION);
		}
	}

	/*
	if (LOWORD(wParam) == IDC_OPT_BEEP && HIWORD(wParam) == BN_CLICKED) {
		config.beep_on_req = (IsDlgButtonChecked(hWnd, IDC_OPT_BEEP) == BST_CHECKED) ? true:false;
	} else if (LOWORD(wParam) == IDC_OPT_KEEP && HIWORD(wParam) == BN_CLICKED) {
		config.keep_on_top = (IsDlgButtonChecked(hWnd, IDC_OPT_KEEP) == BST_CHECKED) ? true:false;
		SetWindowOnTop();
	}
	*/
	return TRUE;
}

WM_HANDLER(Find_OnInitDialog) {
	/*
	ComboBox_AddString(GetDlgItem(hWnd, IDC_FIND_METHODS), "None/Disabled");
	ComboBox_AddString(GetDlgItem(hWnd, IDC_FIND_METHODS), "Music Folder");
	ComboBox_AddString(GetDlgItem(hWnd, IDC_FIND_METHODS), "SAM Broadcaster");
	ComboBox_SetCurSel(GetDlgItem(hWnd, IDC_FIND_METHODS), config.find_method);
	*/

	SetDlgItemText(hWnd, IDC_FIND_DB_FOLDER, config.music_db.folder);
	UpdateMusicDBStatus(hWnd);

	LoadSAMInfo(hWnd);
	return TRUE;
}

BEGIN_HANDLER_MAP(tab_find_handlers)
	HANDLER_MAP_ENTRY(WM_COMMAND, Find_OnCommand)
	HANDLER_MAP_ENTRY(WM_INITDIALOG, Find_OnInitDialog)
END_HANDLER_MAP

INT_PTR CALLBACK TabFindProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	return HandleMap(tab_find_handlers, hWnd, uMsg, wParam, lParam);
}
