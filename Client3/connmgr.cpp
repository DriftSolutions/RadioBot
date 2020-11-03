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

void ChangeValue(wxTextCtrl * txt, char * value) {
	wxString str(value, wxConvUTF8);
#if (wxMAJOR_VERSION < 2) || (wxMAJOR_VERSION == 2 && wxMINOR_VERSION < 7) || (wxMAJOR_VERSION == 2 && wxMINOR_VERSION == 7 && wxRELEASE_NUMBER == 0)
	txt->SetEditable(false);
	txt->SetValue(str);
	txt->SetEditable(true);
#else
	txt->ChangeValue(str);
#endif
}

class ConnMgr: public wxDialog
{
private:
	int cursel;

public:
	void InitDialog(wxInitDialogEvent& event) {
		cursel = -1;
		this->SetTitle(_("Connection Manager"));
		wxListBox * lb = (wxListBox *)this->FindWindowByName(_("IDC_CONLIST"), this);
		if (lb) {
			for (int i=0; i < MAX_CONNECTIONS; i++) {
				if (strlen(config.hosts[i].name)) {
					wxString str(config.hosts[i].name, wxConvUTF8);
					//wxString str = config.hosts[i].name;
					lb->InsertItems(1, &str, i);
				}
			}
			lb->SetSelection(config.index);
			wxCommandEvent ev;
			OnSelectConn(ev);
		}
		//Destroy();
	}

	void OnQuit(wxCloseEvent& event) {
		Destroy();
	}

	void OnClickNew(wxCommandEvent& ev) {
		wxString str = wxGetTextFromUser(_("What name do you want to give your new connection?"), _("New Connection"), _(""), this, wxDefaultCoord, wxDefaultCoord, false);
		if (str.length()) {
			int ind = -1, i;
			for (i=0; i < MAX_CONNECTIONS; i++) {
				if (!strlen(config.hosts[i].name)) {
					ind = i;
					break;
				}
			}
			if (ind != -1) {
				wxListBox * lb = (wxListBox *)this->FindWindowByName(_("IDC_CONLIST"), this);
				if (lb) {
					strcpy(config.hosts[ind].name, (const char *)str.c_str());
					strcpy(config.hosts[ind].host, "localhost");
					strcpy(config.hosts[ind].username, "YourBotNick");
					config.hosts[ind].port = 10001;
					//wxString str2 = config.hosts[ind].name;
					lb->InsertItems(1, &str, ind);
					cursel = ind;
					lb->SetSelection(cursel);
					wxCommandEvent ev;
					OnSelectConn(ev);
				}
			} else {
				wxMessageBox(_("Error: You already have the maximum number of connections!"), _("Error"));
			}
		}
	}

