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

#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <gdk/gdkkeysyms.h>

#include "simu_connection.h"
#include "warlock.h"
#include "debug.h"
#include "preferences.h"
#include "entry.h"
#include "script.h"
#include "warlockview.h"
#include "macro.h"

#ifdef CONFIG_SPIDERMONKEY
#include "jsscript.h"
#endif

extern SimuConnection *connection;

GladeXML *warlock_xml = NULL;
int port = 0;
char *key = NULL;
char *host = NULL;

static void parse_arguments (int argc, char **argv)
{
        gboolean version = FALSE;
	const GOptionEntry entries[] = {
		{"host", 'h', 0, G_OPTION_ARG_STRING, &host,
			"Specify a host (requires key and port", "hostname"},
		{"port", 'p', 0, G_OPTION_ARG_INT, &port,
			"Specify a port (requires host and key)", "port"},
		{"key", 'k', 0, G_OPTION_ARG_STRING, &key,
			"Specify a key (requires host and port)", "key"},
		{"version", 'v', 0, G_OPTION_ARG_NONE, &version,
			"Print the version and exit", NULL},
                {NULL, 0, 0, 0, NULL, NULL, NULL}
	};

	GOptionContext *context;
        GError *err;

        context = g_option_context_new ("- A Simutronics Wizard replacement");
        g_option_context_add_main_entries (context, entries, NULL);
        g_option_context_add_group (context, gtk_get_option_group (TRUE));
        g_option_context_set_help_enabled (context, TRUE);

        err = NULL;
        g_option_context_parse (context, &argc, &argv, &err);

        /* handle parse errors */
	if (err != NULL) {
                print_error (err);
		exit (1);
	}

        /* display version and quit if we have a --version */
        if (version) {
                g_print ("Version: " VERSION "\n");
                exit (0);
        }

	/* parse the .sal file */
        if (argc > 1) {
                const char *filename;
                char *contents;

		debug ("trying to parse file\n");

                filename = argv[1];

                err = NULL;
                if(!g_file_get_contents (filename, &contents, NULL, &err)) {
                        print_error (err);
                } else {
                        char **lines;
                        int i;

                        lines = g_strsplit_set (contents, "\r\n", 0);
                        g_free (contents);
                        for (i = 0; lines[i] != NULL; i++) {
                                char **tokens;

                                tokens = g_strsplit (lines[i], "=", 2);
                                if (tokens[0] == NULL) {
                                        continue;
                                }
                                if (strcmp (tokens[0], "GAMEHOST") == 0) {
                                        if (host == NULL) {
                                                host = g_strdup (tokens[1]);
                                        }
                                } else if (strcmp (tokens[0], "GAMEPORT")
                                                == 0) {
                                        if (port == 0) {
                                                port = atoi (tokens[1]);
                                        }
                                } else if (strcmp (tokens[0], "KEY") == 0) {
                                        if (key == NULL) {
                                                key = g_strdup (tokens[1]);
                                        }
                                }
                                g_strfreev (tokens);
                        }
                        g_strfreev (lines);
                }
        }
}

int main (int argc, char *argv[])
{
        GtkWidget *main_window = NULL;
        int width, height;
	char *glade_filename;

#if 0
#ifdef ENABLE_NLS
        bindtextdomain (GETTEXT_PACKAGE, PACKAGE_LOCALE_DIR);
        bind_textdomain_codeset (GETTEXT_PACKAGE, "UTF-8");
        textdomain (GETTEXT_PACKAGE);
#endif

        gtk_set_locale ();
#endif
	if (!g_thread_supported ()) g_thread_init (NULL);
        gdk_threads_init ();
        GDK_THREADS_ENTER ();

	// calls g_option_context_parse, which initializes gtk
        parse_arguments (argc, argv);

	// initialize glade data
	glade_filename = g_build_filename (PACKAGE_DATA_DIR, PACKAGE,
			"warlock.glade", NULL);
        warlock_xml = glade_xml_new (glade_filename, NULL, NULL);
	g_free (glade_filename);
	if (warlock_xml == NULL) {
		g_printerr ("Could not find glade file.\n");
		exit (1);
	}

        warlock_init ();

        glade_xml_signal_autoconnect (warlock_xml);

        main_window = glade_xml_get_widget (warlock_xml, "main_window");

        /* read window width from config */
        width = preferences_get_int (preferences_get_key (PREF_WINDOW_WIDTH));

        /* read window height from config */
        height = preferences_get_int (preferences_get_key
                        (PREF_WINDOW_HEIGHT));

        gtk_window_set_default_size (GTK_WINDOW (main_window), width, height);

        gtk_widget_show (main_window);
	warlock_connection_init ();

        gtk_main ();

        /* save the height an width */
        gtk_window_get_size (GTK_WINDOW (main_window), &width, &height);
        preferences_set_int (preferences_get_key (PREF_WINDOW_WIDTH), width);
        preferences_set_int (preferences_get_key (PREF_WINDOW_HEIGHT), height);
        GDK_THREADS_LEAVE ();

        debug ("waiting for connection to close\n");
        if (connection != NULL && connection->thread != NULL) {
                g_thread_join (connection->thread);
        }

        return 0;
}

