/* Warlock Front End
 * Copyright Sean Proctor, Marshall Culpepper 2003 - 2004
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

#include "preferences.h"
#include "highlight.h"
#include "debug.h"
#include "warlock.h"
#include "text_strings_dialog.h"
#include "helpers.h"
#include "warlockcolorbutton.h"
#include "warlockfontbutton.h"

enum {
	CASE_SENSITIVE_COLUMN,
        STRING_COLUMN,
	ID_COLUMN,
        N_COLUMNS
};

/* local function prototypes */
static void null_highlight_table (void);
static void selection_changed (GtkTreeSelection *selection, gpointer user_data);
static void rebuild_list (void);
static void cell_toggled (GtkCellRendererToggle *cellrenderertoggle,
                gchar *arg1, gpointer user_data);
static void cell_edited (GtkCellRendererText *cellrenderertext, gchar *arg1,
                gchar *arg2, gpointer user_data);
static void changed_list (const char *key, gpointer user_data);
static void swap_highlights_order (int id1, int id2);
static void set_color_button_properties (const char *key, GtkWidget* button);
static void set_font_button_properties (const char *key, GtkWidget* button);
static void highlights_view_init (void);

/* external variables */
extern GladeXML *warlock_xml;

/* local variables */
static GtkListStore *highlight_dialog_list = NULL;
static GSList *edit_notifiers = NULL;
static GSList *highlight_notifiers = NULL;
static int selected_id = -1;
static GtkWidget* highlight_buttons[HIGHLIGHT_MATCHES][3];

/****************************************************************
 * Global Functions                                             *
 ****************************************************************/

/* initialize separate parts of the window */
void
text_strings_dialog_init (void)
{
        highlights_view_init ();
}

/************************************************************************
 * Glade signal handlers                                                *
 ************************************************************************/

/* hide the window when the close button is hit */
void
on_text_strings_close_button_clicked (GtkButton *button, gpointer user_data)
{
        GtkWidget *dialog;

        dialog = glade_xml_get_widget (warlock_xml, "text_strings_dialog");
        gtk_widget_hide (dialog);
}

/* hide the window when the X is hit */
gboolean
on_text_strings_dialog_delete_event (GtkWidget *widget, GdkEvent *event,
                gpointer user_data)
{
        GtkWidget *dialog;

        dialog = glade_xml_get_widget (warlock_xml, "text_strings_dialog");
        gtk_widget_hide (dialog);

	return TRUE;
}

/* handle the down button being pressed */
void
on_highlight_down_button_clicked (GtkButton *button, gpointer user_data)
{
        GtkWidget *view;
        GtkTreeSelection *selection;
        GtkTreeIter iter;
        int id, next_id;

        /* get the iter of the currently selected highlight */
        view = glade_xml_get_widget (warlock_xml, "highlight_view");
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
        if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
                /* return if there is no selected highlight */
                return;
        }

        /* get the current id */
        gtk_tree_model_get (GTK_TREE_MODEL (highlight_dialog_list), &iter,
                        ID_COLUMN, &id, -1);

        /* get the next id */
        if (!gtk_tree_model_iter_next (GTK_TREE_MODEL (highlight_dialog_list),
                                &iter)) {
                /* don't do anything if we're already at the end of the list */
                return;
        }
        gtk_tree_model_get (GTK_TREE_MODEL (highlight_dialog_list), &iter,
                        ID_COLUMN, &next_id, -1);

        /* and swap the order */
        debug ("moving up %d to %d\n", id, next_id);
        swap_highlights_order (id, next_id);
        selected_id = id;
}

