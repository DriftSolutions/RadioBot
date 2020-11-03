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

#if defined(__WXMSW__) && !defined(__WXWINCE__)
#pragma comment(linker, "\"/manifestdependency:type='win32' name='Microsoft.Windows.Common-Controls' version='6.0.0.0' processorArchitecture='X86' publicKeyToken='6595b64144ccf1df'\"")
#endif

RemoteClient rc;
CONFIG config;
bool shutdown_now = false, debug = false;
bool disconnect = false, doConnect = false;
int conindex = 0;
Titus_Mutex hMutex;
TT_DEFINE_THREAD(ConxThread);

class MainDlg: public wxDialog
{
public:
	/*
	void HideAdminPanel() {
#if defined(WIN32)
		wxButton * com = (wxButton *)this->FindWindowByName(_("IDC_NEXT"), this);
		if (com) {
			wxBoxSizer * box = (wxBoxSizer *)com->GetContainingSizer();// this->FindItem(XRCID("IDC_ADMIN"));
			if (box) {
				box->Show(false);
				SetInitialSize();
			}
		}
#endif
	}

	void ShowAdminPanel() {
#if defined(WIN32)
		wxButton * com = (wxButton *)this->FindWindowByName(_("IDC_NEXT"), this);
		if (com) {
			wxBoxSizer * box = (wxBoxSizer *)com->GetContainingSizer();// this->FindItem(XRCID("IDC_ADMIN"));
			if (box) {
				box->Show(true);
				SetInitialSize();
			}
		}
#endif
	}
	*/

	void InitDialog(wxInitDialogEvent& event) {
		this->SetTitle(_("RadioBot Client 3.0/" PLATFORM ""));
		this->SetWindowStyle(this->GetWindowStyle()|wxMINIMIZE_BOX);
		if (config.keep_on_top) {
			this->SetWindowStyle(this->GetWindowStyle()|wxSTAY_ON_TOP);
		}
		//int i = XRCID("IDC_ADMIN");
		//HideAdminPanel();
		wxStaticBitmap * img = (wxStaticBitmap *)this->FindWindowByName(_("IDC_STATUS_PIC"), this);
		if (img) {
			img->SetBitmap(StatusPics(0));
		}
		wxButton * com = (wxButton *)this->FindWindowByName(_("IDC_DJ_LOGIN"), this);
		if (com) {
			wxColor colbg(0x00, 0x80, 0x00);
			wxColor colfg(0xFF, 0xFF, 0xFF);
			com->SetBackgroundColour(colbg);
			com->SetForegroundColour(colfg);
		}
		com = (wxButton *)this->FindWindowByName(_("IDC_DJ_LOG_OUT"), this);
		if (com) {
			wxColor colbg(0xFF, 0x00, 0x00);
			wxColor colfg(0xFF, 0xFF, 0xFF);
			com->SetBackgroundColour(colbg);
			com->SetForegroundColour(colfg);
		}
		//Destroy();
	}

	void OnQuit(wxCloseEvent& event) {
		rc.SetOutput(NULL);
		shutdown_now = true;
		disconnect = true;
		Destroy();
	}

	void OnClickLogin(wxCommandEvent& ev) {
		if (IsConnected()) {
			wxButton * com = (wxButton *)this->FindWindowByName(_("IDC_LOGIN"), this);
			if (com) {
				com->Enable(false);
				disconnect = true;
			}
		} else {
			SchedConnect(config.index);
		}
	}

	void OnClickConnections(wxCommandEvent& ev) {
		OpenConnManager(this);
	}

	void OnClickDjLogin(wxCommandEvent& ev) {
		REMOTE_HEADER head = { RCMD_REQ_LOGIN, 0 };
		rc.SendPacket(&head, NULL);
	}

	void OnClickDjLogout(wxCommandEvent& ev) {
		REMOTE_HEADER head = { RCMD_REQ_LOGOUT, 0 };
		rc.SendPacket(&head, NULL);
	}