/************************************************************************
 * Event handlers							*
 ************************************************************************/

EXPORT
gboolean
on_main_window_delete_event            (GtkWidget       *widget,
                                        GdkEvent        *event,
                                        gpointer         user_data)
{
        warlock_exit ();
        return TRUE;
}

EXPORT
gboolean
on_main_window_key_press_event         (GtkWidget       *widget,
                                        GdkEventKey     *event,
                                        gpointer         user_data)
{
        gboolean result;

        switch (event->keyval) {
                case GDK_Escape: /* Stop currently running script */
                        if (event->state & GDK_SHIFT_MASK) {
                                script_toggle_suspend ();
#ifdef CONFIG_SPIDERMONKEY
                                js_script_toggle_all ();
#endif
                        } else {
                                script_kill ();
#ifdef CONFIG_SPIDERMONKEY
                                js_script_stop_all ();
#endif
                        }
                        return TRUE;

                case GDK_Page_Up:
                case GDK_Page_Down:
                        //gtk_widget_grab_focus (main_window->scroller);
                        g_signal_emit_by_name (warlock_view_get_scrolled_window
					("main"), "key_press_event", event,
					&result);
                        return result;

                case GDK_KP_Enter :	/* send last command entered */
                        warlock_entry_grab_focus ();
                        if (strcmp (warlock_entry_get_text (), "") != 0) {
                                warlock_entry_give_key_event (event);
                                return TRUE;
                        }

                        warlock_history_last ();
                        return TRUE;

                case GDK_Return:
                        if (event->state & (GDK_MOD1_MASK | GDK_SHIFT_MASK
                                                | GDK_CONTROL_MASK)) {
                                warlock_entry_grab_focus ();
                                warlock_history_last ();
                                return TRUE;
                        }

                        if (warlock_entry_is_focus ())
                                return FALSE;

                        warlock_entry_grab_focus ();
                        warlock_entry_give_key_event (event);
                        return TRUE;

                case GDK_Up :		/* move back in the command history */
                        warlock_entry_grab_focus ();
                        warlock_history_prev ();
                        return TRUE;

                case GDK_Down :		/* move forward in command history */
                        warlock_entry_grab_focus ();
                        warlock_history_next ();
                        return TRUE;

                default:
                        return process_macro_key (event);
        }
}

EXPORT
void
on_quit_activate                      (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
        warlock_exit ();
}

EXPORT
void
on_about_activate                     (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
	GtkWidget *about_dialog;

	about_dialog = glade_xml_get_widget (warlock_xml, "about_dialog");
	gtk_widget_show (about_dialog);
}

EXPORT
void
on_arrival_menu_item_activate          (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
        if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (menuitem))) {
                warlock_view_show ("arrival");
        } else {
                warlock_view_hide ("arrival");
        }
}

EXPORT
void
on_thoughts_menu_item_activate         (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
        if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (menuitem))) {
                warlock_view_show ("thoughts");
        } else {
                warlock_view_hide ("thoughts");
        }

}

EXPORT
void
on_familiar_menu_item_activate         (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
        if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (menuitem))) {
                warlock_view_show ("familiar");
        } else {
                warlock_view_hide ("familiar");
        }

}

