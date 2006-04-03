/* Warlock Front End
 * Copyright 2005 Sean Proctor, Marshall Culpepper
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
 * Foundation, Inc., 59 Temple Place - Suite 330, Boston, MA 02111-1307, USA
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gprintf.h>

#include "highlight.h"
#include "simu_connection.h"
#include "status.h"
#include "warlock.h"
#include "warlockview.h"
#include "preferences.h"
#include "text_strings_dialog.h"
#include "hand.h"
#include "compass.h"
#include "status.h"
#include "entry.h"
#include "warlocktime.h"
#include "preferences_dialog.h"
#include "profile_dialog.h"
#include "debug.h"
#include "macro.h"
#include "macro_dialog.h"
#include "script.h"
#include "log.h"

#ifdef CONFIG_SPIDERMONKEY
#include "jsscript.h"
#endif

#ifndef VERSION
#  error "No version specified"
#endif

extern int wizardparse (void);
extern int wizard_scan_string(const char*);

extern GladeXML *warlock_xml;
extern char *host;
extern char *key;
extern int port;

/* global variables */
SimuConnection *connection = NULL;
gchar *drag_obj = NULL;

/* local variables */
static gboolean warlock_quit = FALSE;

void warlock_send (const char *fmt, ...)
{
        va_list list;
        char *string;
        char *to_send;

        g_assert (fmt != NULL);

        va_start (list, fmt);
        g_vasprintf (&string, fmt, list);
        va_end (list);

        to_send = g_strconcat (string, "\n", NULL);
        g_free (string);

        if (preferences_get_bool (preferences_get_key (PREF_ECHO))) {
                echo (to_send);
        }

	if (connection != NULL) {
		simu_connection_send (connection, to_send);
	}

        g_free (to_send);
}

void
warlock_disconnect (void)
{
        // FIXME timeout after a while and force a quit.

        if (connection != NULL) {
                warlock_send ("quit\n");
        }
}

/* must be called from within gdk threads context */
void
warlock_exit (void)
{
        debug ("exiting warlock\n");

        warlock_quit = TRUE;
        if (connection != NULL) {
                debug ("wait for disconnect\n");
                warlock_disconnect ();
        } else {
                debug ("quit now!\n");
                gtk_main_quit ();
        }
}

void warlock_bell (void)
{
        g_printerr ("\a");
}

void warlock_init (void)
{
#ifdef DEBUG
        g_log_set_always_fatal (G_LOG_LEVEL_CRITICAL | G_LOG_LEVEL_WARNING);
#endif
        preferences_init ();
        highlight_init ();
        preferences_dialog_init ();
        profile_dialog_init ();
        text_strings_dialog_init ();
        hand_init (glade_xml_get_widget (warlock_xml, "left_hand_label"),
                        glade_xml_get_widget (warlock_xml, "right_hand_label"));
        status_init (glade_xml_get_widget (warlock_xml, "status_table"));
        compass_init (glade_xml_get_widget (warlock_xml, "compass_table"));
        warlock_entry_init (glade_xml_get_widget (warlock_xml,
                                "warlock_entry"));
        macro_init ();
        macro_dialog_init ();
        warlock_time_init ();
        warlock_views_init ();
	warlock_log_init ();
        script_init ();

#ifdef CONFIG_SPIDERMONKEY
        if (!js_script_init ()) {
                g_error ("Error initializing javascript runtime");
        }
#endif
}

static void
handle_quit (SimuQuit reason, GError *err, gpointer data)
{
        gdk_threads_enter ();

        debug ("handle quit was called\n");
        switch (reason) {
                case SIMU_EOF:
                        echo ("*** Connection closed by server ***\n");
                        break;

                case SIMU_ERROR:
                        if (err != NULL) {
                                echo_f ("*** Disconnected from server"
						" due to an error %d: %s ***\n",
						err->code, err->message);
                        } else {
                                echo ("*** Disconnected from server due to an "
                                                "unknown error ***\n");
                        }
                        break;

                case SIMU_QUIT:
                        echo ("*** Conection closed ***\n");
                        break;

                default:
                        g_assert_not_reached ();
        }

	g_free (connection);
        connection = NULL;

        if (warlock_quit) {
		warlock_exit ();
        }

        gdk_threads_leave ();
}

static gboolean
handle_line (gchar *string, gpointer data)
{
        int result;

        gdk_threads_enter ();
        wizard_scan_string (string);
        result = wizardparse();
        gdk_threads_leave ();

        switch (result) {
                case 0:
                        return TRUE;
                case 1:
                        // exit the game
                        return FALSE;
                case -1:
                        g_printerr ("failed to parse this string: %s\n",
                                        string);
                        g_assert_not_reached ();
                        return FALSE;
                default:
                        g_assert_not_reached ();
                        return FALSE;
        }
}

static void
handle_init (gpointer data)
{
        GtkWidget *dialog;

	g_assert (connection != NULL);

	simu_connection_send (connection, g_strdup_printf("%s\n", key));
	simu_connection_send (connection, "/FE:WIZARD\n");

        gdk_threads_enter ();
        dialog = data;
	gtk_widget_hide (dialog);
        gdk_threads_leave ();
}

void warlock_connection_init (void)
{
        GtkWidget *connect_dialog;

        debug ("connecting to server\n");

        if (connection != NULL) {
                echo ("*** You are already connected ***\n");
                return;
        }

        if (host == NULL || key == NULL || port == 0 || connection != NULL) {
                debug ("wait nevermind, we're not\n");
                if (connection == NULL) {
                        gtk_widget_show (glade_xml_get_widget (warlock_xml,
                                                "profile_dialog"));
                }
                return;
        }

	connect_dialog = glade_xml_get_widget (warlock_xml,
                        "connect_dialog");
	gtk_window_set_position (GTK_WINDOW (connect_dialog),
			GTK_WIN_POS_CENTER_ON_PARENT);
	gtk_window_set_transient_for (GTK_WINDOW (connect_dialog),
			GTK_WINDOW (glade_xml_get_widget (warlock_xml,
                                        "main_window")));
	gtk_widget_show (connect_dialog);

        connection = simu_connection_init (host, port,
                        handle_init, connect_dialog,
                        handle_line, NULL,
                        handle_quit, NULL);
}