/* handle the up button being pressed */
void
on_highlight_up_button_clicked (GtkButton *button, gpointer user_data)
{
        GtkWidget *view;
        GtkTreeSelection *selection;
        GtkTreeIter iter;
        GtkTreePath *path;
        int id, prev_id;

        /* get the currently selected highlight */
        view = glade_xml_get_widget (warlock_xml, "highlight_view");
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
        if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
                /* return if there is no currently selected highlight */
                return;
        }

        /* get the current id */
        gtk_tree_model_get (GTK_TREE_MODEL (highlight_dialog_list), &iter,
                        ID_COLUMN, &id, -1);

        /* get the id for the item higher on the list */
        path = gtk_tree_model_get_path (GTK_TREE_MODEL (highlight_dialog_list),
                        &iter);
        if (!gtk_tree_path_prev (path)) {
                /* return if there was no item higher on the list */
                return;
        }
        gtk_tree_model_get_iter (GTK_TREE_MODEL (highlight_dialog_list), &iter,
                        path);
        gtk_tree_model_get (GTK_TREE_MODEL (highlight_dialog_list), &iter,
                        ID_COLUMN, &prev_id,
                        -1);
        debug ("moving up %d to %d\n", id, prev_id);
        swap_highlights_order (id, prev_id);
        selected_id = id;
}

/* handle the add button being pressed */
void
on_highlight_add_button_clicked (GtkButton *button, gpointer user_data)
{
        GSList *list;
        int id;

        debug ("called on_highlight_add_button_clicked\n");

        /* get the list from gconf */
        list = preferences_get_list (preferences_get_key
                        (PREF_HIGHLIGHTS_INDEX), PREFERENCES_VALUE_INT);

        /* get a unique id */
        id = g_random_int_range (0, 0xFFFFFF);
        while (g_slist_find (list, GINT_TO_POINTER (id))) {
                id++;
        }

        /* set the properties of the highlight */
        preferences_set_string (preferences_get_highlight_key (id,
                                PREF_HIGHLIGHT_STRING), "");
        preferences_set_bool (preferences_get_highlight_key (id,
                                PREF_HIGHLIGHT_CASE_SENSITIVE), FALSE);
        preferences_set_color (preferences_get_highlight_match_key (id, 0,
                                PREF_HIGHLIGHT_MATCH_TEXT_COLOR),
                        gdk_color_from_string ("yellow"));

        /* add the highlight to the list */
        list = g_slist_append (list, GINT_TO_POINTER (id));
        preferences_set_list (preferences_get_key (PREF_HIGHLIGHTS_INDEX),
                        PREFERENCES_VALUE_INT, list);
        selected_id = id;
}

/* handle the remove button being pressed */
void
on_highlight_remove_button_clicked (GtkButton *button, gpointer user_data)
{
        GtkTreeSelection *selection;
        GtkWidget *view;
        int id;
        GSList *list, *cur;
        char *key;
        GtkTreeIter iter, *other_iter;

        /* get the iter of the currently selected highlight */
        view = glade_xml_get_widget (warlock_xml, "highlight_view");
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
        if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
                /* return if there is no selected highlight */
                return;
        }

        /* get the id */
        gtk_tree_model_get (GTK_TREE_MODEL (highlight_dialog_list), &iter,
                        ID_COLUMN, &id, -1);

        /* get the id of the next item, or previous, or set to -1 */
        other_iter = gtk_tree_iter_copy (&iter);
        if (gtk_tree_model_iter_next (GTK_TREE_MODEL (highlight_dialog_list),
                                other_iter)) {
                gtk_tree_model_get (GTK_TREE_MODEL (highlight_dialog_list),
                                other_iter, ID_COLUMN, &selected_id, -1);
        } else {
                GtkTreePath *path;

                /* get the id for the item higher on the list */
                path = gtk_tree_model_get_path (GTK_TREE_MODEL
                                (highlight_dialog_list), &iter);
                if (gtk_tree_path_prev (path)) {
                        g_assert (gtk_tree_model_get_iter (GTK_TREE_MODEL
                                        (highlight_dialog_list), other_iter,
                                        path));
                        gtk_tree_model_get (GTK_TREE_MODEL
                                        (highlight_dialog_list), other_iter,
                                        ID_COLUMN, &selected_id, -1);
                } else {
                        selected_id = -1;
                }
        }

        debug ("removing %X\n", id);

        /* make sure we aren't watching this highlight any more */
        for (cur = highlight_notifiers; cur != NULL; cur = cur->next) {
                preferences_notify_remove (GPOINTER_TO_INT (cur->data));
        }
        g_slist_free (highlight_notifiers);
        highlight_notifiers = NULL;

        /* clear the table */
        null_highlight_table ();

        /* remove the highlight from the gconf list */
        key = preferences_get_key (PREF_HIGHLIGHTS_INDEX);
        list = preferences_get_list (key, PREFERENCES_VALUE_INT);
        list = g_slist_remove_all (list, GINT_TO_POINTER (id));
        preferences_set_list (key, PREFERENCES_VALUE_INT, list);
        g_free (key);
}

