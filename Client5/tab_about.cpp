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
#include <Richedit.h>

INT_PTR CALLBACK TabAboutProc(HWND, UINT, WPARAM, LPARAM);

HWND CreateTabAbout(HWND hParent) {
	HWND hWnd = CreateDialog(config.hInst, MAKEINTRESOURCE(IDD_ABOUT), hParent, TabAboutProc);
	return hWnd;
}

WM_HANDLER(About_OnCommand) {
	if (LOWORD(wParam) == IDC_ABOUT_WIKI) {
		ShellExecute(NULL, "open", "http://wiki.shoutirc.com/", NULL, NULL, SW_SHOWNORMAL);
	} else if (LOWORD(wParam) == IDC_ABOUT_FORUM) {
		ShellExecute(NULL, "open", "http://forums.shoutirc.com/", NULL, NULL, SW_SHOWNORMAL);
	} else if (LOWORD(wParam) == IDC_ABOUT_WEBSITE) {
		ShellExecute(NULL, "open", "http://www.shoutirc.com/", NULL, NULL, SW_SHOWNORMAL);
	}
	return TRUE;
}

WM_HANDLER(About_OnInitDialog) {
	//char buf[1024];
	SetDlgItemText(hWnd, IDC_ABOUT_VERSION, "RadioBot DJ Client " VERSION "");
	SetDlgItemText(hWnd, IDC_ABOUT_COMPILE, "Compiled on " __DATE__ " at " __TIME__ "");

	PARAFORMAT pf;
	memset(&pf, 0, sizeof(pf));
	pf.cbSize = sizeof(pf);
	pf.dwMask = PFM_ALIGNMENT;
	pf.wAlignment = PFA_CENTER;
	SendDlgItemMessage(hWnd, IDC_ABOUT_RTF, EM_SETPARAFORMAT, 0, (LPARAM)&pf);
	stringstream str;
	str << "Credits\r\n\r\n";

	str << "Developers\r\n";
	str << "Drift Solutions\r\n\r\n";

	str << "Libraries Used\r\n";
#if defined(ENABLE_OPENSSL)
	str << "OpenSSL (http://www.openssl.org)\r\n";
#endif
#if defined(ENABLE_GNUTLS)
	str << "GnuTLS (http://www.gnutls.org)\r\n";
#endif
	str << "SQLite (http://www.sqlite.org)\r\n";
#if defined(WIN32)
	str << "libmariadb (https://mariadb.org)\r\n\r\n";
#else
	str << "libmysqlclient (http://www.mysql.com)\r\n\r\n";
#endif

	str << "Sound & Graphics\r\n";
	str << "ShoutIRC Logo: Hackez\r\n";
	str << "Request beep sound: FreqMan\r\n";
	str << "Traffic light icons: FatCow Web Hosting (http://www.fatcow.com), CC BY 3.0\r\n";
	str << "Arrow on Admin tab: Mark James (http://www.famfamfam.com), CC BY 2.5\r\n";
	SetDlgItemText(hWnd, IDC_ABOUT_RTF, str.str().c_str());
	return TRUE;
}

BEGIN_HANDLER_MAP(tab_about_handlers)
	HANDLER_MAP_ENTRY(WM_COMMAND, About_OnCommand)
	HANDLER_MAP_ENTRY(WM_INITDIALOG, About_OnInitDialog)
END_HANDLER_MAP

INT_PTR CALLBACK TabAboutProc(HWND hWnd, UINT uMsg, WPARAM wParam, LPARAM lParam) {
	return HandleMap(tab_about_handlers, hWnd, uMsg, wParam, lParam);
}
