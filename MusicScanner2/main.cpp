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

#include "scanner.h"
#include <Windowsx.h>
#include <shlobj.h>

#if defined(DEBUG)
#pragma comment(lib, "sqlite3_d.lib")
#else
#pragma comment(lib, "sqlite3.lib")
#endif

int xstatus_printf(char * fmt, ...);
int xdebug_printf(char * fmt, ...);

//bool shutdown_now = false
bool debug = false;
HWND mWnd = NULL;
//Titus_Mutex hMutex;
void clearLog();

INT_PTR CALLBACK DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam);

int __stdcall WinMain(HINSTANCE hInst, HINSTANCE hPrevInst, LPSTR lpCmdLine, int nShowCmd) {
	if (!InitCore(xdebug_printf, xstatus_printf, NULL)) {
		MessageBox(NULL, "Error initializing Scanner Core!", "Fatal Error", MB_ICONERROR);
		return 1;
	}

	HWND hWnd = CreateDialog(hInst, MAKEINTRESOURCE(IDD_MAINDLG), NULL, DlgProc);
	MSG msg;
	while (GetMessage(&msg, NULL, 0, 0) > 0) {
		if (!IsDialogMessage(hWnd, &msg)) {
			TranslateMessage(&msg);
			DispatchMessage(&msg);
		}
	}

	QuitCore();
	return 0;
}

bool BrowseForFolder(char * buf, size_t len) {
    BROWSEINFO bi = { 0 };
	bi.lpszTitle = ("Select a folder...");
    LPITEMIDLIST pidl = SHBrowseForFolder ( &bi );
    if (pidl != NULL) {
        // get the name of the folder and put it in path
        SHGetPathFromIDList(pidl, buf);
        // free memory used
        IMalloc * imalloc = NULL;
        if ( SUCCEEDED( SHGetMalloc ( &imalloc )) ) {
            imalloc->Free(pidl);
            imalloc->Release();
			return (buf[0] != 0) ? true:false;
        }
    }
	return false;
}