/*********************************************************************
 * Local signal handler functions                                    *
 *********************************************************************/

/* build the list, connect the appropriate objects, and clear the table */
static void
highlights_view_init (void)
{
        GtkCellRenderer *text_renderer, *toggle_renderer;
        GtkWidget *highlight_view;
        GtkTreeViewColumn *text_column, *toggle_column;
        GtkTreeSelection *selection;
        GtkWidget *table;
        int i;

        rebuild_list ();

        highlight_view = glade_xml_get_widget (warlock_xml, "highlight_view");

        /* add the case insensitive column */
        toggle_renderer = gtk_cell_renderer_toggle_new ();
        toggle_column = gtk_tree_view_column_new_with_attributes
                (_("Case Sensitive"), toggle_renderer, "active",
                 CASE_SENSITIVE_COLUMN, NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (highlight_view),
                        toggle_column);
	g_signal_connect (toggle_renderer, "toggled", G_CALLBACK (cell_toggled),
                        NULL);

        /* add a text column */
        text_renderer = gtk_cell_renderer_text_new ();
        g_object_set (G_OBJECT (text_renderer), "editable", TRUE,
                        "editable-set", TRUE, NULL);
        text_column = gtk_tree_view_column_new_with_attributes (_("Regex"),
                        text_renderer, "text", STRING_COLUMN, NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (highlight_view),
                        text_column);
	g_signal_connect (text_renderer, "edited", G_CALLBACK (cell_edited),
                        text_column);

        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW
                        (highlight_view));
        gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
        preferences_notify_add (preferences_get_key (PREF_HIGHLIGHTS_INDEX),
                        changed_list, NULL);
        g_signal_connect (selection, "changed", G_CALLBACK (selection_changed),
                        NULL);

        /* create the color/font buttons and attach them to the table */
        table = glade_xml_get_widget (warlock_xml, "highlight_buttons_table");
        for (i = 0; i < HIGHLIGHT_MATCHES; i++) {
                GtkWidget *text_button;
                GtkWidget *base_button;
                GtkWidget *font_button;

                text_button = warlock_color_button_new ();
                base_button = warlock_color_button_new ();
                font_button = warlock_font_button_new ();

                gtk_table_attach (GTK_TABLE (table), text_button,
                                1, 2, i + 1, i + 2,
                                GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL,
                                0, 0);
                gtk_table_attach (GTK_TABLE (table), base_button,
                                2, 3, i + 1, i + 2,
                                GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL,
                                0, 0);
                gtk_table_attach (GTK_TABLE (table), font_button,
                                3, 4, i + 1, i + 2,
                                GTK_SHRINK | GTK_FILL, GTK_SHRINK | GTK_FILL,
                                0, 0);

                highlight_buttons[i][0] = text_button;
                gtk_widget_show (text_button);
                highlight_buttons[i][1] = base_button;
                gtk_widget_show (base_button);
                highlight_buttons[i][2] = font_button;
                gtk_widget_show (font_button);
        }

        null_highlight_table ();
        selected_id = -1;
}

/* handle the user changing the string */
static void
cell_edited (GtkCellRendererText *cellrenderertext, gchar *arg1, gchar *arg2,
                gpointer user_data)
{
	GtkTreeIter iter;
	char *old_tag;
	int n, id;

	debug ("edited: %s, %s\n", arg1, arg2);
	n = atoi (arg1);
	g_assert (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL
                                (highlight_dialog_list), &iter, NULL, n));

	gtk_tree_model_get (GTK_TREE_MODEL (highlight_dialog_list), &iter,
			STRING_COLUMN, &old_tag,
                        ID_COLUMN, &id,
                        -1);

	g_assert (old_tag != NULL);

        if (strcmp (old_tag, arg2) != 0) {
                preferences_set_string (preferences_get_highlight_key (id,
                                        PREF_HIGHLIGHT_STRING), arg2);
        }
}

