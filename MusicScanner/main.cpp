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

int xstatus_printf(char * fmt, ...);
int xdebug_printf(char * fmt, ...);

#if defined(__WXMSW__) && !defined(__WXWINCE__)
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='X86' publicKeyToken='6595b64144ccf1df'\"")
#endif

//bool shutdown_now = false
bool debug = false;
//Titus_Mutex hMutex;

void clearLog();

class MainDlg: public wxFrame
{
public:
//	wxTimer tmr;
//	int newindex;

	void OnInit() {
		SetTitle(_("WebRequest Music Scanner "VERSION"/"PLATFORM""));

		if (!InitCore(xdebug_printf, xstatus_printf, NULL)) {
			wxMessageBox("Error initializing Scanner Core!", "Fatal Error");
			Destroy();
			return;
		}

		wxColor col;
		wxButton * but = dynamic_cast<wxButton *>(this->FindItem(XRCID("ID_SCAN")));
		if (but) { col = but->GetBackgroundColour(); }
		this->SetBackgroundColour(col);

		clearLog();
		//wxListBox * com = dynamic_cast<wxListBox *>(this->FindItem(XRCID("ID_LOG")));
		//if (com) {
		//	com->Clear();
		//}


		wxTextCtrl * txt = dynamic_cast<wxTextCtrl *>(this->FindItem(XRCID("ID_FOLDER1")));
		if (txt) { txt->SetValue(config.music_folder[0]); }
		txt = dynamic_cast<wxTextCtrl *>(this->FindItem(XRCID("ID_FOLDER2")));
		if (txt) { txt->SetValue(config.music_folder[1]); }
		txt = dynamic_cast<wxTextCtrl *>(this->FindItem(XRCID("ID_FOLDER3")));
		if (txt) { txt->SetValue(config.music_folder[2]); }
		txt = dynamic_cast<wxTextCtrl *>(this->FindItem(XRCID("ID_OUTFILE")));
		if (txt) { txt->SetValue(config.out_fn); }

		if (config.compressOutput) {
			wxCheckBox * chk = dynamic_cast<wxCheckBox *>(this->FindItem(XRCID("ID_COMPRESS")));
			if (chk) {
				chk->SetValue(true);
			}
		}
		//SetWindowStyle(GetWindowStyle()|wxMINIMIZE_BOX);
	}

	void OnQuit(wxCloseEvent& event) {
		//shutdown_now = true;
		if (TT_NumThreads() == 0) {
			QuitCore();
			Destroy();
		} else {
			wxMessageBox(_("You can not close the window while the scan is in progress!"), _("Error"));
		}
	}

	void OnClickScan(wxCommandEvent& ev) {
		if (TT_NumThreads()) {
			wxMessageBox(_("A scan is already in progress!"), _("Error"));
			return;
		}

		wxCheckBox * chk = dynamic_cast<wxCheckBox *>(this->FindItem(XRCID("ID_COMPRESS")));
		if (chk && chk->IsChecked()) {
			config.compressOutput = true;
		} else {
			config.compressOutput = false;
		}

		wxTextCtrl * txt = dynamic_cast<wxTextCtrl *>(this->FindItem(XRCID("ID_OUTFILE")));
		if (txt) {
			strcpy(config.out_fn, txt->GetValue().c_str());
		}

		if (!strlen(config.out_fn)) {
			wxMessageBox(_("No output file specified!"), _("Error"));
			return;
		}

		char * p = strrchr(config.out_fn, '.');
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

		txt->SetValue(config.out_fn);

		/*
		wxButton * but = dynamic_cast<wxButton *>(this->FindItem(XRCID("ID_SCAN")));
		if (but) { but->Enable(false); }
		but = dynamic_cast<wxButton *>(this->FindItem(XRCID("ID_DELETE")));
		if (but) { but->Enable(false); }
	wxButton * but = dynamic_cast<wxButton *>(GetMainDlg()->FindItem(XRCID("ID_SCAN")));
	if (but) { but->Enable(true); }
	but = dynamic_cast<wxButton *>(GetMainDlg()->FindItem(XRCID("ID_DELETE")));
	if (but) { but->Enable(true); }
		*/


		txt = dynamic_cast<wxTextCtrl *>(this->FindItem(XRCID("ID_FOLDER1")));
		if (txt) { strcpy(config.music_folder[0], txt->GetValue().c_str()); }
		txt = dynamic_cast<wxTextCtrl *>(this->FindItem(XRCID("ID_FOLDER2")));
		if (txt) { strcpy(config.music_folder[1], txt->GetValue().c_str()); }
		txt = dynamic_cast<wxTextCtrl *>(this->FindItem(XRCID("ID_FOLDER3")));
		if (txt) { strcpy(config.music_folder[2], txt->GetValue().c_str()); }
		for (int i=0; i < NUM_FOLDERS; i++) {
			if (strlen(config.music_folder[i]) && config.music_folder[i][strlen(config.music_folder[i])-1] != PATH_SEP) { strcat(config.music_folder[i], PATH_SEPS); }
		}

		clearLog();
		DoScan();
	}

	void OnClickDeleteMetaCache(wxCommandEvent& ev) {
		if (TT_NumThreads()) {
			wxMessageBox(_("A scan is already in progress,!"), _("Error"));
		} else {
			WipeDB();
		}
	}