	void OnClickDoReq(wxCommandEvent& ev) {
		if (IsConnected()) {
			wxString str = wxGetTextFromUser(_("What text do you want to display for this request?\n(Preview: Current song requested by Your Text)"), _("User Input"), _(""), this);
			if (str.length()) {
				REMOTE_HEADER head = { RCMD_SEND_REQ, strlen((const char*)str.mb_str(wxConvUTF8))+1 };
				rc.SendPacket(&head, (char*)((const char *)str.mb_str(wxConvUTF8)));
			}
		}
	}

	void OnClickShowDed(wxCommandEvent& ev) {
		if (IsConnected()) {
			wxString str = wxGetTextFromUser(_("What text do you want to display for this dedication?\n(Preview: Current song is dedicated to: Your Text)"), _("User Input"), _(""), this);
			if (str.length()) {
				REMOTE_HEADER head = { RCMD_SEND_DED, strlen((const char*)str.mb_str(wxConvUTF8))+1 };
				rc.SendPacket(&head, (char*)((const char *)str.mb_str(wxConvUTF8)));
			}
		}
	}

	void OnDClickReqList(wxCommandEvent& ev) {
		wxListBox * lb = (wxListBox *)this->FindWindowByName(_("IDC_REQUESTS"), this);
		if (lb) {
			int sel = lb->GetSelection();
			if (sel >= 0) {
				if ((sel%2) != 0) { sel--; }
				lb->Delete(sel);
				lb->Delete(sel);
			}
		}
	}
	void OnClickClearReq(wxCommandEvent& ev) {
		wxListBox * lb = (wxListBox *)this->FindWindowByName(_("IDC_REQUESTS"), this);
		if (lb) {
			lb->Clear();
		}
	}

#if defined(WIN32)
	WXLRESULT MSWWindowProc(WXUINT message, WXWPARAM wParam, WXLPARAM lParam) {
		if (message == WM_NCLBUTTONDOWN) {
			if (wParam == HTMINBUTTON) {
				Iconize(true);
			}
		}
		if (message == WM_SYSCOMMAND) {
			if (wParam == SC_MINIMIZE) {
				Iconize(true);
			}
		}
		return wxDialog::MSWWindowProc(message, wParam, lParam);
	}
#endif

	void OnClickSendReq(wxCommandEvent& ev) {
		if (IsConnected()) {
			wxString str = wxGetTextFromUser(_("What song do you want to request?"), _("User Input"), _(""), this);
			if (str.length()) {
				REMOTE_HEADER head = { RCMD_REQ, strlen((const char*)str.mb_str(wxConvUTF8))+1 };
				rc.SendPacket(&head, (char*)((const char *)str.mb_str(wxConvUTF8)));
			}
		}
	}

	void OnClickCountMeIn(wxCommandEvent& ev) {
		REMOTE_HEADER head = { RCMD_SRC_COUNTDOWN, 0 };
		rc.SendPacket(&head, NULL);
	}

	void OnClickForceOff(wxCommandEvent& ev) {
		REMOTE_HEADER head = { RCMD_SRC_FORCE_OFF, 0 };
		rc.SendPacket(&head, NULL);
	}

	void OnClickForceOn(wxCommandEvent& ev) {
		REMOTE_HEADER head = { RCMD_SRC_FORCE_ON, 0 };
		rc.SendPacket(&head, NULL);
	}

	void OnClickReload(wxCommandEvent& ev) {
		REMOTE_HEADER head = { RCMD_SRC_RELOAD, 0 };
		rc.SendPacket(&head, NULL);
	}

	void OnClickAbout(wxCommandEvent& ev) {
		OpenAboutBox(this);
	}

	void OnClickOptions(wxCommandEvent& ev) {
		OpenOptions(this);
		if (config.keep_on_top) {
			this->SetWindowStyle(this->GetWindowStyle() | wxSTAY_ON_TOP);
		} else {
			this->SetWindowStyle(this->GetWindowStyle() & ~wxSTAY_ON_TOP);
		}
	}

	void OnClickNext(wxCommandEvent& ev) {
		REMOTE_HEADER head = { RCMD_SRC_NEXT, 0 };
		rc.SendPacket(&head, NULL);
	}