/* handle the user change the case sensitivity */
static void
cell_toggled (GtkCellRendererToggle *cellrenderertoggle, gchar *arg1,
                gpointer user_data)
{
	GtkTreeIter iter;
	int n, id;
        gboolean case_sensitive;

	debug ("toggled: %s\n", arg1);

	n = atoi (arg1);
	g_assert (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL
                                (highlight_dialog_list), &iter, NULL, n));

	gtk_tree_model_get (GTK_TREE_MODEL (highlight_dialog_list), &iter,
                        ID_COLUMN, &id,
                        CASE_SENSITIVE_COLUMN, &case_sensitive,
                        -1);

        preferences_set_bool (preferences_get_highlight_key
                        (id, PREF_HIGHLIGHT_CASE_SENSITIVE),
                        !case_sensitive);
}

/* change the gconf color */
static void
change_color (WarlockColorButton *button, char *key)
{
        GdkColor *color;

        color = warlock_color_button_get_color (button);
        preferences_set_color (key, color);
        if (color != NULL) {
                gdk_color_free (color);
        }
}

/* change the gconf font */
static void
change_font (WarlockFontButton *button, gpointer user_data)
{
        char *font;
        char *key;

        key = user_data;
	font = warlock_font_button_get_font_name (button);
        preferences_set_string (key, font);
        g_free (font);
}

/* handle when the selection is changed on the highlight_dialog_view */
static void
selection_changed (GtkTreeSelection *selection, gpointer user_data)
{
	int n, id;
        GSList *cur;
        GtkTreeIter iter;

        debug ("selection_changed called\n");

        /* find the currently selected highlight, if there's none, clear the
         * table and return
         */
        if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
                null_highlight_table ();
                return;
        }

        gtk_tree_model_get (GTK_TREE_MODEL (highlight_dialog_list), &iter,
                        ID_COLUMN, &id, -1);

        /* remove the notifiers for the previously selected highlight */
        for (cur = edit_notifiers; cur != NULL; cur = cur->next) {
                preferences_notify_remove (GPOINTER_TO_INT (cur->data));
        }
        g_slist_free (edit_notifiers);
        edit_notifiers = NULL;

        /* for each highlight match, set the proper color */
	for (n = 0; n < HIGHLIGHT_MATCHES; n++) {
                char *key;

                /* do text color button stuff */
                key = preferences_get_highlight_match_key (id, n,
                                PREF_HIGHLIGHT_MATCH_TEXT_COLOR);
                set_color_button_properties (key, highlight_buttons[n][0]);
                g_free (key);

                /* do base color button stuff */
                key = preferences_get_highlight_match_key (id, n,
                                PREF_HIGHLIGHT_MATCH_BASE_COLOR);
                set_color_button_properties (key, highlight_buttons[n][1]);
                g_free (key);

                /* do font button stuff */
                key = preferences_get_highlight_match_key (id, n,
                                PREF_HIGHLIGHT_MATCH_FONT);
                set_font_button_properties (key, highlight_buttons[n][2]);
                g_free (key);
        }
}

/**************************************************************************
 * Preference notification handler functions                              *
 **************************************************************************/

/* handle the case sensitivity being changed in the preferences */
static void
changed_sensitive (const char *key, gpointer user_data)
{
        int n;
        GtkTreeIter iter;

        debug ("called changed_sensitive\n");

        n = GPOINTER_TO_INT (user_data);
	g_assert (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL
                                (highlight_dialog_list), &iter, NULL, n));
        gtk_list_store_set (highlight_dialog_list, &iter,
                        CASE_SENSITIVE_COLUMN, preferences_get_bool (key), -1);
}

/* handle the highlight string being changed in gconf */
static void
changed_string (const char *key, gpointer user_data)
{
        int n;
        GtkTreeIter iter;

        n = GPOINTER_TO_INT (user_data);
	g_assert (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL
                                (highlight_dialog_list), &iter, NULL, n));
        gtk_list_store_set (highlight_dialog_list, &iter,
                        STRING_COLUMN, preferences_get_string (key), -1);
}

/* handle the list being changed */
static void
changed_list (const char *key, gpointer user_data)
{
        rebuild_list ();
}