EXPORT
void
on_deaths_menu_item_activate           (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
        if (gtk_check_menu_item_get_active (GTK_CHECK_MENU_ITEM (menuitem))) {
                warlock_view_show ("deaths");
        } else {
                warlock_view_hide ("deaths");
        }
}

EXPORT
gboolean
on_main_window_key_press_default_event (GtkWidget *widget, GdkEventKey *event,
                gpointer user_data)
{
        if (warlock_entry_is_focus () ||
                        event->keyval == GDK_Alt_R ||
                        event->keyval == GDK_Alt_L ||
                        event->keyval == GDK_Control_R ||
                        event->keyval == GDK_Control_L ||
                        event->keyval == GDK_Shift_R ||
                        event->keyval == GDK_Shift_L) {
                return FALSE;
        }

        warlock_entry_grab_focus ();
        warlock_entry_set_position (-1);

        return warlock_entry_give_key_event (event);
}

EXPORT
void
on_preferences_menu_item_activate (GtkMenuItem *menuitem, gpointer user_data)
{
        GtkWidget *dialog;

        dialog = glade_xml_get_widget (warlock_xml, "preferences_dialog");
        gtk_widget_show (dialog);
}

EXPORT
void
on_text_strings_menu_item_activate (GtkMenuItem *menuitem, gpointer user_data)
{
        GtkWidget *dialog;

        dialog = glade_xml_get_widget (warlock_xml, "text_strings_dialog");
        gtk_widget_show (dialog);
}

EXPORT
void
on_macros_menu_item_activate (GtkMenuItem *menuitem, gpointer user_data)
{
        GtkWidget *dialog;

        dialog = glade_xml_get_widget (warlock_xml, "macros_dialog");
        gtk_widget_show (dialog);
}

EXPORT
void
on_execute_menu_item_activate          (GtkMenuItem     *menuitem,
                                        gpointer         user_data)
{
        GtkWidget *file_chooser;
        GtkFileFilter *wizard_filter, *all_filter;
        int response;
        char *script_path;
        char *key;

        file_chooser = gtk_file_chooser_dialog_new ("Select script to run",
                        GTK_WINDOW (glade_xml_get_widget (warlock_xml,
                                        "main_window")),
                        GTK_FILE_CHOOSER_ACTION_OPEN,
                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                        GTK_STOCK_OPEN, GTK_RESPONSE_ACCEPT,
                        NULL);

        key = preferences_get_key (PREF_SCRIPT_PATH);
        script_path = preferences_get_string (key);
        gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (file_chooser),
                        script_path);
        g_free (script_path);
        g_free (key);

        wizard_filter = gtk_file_filter_new ();
        gtk_file_filter_set_name (wizard_filter, "Wizard Scripting files");
        gtk_file_filter_add_pattern (wizard_filter, "*.[Cc][Mm][Dd]");
        gtk_file_filter_add_pattern (wizard_filter, "*.[Ww][Ii][Zz]");
        gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (file_chooser),
                        wizard_filter);

        all_filter = gtk_file_filter_new ();
        gtk_file_filter_set_name (all_filter, "All files");
        gtk_file_filter_add_pattern (all_filter, "*");
        gtk_file_chooser_add_filter (GTK_FILE_CHOOSER (file_chooser),
                        all_filter);

        response = gtk_dialog_run (GTK_DIALOG (file_chooser));

        if (response == GTK_RESPONSE_ACCEPT) {
                char *filename;

                filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER
                                (file_chooser));
                script_load (filename, 0, NULL);
                g_free (filename);
        }

        gtk_widget_destroy (file_chooser);

}

EXPORT
void on_stop_menu_item_activate (GtkMenuItem *menuitem, gpointer user_data)
{
        script_kill ();
}

EXPORT
void on_connections_activate (GtkMenuItem *menuitem, gpointer user_data)
{
        gtk_widget_show (glade_xml_get_widget (warlock_xml, "profile_dialog"));
}

EXPORT
void on_disconnect_activate (GtkMenuItem *menuitem, gpointer user_data)
{
        warlock_disconnect ();
}

EXPORT
void on_connect_activate (GtkMenuItem *menuitem, gpointer user_data)
{
        warlock_connection_init ();
}