	void OnClickBroadcast(wxCommandEvent& ev) {
		if (IsConnected()) {
			wxString str = wxGetTextFromUser(_("What text would you like to broadcast?"), _("User Input"), _(""), this);
			if (str.length()) {
				REMOTE_HEADER head = { RCMD_BROADCAST_MSG, strlen((const char*)str.mb_str(wxConvUTF8))+1 };
				rc.SendPacket(&head, (char*)((const char *)str.mb_str(wxConvUTF8)));
			}
		}
	}

	void OnClickDoSpam(wxCommandEvent& ev) {
		REMOTE_HEADER head = { RCMD_DOSPAM, 0 };
		rc.SendPacket(&head, NULL);
	}

	void OnClickDie(wxCommandEvent& ev) {
		REMOTE_HEADER head = { RCMD_DIE, 0 };
		rc.SendPacket(&head, NULL);
	}

	void OnClickRestart(wxCommandEvent& ev) {
		REMOTE_HEADER head = { RCMD_RESTART, 0 };
		rc.SendPacket(&head, NULL);
	}

private:
	DECLARE_EVENT_TABLE()
};

BEGIN_EVENT_TABLE(MainDlg, wxDialog)
	EVT_INIT_DIALOG(MainDlg::InitDialog)
//#ifdef WIN32
	//EVT_ICONIZE(MainDlg::OnMinimize)
	//EVT_SIZE(MainDlg::OnSize)
//#endif
	EVT_CLOSE(MainDlg::OnQuit)

	EVT_BUTTON(XRCID("IDC_LOGIN"), MainDlg::OnClickLogin)
	EVT_BUTTON(XRCID("IDC_SERVERS"), MainDlg::OnClickConnections)

	EVT_BUTTON(XRCID("IDC_DJ_LOGIN"), MainDlg::OnClickDjLogin)
	EVT_BUTTON(XRCID("IDC_DJ_LOG_OUT"), MainDlg::OnClickDjLogout)
	EVT_BUTTON(XRCID("IDC_DO_REQ"), MainDlg::OnClickDoReq)
	EVT_BUTTON(XRCID("IDC_SHOW_DED_TO"), MainDlg::OnClickShowDed)
	EVT_BUTTON(XRCID("IDC_CLEAR_REQ"), MainDlg::OnClickClearReq)

	EVT_BUTTON(XRCID("IDC_SEND_REQ"), MainDlg::OnClickSendReq)
	EVT_BUTTON(XRCID("IDC_COUNT"), MainDlg::OnClickCountMeIn)
	EVT_BUTTON(XRCID("IDC_FORCE_OFF"), MainDlg::OnClickForceOff)
	EVT_BUTTON(XRCID("IDC_FORCE_ON"), MainDlg::OnClickForceOn)
	EVT_BUTTON(XRCID("IDC_RELOAD"), MainDlg::OnClickReload)
	EVT_BUTTON(XRCID("IDC_ABOUT"), MainDlg::OnClickAbout)
	EVT_BUTTON(XRCID("IDC_OPTIONS"), MainDlg::OnClickOptions)

	EVT_BUTTON(XRCID("IDC_NEXT"), MainDlg::OnClickNext)
	EVT_BUTTON(XRCID("IDC_BCAST"), MainDlg::OnClickBroadcast)
	EVT_BUTTON(XRCID("IDC_DOSPAM"), MainDlg::OnClickDoSpam)
	EVT_BUTTON(XRCID("IDC_DIE"), MainDlg::OnClickDie)
	EVT_BUTTON(XRCID("IDC_RESTART"), MainDlg::OnClickRestart)

	EVT_LISTBOX_DCLICK(XRCID("IDC_REQUESTS"), MainDlg::OnDClickReqList)
END_EVENT_TABLE()

MainDlg * dlg = NULL;
wxDialog * GetMainDlg() { return dlg; }

void SchedConnect(int newindex) {
	wxButton * com = (wxButton *)dlg->FindWindowByName(_("IDC_LOGIN"), dlg);
	if (com) {
		com->Enable(false);
		conindex = newindex;
		doConnect = true;
	}
}