/* handle the color being changed */
static void
changed_color (const char *key, gpointer user_data)
{
        GtkWidget *button;
        GdkColor *color;

        debug ("called changed_color\n");

        button = user_data;
        color = preferences_get_color (key);

        warlock_color_button_set_color (WARLOCK_COLOR_BUTTON (button), color);
        if (color != NULL) {
                g_free (color);
        }
}

/* handle when the font changes through gconf */
static void
changed_font (const char *key, gpointer user_data)
{
        GtkWidget *button;
        char *font;

        button = user_data;
        font = preferences_get_string (key);

        warlock_font_button_set_font_name (WARLOCK_FONT_BUTTON (button), font);
        if (font != NULL) {
                g_free (font);
        }
}

/**********************************************************************
 * Local functions                                                    *
 **********************************************************************/

/* make a new model and forget about the old one */
static void
rebuild_list (void)
{
        GSList *highlight_list, *cur;
        GtkListStore *old_list;
        GtkWidget *highlight_view;
        int i;

        debug ("called rebuild_list\n");

        old_list = highlight_dialog_list;
        highlight_view = glade_xml_get_widget (warlock_xml, "highlight_view");

        /* remove old notifiers */
        for (cur = highlight_notifiers; cur != NULL; cur = cur->next) {
                preferences_notify_remove (GPOINTER_TO_INT (cur->data));
        }

        /* create and fill the highlight dialog list */
        highlight_dialog_list = gtk_list_store_new (N_COLUMNS, G_TYPE_BOOLEAN,
			G_TYPE_STRING, G_TYPE_INT);
        gtk_tree_view_set_model (GTK_TREE_VIEW (highlight_view), 
                        GTK_TREE_MODEL (highlight_dialog_list));

        /* go through the list of highlights from gconf, and create a model
         * from them */
        highlight_list = preferences_get_list (preferences_get_key
                        (PREF_HIGHLIGHTS_INDEX), PREFERENCES_VALUE_INT);
        for (i = 0, cur = highlight_list; cur != NULL; cur = cur->next, i++) {
                GtkTreeIter iter;
                int id;
                char *string;
                gboolean case_sensitive;
                char *case_key, *string_key;

                id = GPOINTER_TO_INT (cur->data);

                /* get the string, and attach a notifier to it */
                string_key = preferences_get_highlight_key (id,
                                PREF_HIGHLIGHT_STRING);
                string = preferences_get_string (string_key);
                highlight_notifiers = g_slist_append (highlight_notifiers,
                                GINT_TO_POINTER (preferences_notify_add
                                        (string_key, changed_string,
                                         GINT_TO_POINTER (i))));
                g_free (string_key);

                /* get the case and attach a notifier to it */
                case_key = preferences_get_highlight_key
                        (id, PREF_HIGHLIGHT_CASE_SENSITIVE);
                case_sensitive = preferences_get_bool (case_key);
                highlight_notifiers = g_slist_append (highlight_notifiers,
                                GINT_TO_POINTER (preferences_notify_add
                                        (case_key, changed_sensitive,
                                        GINT_TO_POINTER (i))));
                g_free (case_key);

                /* add these to the list */
                gtk_list_store_append (highlight_dialog_list, &iter);
                gtk_list_store_set (highlight_dialog_list, &iter,
                                STRING_COLUMN, string == NULL ? "" : string,
                                ID_COLUMN, id,
                                CASE_SENSITIVE_COLUMN, case_sensitive,
                                -1);
                debug ("id: %X\n", id);

                /* if there is no string, make the user set one */
                if (id == selected_id
                                || (selected_id == -1 && string == NULL)) {
                        GtkTreePath *path;
                        GtkTreeViewColumn *column;

                        path = gtk_tree_model_get_path (GTK_TREE_MODEL
                                        (highlight_dialog_list), &iter);
                        column = gtk_tree_view_get_column (GTK_TREE_VIEW
                                        (highlight_view), 1);
                        gtk_widget_grab_focus (highlight_view);
                        if (string == NULL) {
                                gtk_tree_view_set_cursor (GTK_TREE_VIEW
                                                (highlight_view), path, column,
                                                TRUE);
                        } else {
                                gtk_tree_view_set_cursor (GTK_TREE_VIEW
                                                (highlight_view), path, column,
                                                FALSE);
                        }
                }
        }

        gtk_widget_show (highlight_view);
}

