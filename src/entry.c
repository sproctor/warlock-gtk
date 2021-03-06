/* Warlock Front End
 * Copyright (C) 2001 Chris Schantz.
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

#include <pcre.h>

#include <gtk/gtk.h>
#include <gdk/gdkkeysyms.h>

#include "script.h"
#include "debug.h"
#include "entry.h"
#include "warlock.h"
#include "simu_connection.h"
#include "preferences.h"
#include "helpers.h"
#include "status.h"
#include "warlockview.h"

static void
script_command (int argc, const char **argv);

/* external variables */
extern gchar *drag_obj;

/* local variables */
static GtkWidget *entry = NULL;
static guint pos = 0;
static GSList *command_history = NULL;
static gint min_command_size = 0;
static gint max_history_size = 0;
static guint command_size_notification = 0;
static guint history_size_notification = 0;

// removes elements far in the past so our history doesn't get out of hand
// uses the preference history-size to determine the max size
static void
trim_history (void)
{
        guint size;

        if (max_history_size < 0)
                return;

        size = g_slist_length (command_history);

        if (size > (guint)max_history_size) {
                GSList *new_last, *delete_list;

                // get what should be the last element
                new_last = g_slist_nth (command_history, max_history_size - 1);

                // if we had no last element, then we should clear the entire
                // list
                if (new_last == NULL) {
                        // delete_list is going to be deleted entirely
                        // clear the command_history
                        // this path should only be followed when history size
                        // is set to 0
                        delete_list = command_history;
                        command_history = NULL;
                } else {
                        // new_last should be at the end, so set next to NULL
                        // delete_list is to be deleted entirely
                        delete_list = new_last->next;
                        new_last->next = NULL;
                }

                // remove all items off the end of the list
                while(delete_list != NULL) {
                        delete_list = g_slist_delete_link (delete_list,
                                        delete_list);
                }

		// make sure we aren't still referring to an item off the end
		// of the list
		if (pos >= max_history_size) {
			pos = MAX (0, max_history_size);
		}
        }
}

static void
update_history (const char *string)
{
        /* if the sting is shorter than the minimum size or equal to the last
         * message, ignore it
         */
        if ((gint)strlen (string) >= (gint)min_command_size &&
			(command_history == NULL ||
			 strcmp (string, command_history->data) != 0)) {
                command_history = g_slist_prepend (command_history,
                                g_strdup(string));
                trim_history ();
        }

	// save the history
        preferences_set_list (preferences_get_key (PREF_COMMAND_HISTORY),
                        PREFERENCES_VALUE_STRING, command_history);
}

static void
send_text (const char *text)
{
	const char *script_prefix;

        pos = 0;

        if (text == NULL) {
                return;
        }

	script_prefix = preferences_get_string (preferences_get_key
                        (PREF_SCRIPT_PREFIX));

        if (script_prefix != NULL && g_str_has_prefix (text, script_prefix)) {
		char **argv;
                int argc;
                GError *err;

                err = NULL;
		argv = NULL;
		if (g_shell_parse_argv (text + strlen (script_prefix), &argc,
                                &argv, &err)) {
			print_error (err);
			g_assert (err == NULL);
			script_command (argc, (const char**)argv);
		} else {
			echo_f ("*** Couldn't parse script line due to error %d"
					": %s **\n", err->code, err->message);
		}
		if (argv != NULL)
			g_strfreev (argv);
	} else {
                gboolean auto_sneak;

                auto_sneak = preferences_get_bool (preferences_get_key
                                (PREF_AUTO_SNEAK));

                if (auto_sneak && is_hidden () && is_direction (text)) {
                        warlock_send ("sneak %s", text);
                } else if (drag_obj != NULL && !is_hidden ()) {
                        if (strncmp(text, "go ", 3) == 0) {
                                warlock_send ("drag %s%s", drag_obj, text + 2);
                        } else if (is_direction (text)) {
                                warlock_send ("drag %s %s", drag_obj, text);
                        }
                } else {
                        warlock_send ("%s", text);
                }
        }

        update_history (text);
}

void
warlock_entry_append (char *str)
{
	int n = -1;
	gtk_editable_insert_text (GTK_EDITABLE (entry), str, -1, &n);
}

void
warlock_entry_append_c (char c)
{
	int n = -1;
	gtk_editable_insert_text (GTK_EDITABLE (entry), &c, 1, &n);
}

gboolean
warlock_entry_is_focus (void)
{
	return gtk_widget_is_focus (entry);
}

void
warlock_entry_grab_focus (void)
{
        if (!gtk_widget_is_focus (entry)) {
                gtk_widget_grab_focus (entry);
        }
}

int
warlock_entry_get_position (void)
{
	return gtk_editable_get_position (GTK_EDITABLE (entry));
}

void
warlock_entry_set_position (int position)
{
	gtk_editable_set_position (GTK_EDITABLE (entry), position);
}