INT_PTR CALLBACK DlgProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	switch (uMsg) {
		case WM_INITDIALOG:
			mWnd = hWnd;
			SetWindowText(hWnd, "WebRequest Music Scanner " VERSION "/" PLATFORM "");
			SetDlgItemText(hWnd, IDC_FOLDER1, config.music_folder[0]);
			SetDlgItemText(hWnd, IDC_FOLDER2, config.music_folder[1]);
			SetDlgItemText(hWnd, IDC_FOLDER3, config.music_folder[2]);
			SetDlgItemText(hWnd, IDC_FILE, config.out_fn);

			if (config.compressOutput) {
				Button_SetCheck(GetDlgItem(hWnd, IDC_COMP), BST_CHECKED);
			}
			ShowWindow(hWnd, SW_SHOW);
			return TRUE;
			break;

		case WM_CLOSE:
			if (TT_NumThreads()) {
				MessageBox(hWnd, "Please wait for the current scan to complete before closing the window!", "Error", 0);
			} else {
				EndDialog(hWnd, 0);
				PostQuitMessage(0);
			}
			return TRUE;
			break;

		case WM_COMMAND:
			switch (LOWORD(wParam)) {
				case IDC_WIPE:
					if (TT_NumThreads()) {
						MessageBox(hWnd, "You can not do this when a scan is already in progress!", "Error", MB_ICONERROR);
					} else {
						WipeDB();
					}
					break;

				case IDC_SCAN:
					if (TT_NumThreads()) {
						MessageBox(hWnd, "You can not do this when a scan is already in progress!", "Error", MB_ICONERROR);
					} else {
						if (!strlen(config.out_fn)) {
							MessageBox(hWnd, "No output file specified!", "Error", MB_ICONERROR);
						} else {
							//EnableWindow(GetDlgItem(hWnd, IDC_SCAN), FALSE);
							//EnableWindow(GetDlgItem(hWnd, IDC_WIPE), FALSE);
							DoScan();
						}
					}
					break;

				case IDC_BROWSE1:
				case IDC_BROWSE2:
				case IDC_BROWSE3:
					char buf[MAX_PATH];
					if (BrowseForFolder(buf, sizeof(buf))) {
						if (buf[strlen(buf)-1] != PATH_SEP) {
							strcat(buf, PATH_SEPS);
						}
						if (LOWORD(wParam) == IDC_BROWSE1) {
							strcpy(config.music_folder[0], buf);
							SetDlgItemText(hWnd, IDC_FOLDER1, buf);
						} else if (LOWORD(wParam) == IDC_BROWSE2) {
							strcpy(config.music_folder[1], buf);
							SetDlgItemText(hWnd, IDC_FOLDER2, buf);
						} else if (LOWORD(wParam) == IDC_BROWSE3) {
							strcpy(config.music_folder[2], buf);
							SetDlgItemText(hWnd, IDC_FOLDER3, buf);
						}
					}
					break;

				case IDC_CLEAR1:
				case IDC_CLEAR2:
				case IDC_CLEAR3:
					if (LOWORD(wParam) == IDC_CLEAR1) {
						strcpy(config.music_folder[0], "");
						SetDlgItemText(hWnd, IDC_FOLDER1, "");
					} else if (LOWORD(wParam) == IDC_CLEAR2) {
						strcpy(config.music_folder[1], "");
						SetDlgItemText(hWnd, IDC_FOLDER2, "");
					} else if (LOWORD(wParam) == IDC_CLEAR3) {
						strcpy(config.music_folder[2], "");
						SetDlgItemText(hWnd, IDC_FOLDER3, "");
					}
					break;

				case IDC_BROWSE4:
					OPENFILENAME ofn;
					strcpy(buf, config.out_fn);
					memset(&ofn, 0, sizeof(ofn));
					ofn.lStructSize = sizeof(ofn);
					ofn.hwndOwner = hWnd;
					ofn.lpstrFilter = "ShoutIRC Music Databases (*.SMD, *.SMZ)\0*.SMD;*.SMZ\0\0";
					ofn.lpstrFile = buf;
					ofn.nMaxFile = sizeof(buf);
					ofn.Flags = OFN_NOCHANGEDIR|OFN_OVERWRITEPROMPT;
					if (GetSaveFileName(&ofn) && strlen(buf)) {
						strcpy(config.out_fn, buf);
						SetDlgItemText(hWnd, IDC_FILE, config.out_fn);
					}
					break;

				case IDC_COMP:
					if (HIWORD(wParam) == BN_CLICKED) {
						config.compressOutput = (IsDlgButtonChecked(hWnd, IDC_COMP) == BST_CHECKED) ? true:false;

						char * p = strrchr(config.out_fn, '.');
						if (p) {
							if (config.compressOutput) {
								if (stricmp(p, ".smz")) {
									if (!stricmp(p, ".smd")) {
										strcpy(p, ".smz");
									} else {
										strcat(config.out_fn, ".smz");
									}
								}
							} else {
								if (stricmp(p, ".smd")) {
									if (!stricmp(p, ".smz")) {
										strcpy(p, ".smd");
									} else {
										strcat(config.out_fn, ".smd");
									}
								}
							}
							SetDlgItemText(hWnd, IDC_FILE, config.out_fn);
						}
					}
					break;

			}
			break;
	}
	return FALSE;
}

void clearLog() {
	ListBox_ResetContent(GetDlgItem(mWnd, IDC_LOG));
}

int xstatus_printf(char * fmt, ...) {
	va_list va;
	va_start(va, fmt);
	char buf[4096];
	vsnprintf(buf, sizeof(buf)-1, fmt, va);
	buf[4095] = 0;

	ListBox_AddString(GetDlgItem(mWnd, IDC_LOG), buf);
	ListBox_SetCurSel(GetDlgItem(mWnd, IDC_LOG), ListBox_GetCount(GetDlgItem(mWnd, IDC_LOG)) - 1);
	//printf("%s\n", buf);
	va_end(va);
	return 1;
}

int xdebug_printf(char * fmt, ...) {
	/*
	va_list va;
	va_start(va, fmt);
	char buf[8192];
	memset(&buf, 0, sizeof(buf));
	vsnprintf(buf, sizeof(buf)-1, fmt, va);
	va_end(va);
	printf("%s", buf);
	//if (debug) {
		//ldc_SendString(buf);
	//}
	*/
	return 1;
}