//EVT_BUTTON(XRCID("IDC_SERVERS"), MainDlg::OnClickConnections)

void DoConnect(int newindex) {
	if (IsConnected()) {
		DoDisconnect();
	}

	if (!strlen(config.hosts[newindex].name) || !strlen(config.hosts[newindex].host) || !strlen(config.hosts[newindex].username) || !strlen(config.hosts[newindex].pass)) {
		wxButton * com = (wxButton *)dlg->FindWindowByName(_("IDC_LOGIN"), dlg);
		if (com) {
			com->Enable(true);
		}
		if (wxMessageBox(_("You have not set up your connection!\n\nWould you like to open the Connection Manager now?"), _("Error"), wxYES_NO|wxICON_ERROR) == wxYES) {
#if WX_OLD
			wxCommandEvent evt(wxEVT_COMMAND_BUTTON_CLICKED, XRCID("IDC_SERVERS"));
			dlg->AddPendingEvent(evt);
#else
			wxCommandEvent * evt = new wxCommandEvent(wxEVT_COMMAND_BUTTON_CLICKED, XRCID("IDC_SERVERS"));
			wxQueueEvent(dlg->GetEventHandler(), evt);
#endif
			//dlg->QueueEvent(evt);
			//OpenConnManager(dlg);
		}
		return;
	}

	wxButton * com = (wxButton *)dlg->FindWindowByName(_("IDC_LOGIN"), dlg);
	wxStaticBitmap * img = (wxStaticBitmap *)dlg->FindWindowByName(_("IDC_STATUS_PIC"), dlg);
	if (com) {
		if (img) {
			hMutex.Lock();
			debug_printf("ConxLock\n");
			img->SetBitmap(StatusPics(1));
			com->SetLabel(_("Connecting..."));
			config.index = newindex;
			if (!config.hosts[config.index].port) {
				config.hosts[config.index].port = 10000;
			}
			debug_printf("Attempting connection to %s:%u, un: %s, mode: %u\n", config.hosts[config.index].host, config.hosts[config.index].port, config.hosts[config.index].username, config.hosts[config.index].mode);
			rc.SetClientName("ShoutIRC.com DJ Client " VERSION " (libremote/" LIBREMOTE_VERSION ")");
			if (rc.Connect(config.hosts[config.index].host, config.hosts[config.index].port, config.hosts[config.index].username, config.hosts[config.index].pass, config.hosts[config.index].mode)) {
				debug_printf("Connected\n");
				if (rc.ConnectedWithSSL()) {
					img->SetBitmap(StatusPics(3));
				} else {
					img->SetBitmap(StatusPics(2));
				}
				com->SetLabel(_("Connected"));
				/*
				if (rc.IsIRCBotv5()) {
					if (rc.GetUserFlags() & (UFLAG_MASTER|UFLAG_OP)) {
						dlg->ShowAdminPanel();
					}
				} else if (rc.GetUserLevel() <= 2) {
					dlg->ShowAdminPanel();
				}
				*/
			} else {
				debug_printf("Error connecting\n");
				rc.Disconnect();
				debug_printf("Post-DC\n");
				img->SetBitmap(StatusPics(0));
				com->SetLabel(_("Disconnected"));
			}
			com->Enable(true);
			debug_printf("ConxRelease\n");
			hMutex.Release();
		} else {
			wxMessageBox(_("Error finding image IDC_STATUS_PIC"), _("Error"));
		}
	} else {
		wxMessageBox(_("Error finding button IDC_LOGIN"), _("Error"));
	}
}

bool IsConnected() {
	return rc.IsReady();
}