/* set the properties of the button. key is the gconf key. if key is NULL,
 * then we clear the button */
static void
set_color_button_properties (const char *key, GtkWidget *button)
{
        GdkColor *color;

        /* disconnect any old signal handlers */
        g_signal_handlers_disconnect_matched (button,
                        G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                        G_CALLBACK (change_color), NULL);

        /* if we have a key, for the color, otherwise, NULL */
        if (key != NULL) {
                color = preferences_get_color (key);
        } else {
                color = NULL;
        }

        /* set and free the color */
        warlock_color_button_set_color (WARLOCK_COLOR_BUTTON (button), color);

        if (color != NULL) {
                g_free (color);
        }

        /* if we have a key, connect the signals, otherwise, there is
         * no highlight and we don't need to */
        if (key != NULL) {
                char *tmp_key;

                gtk_widget_set_sensitive (button, TRUE);

                tmp_key = g_strdup (key);
                g_signal_connect (button, "color-set",
                                G_CALLBACK (change_color), tmp_key);
                edit_notifiers = g_slist_append (edit_notifiers,
                                GINT_TO_POINTER (preferences_notify_add
                                        (tmp_key, changed_color, button)));
        } else {
                gtk_widget_set_sensitive (button, FALSE);
        }
}

/* set the properties of the button. key is the gconf key.
 * if key is NULL, then we clear the button. */
static void
set_font_button_properties (const char *key, GtkWidget *button)
{
        char *font;

        /* disconnect any old signal handlers */
        g_signal_handlers_disconnect_matched (button,
                        G_SIGNAL_MATCH_FUNC, 0, 0, NULL,
                        G_CALLBACK (change_font), NULL);

        /* if we have a key, checkbox is senstive, and check for the
         * font, otherwise, NULL */
        if (key != NULL) {
                font = preferences_get_string (key);
        } else {
                font = NULL;
        }

        warlock_font_button_set_font_name (WARLOCK_FONT_BUTTON (button), font);

        if (font != NULL) {
                g_free (font);
        }

        /* if we have a key, connect the signals, otherwise, there is
         * no highlight and we don't need to */
        if (key != NULL) {
                char *tmp_key;

                gtk_widget_set_sensitive (button, TRUE);

                tmp_key = g_strdup (key);
                g_signal_connect (button, "font-set",
                                G_CALLBACK (change_font), tmp_key);
                edit_notifiers = g_slist_append (edit_notifiers,
                                GINT_TO_POINTER (preferences_notify_add
                                        (tmp_key, changed_font, button)));
        } else {
                gtk_widget_set_sensitive (button, FALSE);
        }
}

/* set everything in the table inactive */
static void
null_highlight_table (void)
{
        GSList *cur;
        int n;

        debug ("null_highlight_table called\n");
        for (cur = edit_notifiers; cur != NULL; cur = cur->next) {
                preferences_notify_remove (GPOINTER_TO_INT (cur->data));
        }
        g_slist_free (edit_notifiers);
        edit_notifiers = NULL;

	for (n = 0; n < HIGHLIGHT_MATCHES; n++) {
                set_color_button_properties (NULL, highlight_buttons[n][0]);
                set_color_button_properties (NULL, highlight_buttons[n][1]);
                set_font_button_properties (NULL, highlight_buttons[n][2]);
        }
}

/* swap the oder of the two keys in the gconf list */
static void
swap_highlights_order (int id1, int id2)
{
        GSList *id1_pos, *id2_pos, *list;

        g_return_if_fail (id1 != id2);

        /* get the gconf list */
        list = preferences_get_list (preferences_get_key
                        (PREF_HIGHLIGHTS_INDEX), PREFERENCES_VALUE_INT);

        /* find the two keys */
        id1_pos = g_slist_find (list, GINT_TO_POINTER (id1));
        id2_pos = g_slist_find (list, GINT_TO_POINTER (id2));

        /* swap the order */
        id1_pos->data = GINT_TO_POINTER (id2);
        id2_pos->data = GINT_TO_POINTER (id1);

         /* save the list */
        preferences_set_list (preferences_get_key (PREF_HIGHLIGHTS_INDEX),
                        PREFERENCES_VALUE_INT, list);
}
