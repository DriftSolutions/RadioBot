/*   FILE: monitor.c -- 
 * AUTHOR: Baruch Even <baruch.even@writeme.com>
 *   DATE: 25 September 2000
 *
 * Copyright (c) 2000 Baruch Even <baruch.even@writeme.com>
 * All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

/* ======================================================================== */

/* I disable it since its incomplete */
#ifdef ENABLE_MONITOR

#include "config.h"
#include "support.h"

#include <glib.h>
#include <gdk/gdk.h>
#include <gtk/gtk.h>

struct _Monitor {
	GtkWidget *window;
	GdkPixmap *pixmap;
	GtkWidget **plots;
	GtkPlotDate *data;
	GtkWidget *canvas;
};

/* Create the monitor */
Monitor * monitor_create(void)
{
	Monitor * monitor = NULL;
	GtkWidget * widget = NULL;
	
	monitor = g_malloc( sizeof(Monitor) );
	if (monitor == NULL)
		return NULL;

	monitor->window = 
		monitor->pixmap = 
		monitor->plots = 
		monitor->data = 
		monitor->canvas = NULL;

	monitor->window = widget = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(widget), 
			_("Volume Normalizing plugin Monitor"));

}

/* Add data points */
void monitor_add_data(Monitor * monitor, double power, double gain)
{
}

/* File change detected */
void monitor_file_change(void)
{
}

#endif
