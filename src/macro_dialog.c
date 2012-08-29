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

#include "preferences.h"
#include "debug.h"
#include "warlock.h"
#include "macro_dialog.h"
#include "macro.h"

enum {
        COMMAND_COLUMN,
        KEY_COLUMN,
        KEYVAL_COLUMN,
        STATE_COLUMN,
        N_COLUMNS
};

extern GladeXML *warlock_xml;

static GtkListStore *macro_list = NULL;

EXPORT
void
on_macro_remove_button_clicked (GtkButton *button, gpointer user_data)
{
        int keyval, state;
        GSList *list, *cur;
        GtkTreeSelection *selected_macro;
        GtkWidget *view;
        GtkTreeIter iter;

        g_assert (macro_list != NULL);

        view = glade_xml_get_widget (warlock_xml, "macro_view");
        selected_macro = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
        if (!gtk_tree_selection_get_selected (selected_macro, NULL, &iter)) {
                /* if there's no selected macro, don't do anything */
                return;
        }

        gtk_tree_model_get (GTK_TREE_MODEL (macro_list), &iter,
                        KEYVAL_COLUMN, &keyval,
                        STATE_COLUMN, &state,
                        -1);

        list = preferences_get_list (preferences_get_key (PREF_MACROS),
                        PREFERENCES_VALUE_STRING);
        for (cur = list; cur != NULL; cur = cur->next) {
                guint cur_keyval, cur_state;

                macro_parse_string (cur->data, &cur_keyval, &cur_state, NULL);
                if (keyval == cur_keyval && state == cur_state) {
                        list = g_slist_remove_link (list, cur);
                        break;
                }
        }
        g_assert (cur != NULL);
        preferences_set_list (preferences_get_key (PREF_MACROS),
                        PREFERENCES_VALUE_STRING, list);
}

static char *macro_to_string (guint keyval, guint state, char *command)
{
        if (command != NULL) {
                return g_strdup_printf ("%s %d %s", gdk_keyval_name (keyval),
                                state, command);
        } else {
                return g_strdup_printf ("%s %d ", gdk_keyval_name (keyval),
                                state);
        }
}

static void cell_edited (GtkCellRendererText *cellrenderertext,
		gchar *arg1, gchar *arg2, gpointer user_data)
{
	GtkTreeIter tree_iter;
	int n;
        guint keyval, state;
        GSList *list, *cur;

        g_assert (macro_list != NULL);

	debug ("edited: %s, %s\n", arg1, arg2);
	n = atoi (arg1);
	g_assert (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL (macro_list),
                                &tree_iter, NULL, n));

	gtk_tree_model_get (GTK_TREE_MODEL (macro_list), &tree_iter,
                        KEYVAL_COLUMN, &keyval,
                        STATE_COLUMN, &state,
                        -1);

        list = preferences_get_list (preferences_get_key (PREF_MACROS),
                        PREFERENCES_VALUE_STRING);
        for (cur = list; cur != NULL; cur = cur->next) {
                guint cur_keyval, cur_state;
                char *command;

                macro_parse_string (cur->data, &cur_keyval, &cur_state,
                                &command);
                if (keyval == cur_keyval && state == cur_state) {
                        cur->data = macro_to_string (keyval, state, arg2);
                }
        }
        preferences_set_list (preferences_get_key (PREF_MACROS),
                        PREFERENCES_VALUE_STRING, list);
}

static void rebuild_list (void)
{
        GSList *macro_pref_list, *cur;
        GtkWidget *macro_view;

        macro_view = glade_xml_get_widget (warlock_xml, "macro_view");


        /* create and fill the macro dialog list */
        macro_list = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING,
                        G_TYPE_STRING, G_TYPE_UINT, G_TYPE_UINT);

        macro_pref_list = preferences_get_list (preferences_get_key
                        (PREF_MACROS), PREFERENCES_VALUE_STRING);
        for (cur = macro_pref_list; cur != NULL; cur = cur->next) {
                GtkTreeIter iter;
                guint keyval, state;
                GString *key;
                char *command;

                macro_parse_string (cur->data, &keyval, &state, &command);

                if (keyval == 0) {
                        debug ("Got an invalid macro key\n");
                        break;
                }

                key = g_string_new ("");
                if (state & GDK_CONTROL_MASK) {
                        g_string_append (key, "<Ctrl>");
                }
                if (state & GDK_MOD1_MASK) {
                        g_string_append (key, "<Alt>");
                }
                g_string_append (key, gdk_keyval_name (keyval));

                gtk_list_store_append (macro_list, &iter);
                gtk_list_store_set (macro_list, &iter,
                                KEY_COLUMN, key->str,
                                KEYVAL_COLUMN, keyval,
                                STATE_COLUMN, state,
                                COMMAND_COLUMN, command,
                                -1);
        }

        gtk_tree_view_set_model (GTK_TREE_VIEW (macro_view), 
                        GTK_TREE_MODEL (macro_list));

        gtk_widget_show (macro_view);
}

static void changed_list (const char *key, gpointer user_data)
{
        rebuild_list ();
}

