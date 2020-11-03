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

class mOptionsBox: public wxDialog
{
public:

	void InitDialog(wxInitDialogEvent& event) {
		this->SetTitle(_("RadioBot Client Options..."));
		wxCheckBox * cb = (wxCheckBox *)this->FindWindowByName(_("IDC_ONTOP"), this);
		if (cb) {
			cb->SetValue(config.keep_on_top);
		}
		cb = (wxCheckBox *)this->FindWindowByName(_("IDC_BEEP"), this);
		if (cb) {
			cb->SetValue(config.beep_on_req);
		}
	}

	void OnQuit(wxCloseEvent& event) {
		Destroy();
	}

	void OnClickSave(wxCommandEvent& ev) {
		wxCheckBox * cb = (wxCheckBox *)this->FindWindowByName(_("IDC_ONTOP"), this);
		if (cb) {
			config.keep_on_top = cb->GetValue();
		}
		cb = (wxCheckBox *)this->FindWindowByName(_("IDC_BEEP"), this);
		if (cb) {
			config.beep_on_req = cb->GetValue();
		}
		Destroy();
	}

private:
	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(mOptionsBox, wxDialog)
	EVT_INIT_DIALOG(mOptionsBox::InitDialog)
	EVT_CLOSE(mOptionsBox::OnQuit)

	EVT_BUTTON(XRCID("IDC_SAVE"), mOptionsBox::OnClickSave)
END_EVENT_TABLE()

bool OpenOptions(wxDialog * parent) {
	mOptionsBox * conmgr = new mOptionsBox;
	wxXmlResource::Get()->LoadDialog(conmgr, parent, _("OptionsDlg"));
	if (conmgr) {
		conmgr->ShowModal();
	} else {
		delete conmgr;
		wxMessageBox(_("Error loading dialog from XRC"), _("Error"));
		return false;
	}
	return true;
}