	void OnClickFolderBrowse(wxCommandEvent& ev) {
		int id = ev.GetId();
		if (id == XRCID("ID_BROWSEM1")) {
			id = XRCID("ID_FOLDER1");
		} else if (id == XRCID("ID_BROWSEM2")) {
			id = XRCID("ID_FOLDER2");
		} else if (id == XRCID("ID_BROWSEM3")) {
			id = XRCID("ID_FOLDER3");
		}
		wxTextCtrl * txt = dynamic_cast<wxTextCtrl *>(this->FindItem(id));
		if (txt) {
			wxDirDialog dialog(this, "Select a folder with your music in it:", txt->GetValue());
			if (dialog.ShowModal() == wxID_OK && dialog.GetPath().length()) {
				txt->SetValue(dialog.GetPath());
			}
		}
	}

	void OnClickOutBrowse(wxCommandEvent& ev) {
		wxTextCtrl * txt = dynamic_cast<wxTextCtrl *>(this->FindItem(XRCID("ID_OUTFILE")));
		if (txt) {
			wxFileDialog dialog(this, "Select an output file...", wxEmptyString, "music.smd", "ShoutIRC Music Databases (*.SMD, *.SMZ)|*.SMD;*.SMZ", wxFD_OPEN|wxFD_OVERWRITE_PROMPT);
			if (dialog.ShowModal() == wxID_OK && dialog.GetPath().length()) {
				txt->SetValue(dialog.GetPath());
			}
		}
	}

private:
	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(MainDlg, wxFrame)
//	EVT_INIT_DIALOG(MainDlg::InitDialog)
	EVT_CLOSE(MainDlg::OnQuit)

	//EVT_MENU(XRCID("ID_LOAD"), MainDlg::OnMenuOpen)
	//EVT_MENU(XRCID("ID_EXIT"), MainDlg::OnMenuExit)
	EVT_BUTTON(XRCID("ID_SCAN"), MainDlg::OnClickScan)
	EVT_BUTTON(XRCID("ID_DELETE"), MainDlg::OnClickDeleteMetaCache)
	EVT_BUTTON(XRCID("ID_BROWSEM1"), MainDlg::OnClickFolderBrowse)
	EVT_BUTTON(XRCID("ID_BROWSEM2"), MainDlg::OnClickFolderBrowse)
	EVT_BUTTON(XRCID("ID_BROWSEM3"), MainDlg::OnClickFolderBrowse)
	EVT_BUTTON(XRCID("ID_BROWSEOUT"), MainDlg::OnClickOutBrowse)

	//EVT_TEXT(XRCID("ID_BASE_SENDQ"), MainDlg::OnTextOnlyNumeric)

	//EVT_NOTEBOOK_PAGE_CHANGED(XRCID("ID_NOTEBOOK"), MainDlg::OnTabChange)
END_EVENT_TABLE()

MainDlg * dlg = NULL;
wxFrame * GetMainDlg() { return dlg; }

void clearLog() {
	wxListBox * com = dynamic_cast<wxListBox *>(dlg->FindItem(XRCID("ID_LOG")));
	if (com) {
		com->Clear();
	}
}

int xstatus_printf(char * fmt, ...) {
	va_list va;
	va_start(va, fmt);
	char buf[4096];
	vsnprintf(buf, sizeof(buf)-1, fmt, va);
	buf[4095] = 0;
	wxListBox * com = dynamic_cast<wxListBox *>(dlg->FindItem(XRCID("ID_LOG")));
	if (com) {
		//(const wxChar *)&
		const wxString str(buf);
		com->AppendAndEnsureVisible(str);
		//com->InsertItems(1, &str, 0);
		com->SetFirstItem(com->GetCount() > 0 ? com->GetCount()-1:0);
	}
	printf("%s\n", buf);
	va_end(va);
	return 1;
}

int xdebug_printf(char * fmt, ...) {
	va_list va;
	va_start(va, fmt);
	char buf[8192];
	memset(&buf, 0, sizeof(buf));
	vsnprintf(buf, sizeof(buf)-1, fmt, va);
	va_end(va);
	printf("%s", buf);
	if (debug) {
		//ldc_SendString(buf);
	}
	return 1;
}

class MyApp: public wxApp
{
	virtual bool OnInit() {
#if !defined(WIN32)
		signal(SIGSEGV, SIG_IGN);
		signal(SIGPIPE, SIG_IGN);
#endif

		memset(&config, 0, sizeof(config));
		//debug_printf("Loading config\n");
//		LoadConfig();

		wxInitAllImageHandlers();
		wxXmlResource::Get()->InitAllHandlers();
#if defined(DEBUG)
		wxXmlResource::Get()->Load("scanner_wdr.xrc");
		chdir("../Output");
#else
		CL3_InitXmlResource();
#endif

		dlg = new MainDlg;
		wxXmlResource::Get()->LoadFrame(dlg, NULL, _("MainDlg"));
		if (dlg) {
			xdebug_printf("Main dialog loaded, opening...\n");
			dlg->OnInit();
			dlg->Center();
			dlg->Show(true);
			//dlg->SetInitialSize();
			SetTopWindow(dlg);
		} else {
			wxMessageBox(_("Error loading dialog from XRC"), _("Fatal Error"));
			return false;
		}
		return true;
	}

	virtual int OnExit() {
		if (config.dbHandle) { sqlite3_close(config.dbHandle); config.dbHandle=NULL; }
		debug_printf("wxApp::OnExit()\n");
		debug_printf("Bye\n");
		return 0;
	}
};

IMPLEMENT_APP(MyApp)
