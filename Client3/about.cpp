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

void Execute(char * cmd) {
#if defined(WIN32)
	ShellExecuteA(NULL, "open", cmd, NULL, NULL, SW_SHOWNORMAL);
#else
	system(cmd);
#endif
}

class mAboutBox: public wxDialog
{

public:
	void AddLongHeader(const wxChar * str, wxTextCtrl * txt=NULL) {
		if (!txt) { txt = (wxTextCtrl *)this->FindWindowByName(_("IDC_LONGABOUT"), this); }
		if (txt) {
			int beg = txt->GetInsertionPoint();
			wxString str2;//(str);
			str2.Append(str);
			txt->AppendText(str2);
			int end = txt->GetLastPosition();
			wxTextAttr st = txt->GetDefaultStyle();
			wxFont ft = st.GetFont();
			ft.SetWeight(wxFONTWEIGHT_BOLD);
			st.SetFont(ft);
			st.SetAlignment(wxTEXT_ALIGNMENT_CENTRE);
			//st.SetFlags();
			txt->SetStyle(beg, end, st);
		}
	}

	void AddLongLineColor(const wxChar * str, uint8 r, uint8 g, uint8 b, wxTextCtrl * txt=NULL) {
		if (!txt) { txt = (wxTextCtrl *)this->FindWindowByName(_("IDC_LONGABOUT"), this); }
		if (txt) {
			int beg = txt->GetInsertionPoint();
			wxString str2(str);
			txt->AppendText(str2);
			int end = txt->GetLastPosition();

			wxTextAttr st = txt->GetDefaultStyle();
			wxColor col(r,g,b);
			st.SetTextColour(col);
			st.SetAlignment(wxTEXT_ALIGNMENT_CENTRE);
			txt->SetStyle(beg, end, st);
		}
	}

	void AddLongLine(const wxChar * str, wxTextCtrl * txt=NULL) {
		if (!txt) { txt = (wxTextCtrl *)this->FindWindowByName(_("IDC_LONGABOUT"), this); }
		if (txt) {
			int beg = txt->GetInsertionPoint();
			wxString str2(str);
			txt->AppendText(str2);
			int end = txt->GetLastPosition();
			txt->SetStyle(beg, end, txt->GetDefaultStyle());

			wxTextAttr st = txt->GetDefaultStyle();
			wxFont ft = st.GetFont();
			ft.SetWeight(wxFONTWEIGHT_BOLD);
			st.SetFont(ft);
			st.SetAlignment(wxTEXT_ALIGNMENT_CENTRE);
			txt->SetStyle(beg, end, st);

		}
	}

	void InitDialog(wxInitDialogEvent& event) {
		this->SetTitle(_("About RadioBot Client..."));
		wxStaticText * lb = (wxStaticText *)this->FindWindowByName(_("IDC_NAME"), this);
		if (lb) {
			lb->SetLabel(_("RadioBot DJ Client " VERSION ""));
		}
		lb = (wxStaticText *)this->FindWindowByName(_("IDC_COPYRIGHT"), this);
		if (lb) {
			lb->SetLabel(_("Copyright (c) 2003-2013 Drift Solutions. All Rights Reserved."));
		}
		lb = (wxStaticText *)this->FindWindowByName(_("IDC_COMPILED"), this);
		if (lb) {
			lb->SetLabel(_("Compiled on " __DATE__ " at " __TIME__ ""));
		}

		wxStaticBitmap * img = (wxStaticBitmap *)this->FindWindowByName(_("IDC_SLOGO"), this);
		if (img) {
			img->SetBitmap(StatusPics(5));
		}
		img = (wxStaticBitmap *)this->FindWindowByName(_("IDC_DLOGO"), this);
		if (img) {
			img->SetBitmap(StatusPics(4));
		}
		this->SetInitialSize();

		wxTextCtrl * txt = (wxTextCtrl *)this->FindWindowByName(_("IDC_LONGABOUT"), this);
		if (txt) {
			txt->Clear();

			time_t tme = time(NULL);
			tm t;
			localtime_r(&tme, &t);
			if (t.tm_mon == 11 && (t.tm_mday >= 24 || t.tm_mday <= 25)) {
				AddLongLineColor(_("Happy "), 0xff, 0, 0, txt);
				AddLongLineColor(_("Holidays\n\n"), 0, 0x80, 0, txt);
			}
			if (t.tm_mon == 9 && t.tm_mday == 31) {
				AddLongLineColor(_("Boo! Happy Halloween\n\n"), 0xFF, 0x80, 0, txt);
			}

			AddLongHeader(_("Developers\n"), txt);
			AddLongLine(_("Drift Solutions\n"), txt);

			AddLongLine(_("\n"), txt);
			AddLongHeader(_("Contributors\n"), txt);
			AddLongLine(_("Karol P. (Original GUI Design, ShoutIRC Logo)\n"), txt);

			AddLongLine(_("\n"), txt);
			AddLongHeader(_("Beta Testing\n"), txt);
			AddLongLine(_("Karol P.\n"), txt);

			AddLongLine(_("\n"), txt);
			AddLongHeader(_("Credits\n"), txt);
			AddLongLine(_("Uses software by:\nDrift Solutions (http://www.driftsolutions.com)\n"), txt);
			AddLongLine(_("Titus Software (http://www.titus-dev.com)\n"), txt);
			AddLongLine(_("Uses the following libraries:\n"), txt);
			AddLongLine(_("wxWidgets (http://www.wxwidgets.org)\n"), txt);
#if defined(ENABLE_OPENSSL)
			AddLongLine(_("OpenSSL (http://www.openssl.org/)\n"), txt);
#endif
#if defined(ENABLE_GNUTLS)
			AddLongLine(_("GnuTLS (http://www.gnutls.org/)\n"), txt);
#endif
			AddLongLine(_("Request beep sound by: FreqMan\n"), txt);

			AddLongLine(_("\nPlease see your Licenses folder for various software licenses...\n"), txt);

			txt->SetInsertionPoint(0);
		}
	}

	void OnQuit(wxCloseEvent& event) {
		Destroy();
	}

	void OnClickWWW(wxCommandEvent& ev) {
		Execute("http://www.shoutirc.com");
	}

	void OnClickWiki(wxCommandEvent& ev) {
		Execute("http://wiki.shoutirc.com");
	}

	void OnClickClose(wxCommandEvent& ev) {
		Destroy();
	}

private:
	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(mAboutBox, wxDialog)
	EVT_INIT_DIALOG(mAboutBox::InitDialog)
	EVT_CLOSE(mAboutBox::OnQuit)

	EVT_BUTTON(XRCID("IDC_WWW"), mAboutBox::OnClickWWW)
	EVT_BUTTON(XRCID("IDC_CLOSE"), mAboutBox::OnClickClose)
	EVT_BUTTON(XRCID("IDC_WIKI"), mAboutBox::OnClickWiki)
END_EVENT_TABLE()

bool OpenAboutBox(wxDialog * parent) {
	mAboutBox * conmgr = new mAboutBox;
	wxXmlResource::Get()->LoadDialog(conmgr, parent, _("AboutBox"));
	if (conmgr) {
		conmgr->ShowModal();
	} else {
		wxMessageBox(_("Error loading dialog from XRC"), _("Error"));
		return false;
	}
	return true;
}
