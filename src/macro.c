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

#include "warlock.h"
#include "macro.h"
#include "debug.h"
#include "entry.h"
#include "warlocktime.h"
#include "warlockview.h"
#include "preferences.h"

/* external variables */
extern GladeXML *warlock_xml;

/* local variables */
static GHashTable *macros = NULL;

/* function definitions */
static gboolean macro_equal (gconstpointer a, gconstpointer b)
{
        return ((GdkEventKey*)a)->state == ((GdkEventKey*)b)->state
		&& ((GdkEventKey*)a)->keyval == ((GdkEventKey*)b)->keyval;
}

static guint macro_hash (gconstpointer key)
{
        return ((GdkEventKey*)key)->keyval * ((GdkEventKey*)key)->state;
}

static void macro_create (int keyval, int state, const char *command)
{
	GdkEventKey *key = g_new(GdkEventKey, 1);
	key->keyval = keyval;
	key->state = state;

	g_hash_table_insert (macros, key, g_strdup (command));
}

void macro_parse_string (const char *string, guint *keyval, guint *state,
                char **command)
{
        int i;
        char *state_str, *str, *command_str;

        command_str = NULL;
        state_str = NULL;
        str = g_strdup (string);

        for (i = 0; str[i] != '\0'; i++) {
                if (str[i] == ' ') {
                        str[i] = '\0';
                        if (state_str == NULL) {
                                state_str = &str[i + 1];
                        } else {
                                command_str = &str[i + 1];
                                break;
                        }
                }
        }
        g_assert (command_str != NULL);
        if (keyval != NULL) {
                *keyval = gdk_keyval_from_name (str);
        }
        if (state != NULL) {
                *state = atoi (state_str);
        }
        if (command != NULL) {
                *command = command_str;
        }
}

static void build_list (void)
{
        GSList *list, *cur;

	macros = g_hash_table_new (macro_hash, macro_equal);

        list = preferences_get_list (preferences_get_key (PREF_MACROS),
                        PREFERENCES_VALUE_STRING);
        for (cur = list; cur != NULL; cur = cur->next) {
                guint keyval, state;
                char *command;
                macro_parse_string (cur->data, &keyval, &state, &command);
                macro_create (keyval, state, command);
        }
}

static void rebuild_list (const char *key, gpointer user_data)
{
        build_list ();
}

void macro_init (void)
{
        build_list ();
        preferences_notify_add (preferences_get_key (PREF_MACROS),
                        rebuild_list, NULL);
}

static char *prompt_string (const char *title)
{
	GtkWidget *dialog;
	GtkWidget *entry;
	GtkWidget *label;
	gint result;
        char *text;

	dialog = gtk_dialog_new_with_buttons (title, GTK_WINDOW
                        (glade_xml_get_widget (warlock_xml, "main_window")),
                        GTK_DIALOG_DESTROY_WITH_PARENT,
                        GTK_STOCK_OK, GTK_RESPONSE_ACCEPT,
                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL, NULL);
	gtk_dialog_set_default_response (GTK_DIALOG (dialog),
			GTK_RESPONSE_ACCEPT);

	entry = gtk_entry_new();
	gtk_entry_set_activates_default (GTK_ENTRY (entry), TRUE);
	label = gtk_label_new (_("Enter text:"));
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), label, TRUE,
			TRUE, 0);
	gtk_box_pack_start (GTK_BOX (GTK_DIALOG (dialog)->vbox), entry, TRUE,
			TRUE, 0);
	gtk_widget_show_all (dialog);

	result = gtk_dialog_run (GTK_DIALOG (dialog));

	if (result != GTK_RESPONSE_ACCEPT) {
		text = NULL;
	} else {
                text = g_strdup (gtk_entry_get_text (GTK_ENTRY (entry)));
        }

        gtk_widget_destroy (dialog);

        return text;
}

static void parse_macro_command (const char *string)
{
	char *response = NULL;
	int i;
        double time;
        static char *saved_text = NULL;
	static int saved_position = 0;

	for (i = 0; string[i] != '\0'; i++) {
		debug ("%c", string[i]);
		if (string[i] == '\\' && string[i + 1] != '\0') {
			i++;
			debug ("\nMACRO COMMAND == '\\%c'\n", string[i]);

			switch (string[i]) {
				/* Send everything before it to DR */
                                case 'n':
				case 'r':
					warlock_entry_submit ();
					break;

				/* Pause 1 second */
				case 'p':
                                        time = warlock_get_time () + 1.0;
                                        while (warlock_get_time () < time) {
                                                gtk_main_iteration ();
                                        }
					break;

				/* Clear the command entry */
				case 'x':
					warlock_entry_clear ();
					break;

				/* Prompt user for value -- only prompt once and fill in the rest */
				case '?':
					if (response == NULL) {
						response = prompt_string
							(string);	
						if (response == NULL) {
							response = "";
						}
					}
					warlock_entry_append (response);	
					break;

                                case '\\':
                                case '@':
                                        warlock_entry_append_c (string[i]);
                                        break;

                                case 'S':
                                        if (saved_text != NULL) {
                                                g_free (saved_text);
                                        }
					saved_position =
						warlock_entry_get_position ();
                                        saved_text = g_strdup
                                                (warlock_entry_get_text ());
                                        break;

                                case 'R':
                                        if (saved_text != NULL) {
                                                warlock_entry_append
                                                        (saved_text);
						warlock_entry_set_position
							(saved_position);
                                        }
                                        break;

				default:
					echo ("Invalid macro\n");
					return;
			}

		} else if (string[i] == '@') {
			/* Insert cursor at this index .. */
			warlock_entry_set_position (-1);
		} else {
			warlock_entry_append_c (string[i]);
		}
	}
}

gboolean process_macro_key (GdkEventKey *key)
{
	char *command;

        g_assert (macros != NULL);

	command = g_hash_table_lookup (macros, key);

	if (command == NULL) {
		return FALSE;
	}

	parse_macro_command (command);
	return TRUE;
}
