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
#if defined(UNIX) && !defined(NO_DBUS)

static DBusHandlerResult SkypeAPI_SignalHandler(DBusConnection *connection, DBusMessage *message, gpointer user_data);
void skype_dbus_receive_thread(void);

bool skype_dbus_connect() {

	if (!g_thread_supported()) {
		g_thread_init(NULL);
	}

	DBusError err;
	DBusObjectPathVTable vtable;

	dbus_error_init(&err);

	h.dConx = dbus_bus_get(DBUS_BUS_SESSION, &err);//&error);
	if (h.dConx == NULL) {
		if (dbus_error_is_set(&err)) {
			fprintf(stderr, "Error connecting to D-BUS: %s\n", err.message);
			dbus_error_free(&err);
		} else {
			fprintf(stderr, "Error connecting to D-BUS: unknown error\n");
		}
		return false;
	}

	vtable.message_function = SkypeAPI_SignalHandler;
	if (dbus_connection_register_object_path(h.dConx, "/com/Skype/Client", &vtable, NULL) == FALSE) {
		fprintf(stderr, "Error registering object path!\n");
		return false;
	}

	h.loop_running = true;
	g_thread_create((GThreadFunc)skype_dbus_receive_thread, NULL, FALSE, NULL);

	char buf[1024];
	sprintf(buf, "NAME %s", h.client->app_name);
	skype_dbus_send_command(buf);
	sprintf(buf, "PROTOCOL %d", h.client->protocol_version);
	skype_dbus_send_command(buf);
	h.pending = true;
	h.client->status_cb(SAPI_STATUS_PENDING_AUTHORIZATION);

	return true;
}

bool skype_dbus_send_command(char * str) {
	DBusMessageIter args;

	DBusMessage * msg = dbus_message_new_method_call(SKYPE_SERVICE, SKYPE_PATH, SKYPE_INTERFACE, "Invoke");
	if (msg == NULL) {
		fprintf(stderr, "Error allocating new message!\n");
		return false;
	}

	// append arguments
	dbus_message_iter_init_append(msg, &args);
	if (!dbus_message_iter_append_basic(&args, DBUS_TYPE_STRING, &str)) {
		fprintf(stderr, "Out Of Memory!\n");
		return false;
	}

	// send message and get a handle for a reply
	if (!dbus_connection_send(h.dConx, msg, NULL)) {
		fprintf(stderr, "Out Of Memory!\n");
		return false;
	}
	//dbus_connection_flush(h.dConx);

	// free message
	dbus_message_unref(msg);
	return true;
}

void skype_dbus_disconnect() {
	dbus_connection_unregister_object_path(h.dConx, "/com/Skype/Client");
	dbus_connection_unref(h.dConx);
	while (h.loop_running) {
		usleep(10000);
	}
}

void skype_dbus_receive_thread(void) {
	while (!h.quit && dbus_connection_read_write_dispatch(h.dConx, 100) == TRUE) {;}
	if (!h.quit) {
		//we've been disconnected
		h.client->status_cb(SAPI_STATUS_DISCONNECTED);
	}
	h.loop_running = false;
}

static DBusHandlerResult SkypeAPI_SignalHandler(DBusConnection *connection, DBusMessage *message, gpointer user_data) {
	DBusMessageIter iterator;
	gchar *message_temp;

	DBusMessage * temp_message = dbus_message_ref(message);
	dbus_message_iter_init(temp_message, &iterator);
	if (dbus_message_iter_get_arg_type(&iterator) != DBUS_TYPE_STRING) {
		dbus_message_unref(message);
		return DBUS_HANDLER_RESULT_NOT_YET_HANDLED;
	}

    do
    {
		dbus_message_iter_get_basic(&iterator, &message_temp);
		if (h.pending) {
			h.pending = false;
			h.client->status_cb(SAPI_STATUS_CONNECTED);
		}
		h.client->message_cb(message_temp);
    } while(dbus_message_iter_has_next(&iterator) && dbus_message_iter_next(&iterator));

	dbus_message_unref(message);

	return DBUS_HANDLER_RESULT_HANDLED;
}

#endif // UNIX