void macro_dialog_init (void)
{
        GtkCellRenderer *command_renderer, *key_renderer;
        GtkWidget *macro_view;
        GtkTreeViewColumn *command_column, *key_column;
        GtkTreeSelection *selection;

        rebuild_list ();

        macro_view = glade_xml_get_widget (warlock_xml, "macro_view");

        /* add the key column */
        key_renderer = gtk_cell_renderer_text_new ();
        key_column = gtk_tree_view_column_new_with_attributes (_("Key Combo"),
                        key_renderer, "text", KEY_COLUMN, NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (macro_view), key_column);

        /* add a text column */
        command_renderer = gtk_cell_renderer_text_new ();
	g_signal_connect (command_renderer, "edited", G_CALLBACK (cell_edited),
                        NULL);
        g_object_set (G_OBJECT (command_renderer), "editable", TRUE,
                        "editable-set", TRUE, NULL);
        command_column = gtk_tree_view_column_new_with_attributes
                (_("Command"), command_renderer, "text", COMMAND_COLUMN, NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (macro_view),
                        command_column);

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (macro_view));
        gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
        preferences_notify_add (preferences_get_key (PREF_MACROS), changed_list,
                        NULL);
}

static int
match_key (const char *macro1, const char *macro2)
{
        guint keyval1, state1, keyval2, state2;

        macro_parse_string (macro1, &keyval1, &state1, NULL);
        macro_parse_string (macro2, &keyval2, &state2, NULL);

	if (keyval1 == keyval2 && state1 == state2) {
		return 0;
	} else if (keyval1 > keyval2) {
                /* this condition and the next one are totally contrived */
                return 1;
	} else {
                return -1;
        }
}

static gboolean grab_keys (GtkWidget *widget, GdkEventKey *event,
                gpointer user_data)
{
        GSList *list;
	char *macro;
        char *old_macro;
        char *command;

        switch (event->keyval) {
                case GDK_Control_R:
                case GDK_Control_L:
                case GDK_Alt_R:
                case GDK_Alt_L:
                case GDK_Shift_L:
                case GDK_Shift_R:
                        return FALSE;
        }

        list = preferences_get_list (preferences_get_key
                                        (PREF_MACROS),
                                        PREFERENCES_VALUE_STRING);

        /* if we were passed a match, remove that one from the list */
        old_macro = user_data;
        if (old_macro != NULL) {
                GSList *link;

                macro_parse_string (old_macro, NULL, NULL, &command);
                link = g_slist_find_custom (list, old_macro,
                                (GCompareFunc)match_key);
                g_assert (link != NULL);
                list = g_slist_delete_link (list, link);
        } else {
                command = NULL;
        }

        macro = macro_to_string (event->keyval, event->state, command);
        if (g_slist_find_custom (list, macro, (GCompareFunc)match_key)
                        == NULL) {
                list = g_slist_append (list, macro);
                preferences_set_list (preferences_get_key
                                (PREF_MACROS),
                                PREFERENCES_VALUE_STRING, list);
                gtk_dialog_response (GTK_DIALOG (widget),
                                GTK_RESPONSE_OK);
        }
        /* FIXME: else select the macro they just chose */

        return TRUE;
}

static void create_macro_grab_dialog (char *key)
{
        GtkWidget *dialog;
        GtkWidget *label;

        dialog = gtk_dialog_new_with_buttons (_("Macro Key"),
                        GTK_WINDOW (glade_xml_get_widget (warlock_xml,
                                        "main_window")),
                        GTK_DIALOG_MODAL | GTK_DIALOG_DESTROY_WITH_PARENT,
                        GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
                        NULL);
        g_signal_connect (dialog, "key_press_event", G_CALLBACK (grab_keys),
                        key);
        label = gtk_label_new (_("Press a key combination that you would like "
                                "associated with a command."));
        gtk_label_set_line_wrap (GTK_LABEL (label), TRUE);
        gtk_container_add (GTK_CONTAINER (GTK_DIALOG (dialog)->vbox), label);
        gtk_widget_show_all (dialog);

        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
}

EXPORT
void on_macro_add_button_clicked (GtkButton *button, gpointer user_data)
{
        create_macro_grab_dialog (NULL);
}

EXPORT
void on_macro_change_button_clicked (GtkButton *button, gpointer user_data)
{
        GtkWidget *view;
        GtkTreeSelection *selected_macro;
        GtkTreeIter iter;
        guint keyval, state;
        char *command;

        g_assert (macro_list != NULL);

        view = glade_xml_get_widget (warlock_xml, "macro_view");
        selected_macro = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
        if (!gtk_tree_selection_get_selected (selected_macro, NULL, &iter)) {
                /* if there's no selected macro, don't do anything */
                return;
        }

        gtk_tree_model_get (GTK_TREE_MODEL (macro_list), &iter,
                        KEYVAL_COLUMN, &keyval,
                        STATE_COLUMN, &state,
                        COMMAND_COLUMN, &command,
                        -1);

        create_macro_grab_dialog (macro_to_string (keyval, state, command));
}

EXPORT
void
on_macros_close_button_clicked (GtkButton *button, gpointer user_data)
{
        gtk_widget_hide (glade_xml_get_widget (warlock_xml, "macros_dialog"));
}

EXPORT
gboolean
on_macros_dialog_delete_event (GtkWidget *widget, GdkEvent *event,
                gpointer user_data)
{
        gtk_widget_hide (glade_xml_get_widget (warlock_xml, "macros_dialog"));

	return TRUE;
}