void DoDisconnect() {
	if (!IsConnected()) { return; }

	if (!shutdown_now) {
		//dlg->HideAdminPanel();
		wxButton * com = (wxButton *)dlg->FindWindowByName(_("IDC_LOGIN"), dlg);
		wxStaticBitmap * img = (wxStaticBitmap *)dlg->FindWindowByName(_("IDC_STATUS_PIC"), dlg);
		if (com) {
			if (img) {
				hMutex.Lock();
				img->SetBitmap(StatusPics(1));
				com->SetLabel(_("Disconnecting..."));
				rc.Disconnect();
				com->SetLabel(_("Disconnected"));
				img->SetBitmap(StatusPics(0));
				com->Enable(true);
				hMutex.Release();
			} else {
				wxMessageBox(_("Error finding image IDC_STATUS_PIC"), _("Error"));
			}
		} else {
			wxMessageBox(_("Error finding button IDC_LOGIN"), _("Error"));
		}
	} else {
		rc.Disconnect();
	}
}

int status_printf(const char * fmt, ...) {
	va_list va;
	va_start(va, fmt);
	char buf[4096];
	vsnprintf(buf, sizeof(buf)-1, fmt, va);
	buf[4095] = 0;
	if (!shutdown_now) {
		wxStaticText * com = (wxStaticText *)dlg->FindWindowByName(_("IDC_RESPONSE"), dlg);
		if (com) {
			wxString str(buf, wxConvUTF8);
			com->SetLabel(str);
			//com->Set
		}
	}
	va_end(va);
	return 1;
}

int remote_printf(void *uptr, const char * fmt, ...) {
	va_list va;
	va_start(va, fmt);
	char buf[4096];
	vsnprintf(buf, sizeof(buf)-1, fmt, va);
	buf[4095] = 0;
	if (!shutdown_now) {
		wxStaticText * com = (wxStaticText *)dlg->FindWindowByName(_("IDC_RESPONSE"), dlg);
		if (com) {
			wxString str(buf, wxConvUTF8);
			com->SetLabel(str);
			//com->Set
		}
	}
	va_end(va);
	return 1;
}

int debug_printf(char * fmt, ...) {
	va_list va;
	va_start(va, fmt);
	char buf[8192];
	memset(&buf, 0, sizeof(buf));
	vsnprintf(buf, sizeof(buf)-1, fmt, va);
	va_end(va);
	printf("%s", buf);
	if (debug) {
		ldc_SendString(buf);
	}
	return 1;
}