void
warlock_entry_set_text (char *str)
{
	gtk_entry_set_text (GTK_ENTRY (entry), str);
}

void
warlock_entry_clear (void)
{
	warlock_entry_set_text ("");
}

const char *
warlock_entry_get_text (void)
{
	return gtk_entry_get_text (GTK_ENTRY (entry));
}

gboolean
warlock_entry_give_key_event (GdkEventKey *event)
{
	gboolean result;
	g_signal_emit_by_name (entry, "key_press_event", event, &result);
        return result;
}

void
warlock_entry_submit (void)
{
	send_text (gtk_entry_get_text (GTK_ENTRY (entry)));
        gtk_entry_set_text(GTK_ENTRY(entry), "");
}

static gboolean
entry_finish (GtkWidget *widget, GdkEvent *event, gpointer user_data)
{
        preferences_notify_remove (command_size_notification);
        preferences_notify_remove (history_size_notification);

	return FALSE;
}

static void
command_size_change (const char *key, gpointer user_data)
{
        min_command_size = preferences_get_int (key);
}

static void
history_size_change (const char *key, gpointer user_data)
{
        max_history_size = preferences_get_int (key);
        trim_history ();
}

void
warlock_entry_init (GtkWidget *widget)
{
        char *key;

        entry = widget;

        // init command_size
        key = preferences_get_key (PREF_COMMAND_SIZE);
        min_command_size = preferences_get_int (key);
        command_size_notification = preferences_notify_add (key,
                        command_size_change, NULL);
        g_free(key);

        // init history size
        key = preferences_get_key (PREF_COMMAND_HISTORY_SIZE);
        max_history_size = preferences_get_int (key);
        history_size_notification = preferences_notify_add (key,
                        history_size_change, NULL);
        g_free(key);

        // init history
        key = preferences_get_key (PREF_COMMAND_HISTORY);
        command_history = preferences_get_list (key, PREFERENCES_VALUE_STRING);
        g_free(key);

	// Clean-up when the widget is destroyed
        g_signal_connect (entry, "delete-event", G_CALLBACK(entry_finish),
			NULL);
}

void
warlock_history_next (void)
{
        if (pos > 0) {
                pos--;
        }
        if (pos == 0) {
                warlock_entry_clear ();
        } else {
                char* str;
                str = g_slist_nth_data (command_history, pos - 1);
                warlock_entry_set_text (str);
                warlock_entry_set_position (-1);
        }
}

void
warlock_history_prev (void)
{
        char* str;

        str = g_slist_nth_data (command_history, pos);
        if (str != NULL) {
                pos++;
                warlock_entry_set_text (str);
                warlock_entry_set_position (-1);
        }
}

void
warlock_history_last (void)
{
        if (command_history != NULL && command_history->data != NULL) {
                send_text (command_history->data);
        }
}

EXPORT
void
on_warlock_entry_activate              (GtkEntry        *entry,
                                        gpointer         user_data)
{
	warlock_entry_submit ();
}

static void
script_command (int argc, const char **argv)
{
	char *script_path;
        char *script_prefix;
        char *key;
	int i;
	gboolean found;

	const struct {
		const char *suffix;
		void (*script_loader) (const char *filename, int argc,
				const char **argv);
	} suffixes[] = {
		{"\\.[Cc][Mm][Dd]", script_load},
		{NULL, NULL}
	};

        g_assert (argc >= 1);

        key = preferences_get_key (PREF_SCRIPT_PATH);
        script_path = preferences_get_string (key);
        g_free (key);

        script_prefix = g_build_filename (script_path, argv[0], NULL);

	found = FALSE;
	for (i = 0; suffixes[i].suffix != NULL && !found; i++) {
		char *pattern;
		pcre *regex;
		GDir *dir;
		GError *err;
		const char *file;
		const char *pcre_err;
		int match[3];
		int offset;

		/* create the pattern */
		pattern = g_strdup_printf ("^%s%s$", argv[0],
				suffixes[i].suffix);
		regex = pcre_compile (pattern, 0, &pcre_err, &offset, NULL);
		g_free (pattern);
		if (regex == NULL) {
			g_error ("Error compiling regex to open script at "
					"character %d: %s\n", offset, pcre_err);
			continue;
		}

		/* check if any files match */
		err = NULL;
		dir = g_dir_open (script_path, 0, &err);
		print_error (err);
		while ((file = g_dir_read_name (dir)) != NULL) {
			if (pcre_exec (regex, NULL, file, (int)strlen (file), 0,
						0, match, 3) >= 1) {
				char *result;

				result = g_build_filename (script_path, file,
						NULL);
				(suffixes[i].script_loader)(result, argc, argv);

				found = TRUE;
				break;
			}
		}
		g_dir_close (dir);
		pcre_free (regex);
	}

	if (!found) {
		echo_f ("*** could not load script: %s ***\n", argv[0]);
        }

        g_free (script_path);
        g_free (script_prefix);
}