	void OnSelectConn(wxCommandEvent& ev) {
		wxListBox * lb = (wxListBox *)this->FindWindowByName(_("IDC_CONLIST"), this);
		if (lb) {
			cursel = lb->GetSelection();
			if (cursel == wxNOT_FOUND) {
				cursel = -1;
				wxTextCtrl * txt = (wxTextCtrl *)this->FindWindowByName(_("IDC_BOTNAME"), this);
				if (txt) { txt->Enable(false); ChangeValue(txt,""); }
				txt = (wxTextCtrl *)this->FindWindowByName(_("IDC_BOTIP"), this);
				if (txt) { txt->Enable(false); ChangeValue(txt,""); }
				txt = (wxTextCtrl *)this->FindWindowByName(_("IDC_USERNAME"), this);
				if (txt) { txt->Enable(false); ChangeValue(txt,""); }
				txt = (wxTextCtrl *)this->FindWindowByName(_("IDC_PASS"), this);
				if (txt) { txt->Enable(false); ChangeValue(txt,""); }
				txt = (wxTextCtrl *)this->FindWindowByName(_("IDC_PORT"), this);
				if (txt) { txt->Enable(false); ChangeValue(txt,""); }
				wxRadioBox * rb = (wxRadioBox *)this->FindWindowByName(_("IDC_CONNTYPE"), this);
				if (rb) {
					rb->SetSelection(0);
				}
			} else {
				wxTextCtrl * txt = (wxTextCtrl *)this->FindWindowByName(_("IDC_BOTNAME"), this);
				if (txt) {
					txt->SetMaxLength(sizeof(config.hosts[cursel].name)-1);
					txt->Enable(true);
					ChangeValue(txt,config.hosts[cursel].name);
				}
				txt = (wxTextCtrl *)this->FindWindowByName(_("IDC_BOTIP"), this);
				if (txt) {
					txt->SetMaxLength(sizeof(config.hosts[cursel].host)-1);
					txt->Enable(true);
					ChangeValue(txt,config.hosts[cursel].host);
				}
				txt = (wxTextCtrl *)this->FindWindowByName(_("IDC_USERNAME"), this);
				if (txt) {
					txt->SetMaxLength(sizeof(config.hosts[cursel].username)-1);
					txt->Enable(true);
					ChangeValue(txt,config.hosts[cursel].username);
				}
				txt = (wxTextCtrl *)this->FindWindowByName(_("IDC_PASS"), this);
				if (txt) {
					txt->SetMaxLength(sizeof(config.hosts[cursel].pass)-1);
					txt->Enable(true);
					ChangeValue(txt,config.hosts[cursel].pass);
				}
				txt = (wxTextCtrl *)this->FindWindowByName(_("IDC_PORT"), this);
				if (txt) {
					txt->SetMaxLength(5);
					txt->Enable(true);
					char buf[6];
					if (config.hosts[cursel].port == 0) {
						config.hosts[cursel].port = 10001;
					}
					sprintf(buf, "%u", config.hosts[cursel].port);
					ChangeValue(txt, buf);
				}

				wxRadioBox * rb = (wxRadioBox *)this->FindWindowByName(_("IDC_CONNTYPE"), this);
				if (rb) {
					if (config.hosts[cursel].mode > 2) {
						config.hosts[cursel].mode = 0;
					}
					rb->SetSelection(config.hosts[cursel].mode);
				}
			}
		}
	}

	void OnClickDelete(wxCommandEvent& ev) {
		if (cursel != -1) {
			if (IsConnected()) {
				wxMessageBox(_("This will disconnect your current session"), _("Notice"));
				DoDisconnect();
			}
			if (cursel == 0) {
				memmove(&config.hosts[0], &config.hosts[1], sizeof(config.hosts[0])*(MAX_CONNECTIONS-1));
			} else if (cursel == (MAX_CONNECTIONS-1)) {
				memset(&config.hosts[cursel], 0, sizeof(config.hosts[cursel]));
			} else if (!strlen(config.hosts[cursel+1].name)) {
				memset(&config.hosts[cursel], 0, sizeof(config.hosts[cursel]));
			} else {
				int num = MAX_CONNECTIONS - cursel - 1;
				memmove(&config.hosts[cursel], &config.hosts[cursel+1], sizeof(config.hosts[0])*num);
				memset(&config.hosts[(MAX_CONNECTIONS-1)], 0, sizeof(config.hosts[(MAX_CONNECTIONS-1)]));
			}
			wxListBox * lb = (wxListBox *)this->FindWindowByName(_("IDC_CONLIST"), this);
			//if (lb) {
				lb->Clear();
				for (int i=0; i < MAX_CONNECTIONS; i++) {
					if (strlen(config.hosts[i].name)) {
						wxString str(config.hosts[i].name, wxConvUTF8);
						lb->InsertItems(1, &str, i);
					}
				}
				config.index = cursel = 0;
				lb->SetSelection(cursel);
				wxCommandEvent ev;
				OnSelectConn(ev);
			//}
		} else {
			wxMessageBox(_("Error: You do not have any connection selected!"), _("Error"));
		}
	}

	void OnClickConnect(wxCommandEvent& ev) {
		if (cursel != -1) {
			if (!IsConnected() || wxMessageBox(_("Break the current connection?"), _("Confirm"), wxYES_NO|wxICON_QUESTION, this) == wxYES) {
				SchedConnect(cursel);
				Destroy();
			}
		} else {
			wxMessageBox(_("Error: You do not have any connection selected!"), _("Error"));
		}
	}