class MyApp: public wxApp
{
	virtual bool OnInit() {
		dsl_init();
#if !defined(WIN32)
		signal(SIGSEGV, SIG_IGN);
		signal(SIGPIPE, SIG_IGN);
#endif

		memset(&config, 0, sizeof(config));
		if (this->argc > 1) {
#if WX_OLD
			if (!stricmp((char *)argv[1], "--debug")) {
#else
			if (!stricmp((const char *)argv[1].mb_str(wxConvUTF8), "--debug")) {
#endif
				debug = true;
			}
		}
		if (debug) {
			ldc_Init();
		}
		debug_printf("wxApp::Init()\n");
		debug_printf("Loading config\n");
		if (!LoadConfig()) {
			config.beep_on_req = true;
		}

		wxInitAllImageHandlers();
		wxXmlResource::Get()->InitAllHandlers();
#if defined(DEBUG)
		wxXmlResource::Get()->Load(_("client_wdr.xrc"));
#else
		CL3_InitXmlResource();
#endif
		rc.SetOutput(remote_printf);
		dlg = new MainDlg;
//		pDlg = dlg;
		//(MainDlg *)wxXmlResource::Get()->LoadDialog(NULL, "MainDlg2");
		wxXmlResource::Get()->LoadDialog(dlg, NULL, _("MainDlg2"));
		if (dlg) {
			debug_printf("Main dialog loaded, opening...\n");
			dlg->Show(true);
			SetTopWindow(dlg);
			TT_StartThread((ThreadProto)ConxThread, NULL, "Connection Manager");
			//dlg->Destroy();
		} else {
			wxMessageBox(_("Error loading dialog from XRC"), _("Fatal Error"));
			return false;
		}
		return true;
	}

	virtual int OnExit() {
		debug_printf("wxApp::OnExit()\n");

		//DoDisconnect();
		shutdown_now = true;
		disconnect = true;
		while (TT_NumThreads()) {
			safe_sleep(1);
		}
		debug_printf("Saving config...\n");
		SaveConfig();
		debug_printf("Bye\n");
		if (debug) {
			ldc_Quit();
		}
		dsl_cleanup();
		return 0;
	}
};

IMPLEMENT_APP(MyApp)

/*
int WINAPI WinMain(HINSTANCE hInstance, HINSTANCE hPrevInstance, LPSTR lpCmdLine, int nCmdShow) {
	MyApp * myapp = new MyApp;
	Sleep(5000);
	return 0;
}
*/

TT_DEFINE_THREAD(ConxThread) {
	TT_THREAD_START

	char buf[4096];
	REMOTE_HEADER head;
	timeval tv;
	int n=0;
	time_t lastSU = 0;
	while (!shutdown_now) {
		hMutex.Lock();
		if (disconnect) {
			DoDisconnect();
			disconnect = false;
		}
		if (doConnect) {
			DoConnect(conindex);
			doConnect = false;
		}
		if (IsConnected()) {
			if ((time(NULL) - lastSU) > 30) {
				lastSU = time(NULL);
				head.ccmd = RCMD_QUERY_STREAM;
				head.datalen = 0;
				rc.SendPacket(&head, NULL);
			}
			tv.tv_sec = 1;
			tv.tv_usec = 0;
			n = rc.WaitForData(&tv);
			switch(n) {
				case 1:
					if (rc.RecvPacket(&head, buf, sizeof(buf))) {
						switch(head.scmd) {
							case RCMD_REQ_LOGOUT_ACK:
								status_printf("You are no longer the active DJ");
								break;
							case RCMD_REQ_LOGIN_ACK:
								status_printf("You are now the active DJ");
								break;
							case RCMD_REQ_INCOMING:{
									char * p2 = NULL;
									char * p = strtok_r(buf, "\xFE", &p2);
									wxListBox * lb = (wxListBox *)dlg->FindWindowByName(_("IDC_REQUESTS"), dlg);
									if (lb) {
										while (p) {
											wxString str(p, wxConvUTF8);
											lb->Append(str);
											p = strtok_r(NULL, "\xFE", &p2);
										}
#if defined(WIN32)
										#pragma comment(lib, "winmm.lib")
										if (config.beep_on_req) {
											if (access("incoming_req.wav",0) == 0) {
												PlaySound(TEXT("incoming_req.wav"), NULL, SND_FILENAME|SND_ASYNC);
											} else {
												PlaySound(MAKEINTRESOURCE(IDR_WAVE1), GetModuleHandle(NULL), SND_RESOURCE|SND_ASYNC);
											}
										}
#endif
									}
									//status_printf("You are now the active DJ");
								}
								break;
							case RCMD_STREAM_INFO:{
									STREAM_INFO * si = (STREAM_INFO *)buf;
									wxStaticText * txt = (wxStaticText *)dlg->FindWindowByName(_("IDC_TITLE"), dlg);
									if (txt) {
										wxString str(si->title, wxConvUTF8);
										txt->SetLabel(str);
									}
									txt = (wxStaticText *)dlg->FindWindowByName(_("IDC_OTHER"), dlg);
									if (txt) {
										char * tmp = zmprintf("Listeners: %d/%d, Peak: %d", si->listeners, si->max, si->peak);
										wxString str(tmp, wxConvUTF8);
										txt->SetLabel(str);
										zfree(tmp);
									}
								}
								break;
							case RCMD_GENERIC_MSG:{
									if (strlen(buf)) {
										status_printf("%s", buf);
									}
								}
								break;
							case RCMD_GENERIC_ERROR:{
									if (strlen(buf)) {
										status_printf("%s", buf);
										//wxMessageBox(_("Error finding button IDC_LOGIN"), _("Error"), wxICON_ERROR|wxOK);
									}
								}
								break;
						}
					} else {
						DoDisconnect();
					}
					break;
				case -1:{
						DoDisconnect();
						//wxCommandEvent ev;
						//dlg->OnClickLogin(ev);
					}
					break;
				default:
					break;
			}
			hMutex.Release();
		} else {
			lastSU = 0;
			hMutex.Release();
			safe_sleep(100,true);
		}
	}
	DoDisconnect();

	TT_THREAD_END
}

