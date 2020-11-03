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

/*** libSkypeAPI: Skype API Library ***

Written by Indy Sams (indy@driftsolutions.com)
Based on the Skype API Plugin for Pidgin/libpurple/Adium by Eion Robb <http://myjobspace.co.nz/images/pidgin/>

This file is part of the libSkypeAPI library, for licensing see the LICENSE file that came with this package.

***/

#include "skype.h"
#if defined(UNIX)

int x11_error_handler(Display *disp, XErrorEvent *error);
void skype_x11_receive_thread(void);

int x11_error_handler(Display *disp, XErrorEvent *error) {
        h.x11_error_code = error->error_code;
        return FALSE;
}

bool skype_x11_connect() {

	if (!g_thread_supported()) {
		g_thread_init (NULL);
	}
	Window root;
    Atom skype_inst;
    Atom type_ret;
    int format_ret;
    unsigned long nitems_ret;
    unsigned long bytes_after_ret;
    unsigned char *prop;
    int status;

	h.client->status_cb(SAPI_STATUS_CONNECTING);

    XSetErrorHandler(x11_error_handler);
    h.disp = XOpenDisplay(getenv("DISPLAY"));
    if (h.disp == NULL) {
		printf("Skype-X11: Couldn't open display\n");
		return false;
    }
    h.message_start = XInternAtom( h.disp, "SKYPECONTROLAPI_MESSAGE_BEGIN", False );
    h.message_continue = XInternAtom( h.disp, "SKYPECONTROLAPI_MESSAGE", False );
    root = DefaultRootWindow( h.disp );
    h.win = XCreateSimpleWindow( h.disp, root, 0, 0, 1, 1, 0, BlackPixel( h.disp, DefaultScreen( h.disp ) ), BlackPixel( h.disp, DefaultScreen( h.disp ) ));
    XFlush(h.disp);
    if (h.win == -1) {
		printf("Skype-X11: Could not create X11 messaging window\n");
		return false;
    }
    skype_inst = XInternAtom(h.disp, "_SKYPE_INSTANCE", True);
    if (skype_inst == None) {
		h.skype_win = (Window)-1;
		printf("Skype-X11: Could not create skype Atom\n");
		return false;
    }
    status = XGetWindowProperty(h.disp, root, skype_inst, 0, 1, False, XA_WINDOW, &type_ret, &format_ret, &nitems_ret, &bytes_after_ret, &prop);
    if(status != Success || format_ret != 32 || nitems_ret != 1) {
		h.skype_win = (Window)-1;
		printf("Skype-X11: Skype instance not found\n");
		return false;
    }
    h.skype_win = * (const unsigned long *) prop & 0xffffffff;

	h.loop_running = true;
    g_thread_create((GThreadFunc)skype_x11_receive_thread, NULL, FALSE, NULL);

	printf("Skype-X11: Connection completed, sending authorization request...\n");

	char buf[1024];
	sprintf(buf, "NAME %s", h.client->app_name);
	skype_x11_send_command(buf);
	sprintf(buf, "PROTOCOL %d", h.client->protocol_version);
	skype_x11_send_command(buf);
	h.pending = true;
	h.client->status_cb(SAPI_STATUS_PENDING_AUTHORIZATION);
	return true;
}

bool skype_x11_send_command(char * message) {
	unsigned int pos = 0;
    unsigned int len = strlen( message );
    XEvent e;
    int message_num;
    char error_return[15];
    unsigned int i;

    if (h.quit || h.skype_win == -1 || h.win == -1 || h.disp == NULL) {
        //There was an error
		printf("Skype-X11: Not Connected or Disconnecting\n");
        return false;
    }

    memset(&e, 0, sizeof(e));
    e.xclient.type = ClientMessage;
    e.xclient.message_type = h.message_start;
    e.xclient.display = h.disp;
    e.xclient.window = h.win;
    e.xclient.format = 8;

    do
    {
            for( i = 0; i < 20 && i + pos <= len; ++i )
                    e.xclient.data.b[ i ] = message[ i + pos ];
            XSendEvent( h.disp, h.skype_win, False, 0, &e );

            e.xclient.message_type = h.message_continue;
            pos += i;
    } while( pos <= len );

    //XFlush(disp);

    if (h.x11_error_code == BadWindow) {
		//There was an error
		printf("Skype-X11: X11 Error %u\n", h.x11_error_code);
		return false;
    }

	return true;
}

void skype_x11_disconnect() {

	XEvent * e = g_new0(XEvent, 1);
    e->xclient.type = DestroyNotify;
    XSendEvent(h.disp, h.win, False, 0, e);

	while (h.loop_running) {
		usleep(10000);
	}

    XDestroyWindow(h.disp, h.win);
    XCloseDisplay(h.disp);
}

void skype_x11_receive_thread(void) {
    XEvent e;
    GString *msg = NULL;
    char msg_temp[21];
    size_t len;
    Bool event_bool;

    msg_temp[20] = '\0';
    while (!h.quit) {
        if (!h.disp) {
			printf("Skype-X11: display has disappeared\n");
			h.client->status_cb(SAPI_STATUS_DISCONNECTED);
            break;
        }
        event_bool = XCheckTypedEvent(h.disp, ClientMessage, &e);
        if (!event_bool) {
			usleep(1000);
			continue;
        }

        strncpy(msg_temp, e.xclient.data.b, 20);
        len = strlen(msg_temp);
		if (e.xclient.message_type == h.message_start) {
			msg = g_string_new_len(msg_temp, len);
		} else if (e.xclient.message_type == h.message_continue) {
			msg = g_string_append_len(msg, msg_temp, len);
		} else {
			printf("Skype-X11: unknown message type: %d\n", e.xclient.message_type);
			XFlush(h.disp);
			continue;
        }

        if (len < 20) {
			if (h.pending) {
				if (!strcmp(msg->str, "OK")) {
					h.pending = false;
					h.client->status_cb(SAPI_STATUS_CONNECTED);
				} else {
					h.client->status_cb(SAPI_STATUS_ERROR);
				}
			} else {
				h.client->message_cb(msg->str);
			}
			g_string_free(msg, TRUE);
			msg = NULL;
			XFlush(h.disp);
		}
    }

	if (msg) {
		g_string_free(msg, TRUE);
	}

	h.loop_running = false;
}

#endif