	void OnDoubleClick(wxCommandEvent& ev) {
		OnSelectConn(ev);
		if (cursel != -1) {
			OnClickConnect(ev);
		}
	}

	void OnClickExit(wxCommandEvent& ev) {
		Destroy();
	}

	void OnUpdateBotName(wxCommandEvent& ev) {
		printf("update\n");
		if (cursel != -1) {
			strcpy(config.hosts[cursel].name, (const char *)ev.GetString().c_str());
			wxListBox * lb = (wxListBox *)this->FindWindowByName(_("IDC_CONLIST"), this);
			if (lb) {
				lb->SetString(cursel, ev.GetString());
			}
		}
	}

	void OnUpdateBotIP(wxCommandEvent& ev) {
		if (cursel != -1) {
			strcpy(config.hosts[cursel].host, (const char *)ev.GetString().c_str());
		}
	}

	void OnUpdateUsername(wxCommandEvent& ev) {
		if (cursel != -1) {
			strcpy(config.hosts[cursel].username, (const char *)ev.GetString().c_str());
		}
	}

	void OnUpdatePass(wxCommandEvent& ev) {
		if (cursel != -1) {
			strcpy(config.hosts[cursel].pass, (const char *)ev.GetString().c_str());
		}
	}

	void OnUpdatePort(wxCommandEvent& ev) {
		if (cursel != -1) {
			int port = atoi((const char *)ev.GetString().c_str());
			if (port > 65535 || port == 0) {
				port = 10001;
			}
			char buf[6];
			sprintf(buf, "%u", config.hosts[cursel].port);
			config.hosts[cursel].port = port;
		}
	}

	void OnChangeConnType(wxCommandEvent& ev) {
		if (cursel != -1) {
			config.hosts[cursel].mode = ev.GetInt();
		}
	}

private:
	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(ConnMgr, wxDialog)
	EVT_INIT_DIALOG(ConnMgr::InitDialog)
	EVT_CLOSE(ConnMgr::OnQuit)

	EVT_BUTTON(XRCID("IDC_NEW"), ConnMgr::OnClickNew)
	EVT_BUTTON(XRCID("IDC_DELETE"), ConnMgr::OnClickDelete)
	EVT_BUTTON(XRCID("IDC_CONNECT"), ConnMgr::OnClickConnect)
	EVT_BUTTON(XRCID("IDC_EXIT"), ConnMgr::OnClickExit)

	EVT_LISTBOX(XRCID("IDC_CONLIST"), ConnMgr::OnSelectConn)
	EVT_LISTBOX_DCLICK(XRCID("IDC_CONLIST"), ConnMgr::OnDoubleClick)

	EVT_TEXT(XRCID("IDC_BOTNAME"), ConnMgr::OnUpdateBotName)
	EVT_TEXT(XRCID("IDC_BOTIP"), ConnMgr::OnUpdateBotIP)
	EVT_TEXT(XRCID("IDC_USERNAME"), ConnMgr::OnUpdateUsername)
	EVT_TEXT(XRCID("IDC_PASS"), ConnMgr::OnUpdatePass)
	EVT_TEXT(XRCID("IDC_PORT"), ConnMgr::OnUpdatePort)
	EVT_RADIOBOX(XRCID("IDC_CONNTYPE"), ConnMgr::OnChangeConnType)
END_EVENT_TABLE()

bool OpenConnManager(wxDialog * parent) {
	//wxXmlResource::Get()->Load(_("client_wdr.xrc"));
	//rc.SetOutput(status_printf);
	ConnMgr * conmgr = new ConnMgr;
//		pDlg = dlg;
	//(MainDlg *)wxXmlResource::Get()->LoadDialog(NULL, "MainDlg2");
	wxXmlResource::Get()->LoadDialog(conmgr, parent, _("ConnectionManager"));
	if (conmgr) {
		conmgr->ShowModal();
		//conmgr->Show(true);
		//SetTopWindow(conmgr);
		//dlg->Destroy();
	} else {
		wxMessageBox(_("Error loading dialog from XRC"), _("Error"));
		return false;
	}
	return true;
}
