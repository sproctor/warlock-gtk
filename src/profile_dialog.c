/* Warlock Front End
 * Copyright Sean Proctor 2004
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

#include "debug.h"
#include "preferences.h"
#include "simu_connection.h"
#include "sge_connection.h"
#include "warlock.h"

/* external variables */
extern GladeXML *warlock_xml;

/* data types */
enum {
        NAME_COLUMN,
        ID_COLUMN,
        N_COLUMNS
};

/* local function declarations */
static void profile_clear (void);
static void next_clicked (GtkButton *button, gpointer user_data);
static void close_window (void);
static void load_profile (SgeState state, gpointer user_data);
static int get_id (void);
static void run_next (SgeState state);
static void prev_clicked (GtkButton *button, gpointer user_data);

/* local varaibles */
static GtkListStore *profile_dialog_list = NULL;
static GtkWidget *user_widget = NULL;
static GtkWidget *pass_widget = NULL;
static GtkWidget *game_widget = NULL;
static GtkWidget *char_widget = NULL;
static gboolean automated = FALSE;
static SgeData *sge_data = NULL;

static void cell_edited (GtkCellRendererText *cellrenderertext,
                gchar *arg1, gchar *arg2, gpointer user_data)
{
        GtkTreeIter iter;
        int n, id;
        char *old_name;

        debug ("edited: %s, %s\n", arg1, arg2);

        n = atoi (arg1);
        g_assert (gtk_tree_model_iter_nth_child (GTK_TREE_MODEL
                                (profile_dialog_list), &iter, NULL, n));

        gtk_tree_model_get (GTK_TREE_MODEL (profile_dialog_list), &iter,
                        NAME_COLUMN, &old_name,
                        ID_COLUMN, &id,
                        -1);

        if (old_name != NULL && strcmp (old_name, arg2) == 0) {
                return;
        }

        preferences_set_string (preferences_get_profile_key
                        (id, PREF_PROFILE_NAME), arg2);
}

static void changed_name (const char *key, gpointer user_data)
{
        int id, cur_id;
        GtkTreeIter iter;
        char *name;

        name = preferences_get_string (key);
        id = GPOINTER_TO_INT (user_data);
        g_assert (gtk_tree_model_get_iter_first (GTK_TREE_MODEL
                                (profile_dialog_list), &iter));
        gtk_tree_model_get (GTK_TREE_MODEL (profile_dialog_list), &iter,
                        ID_COLUMN, &cur_id, -1);
        while (cur_id != id) {
                g_assert (gtk_tree_model_iter_next (GTK_TREE_MODEL
                                        (profile_dialog_list), &iter));
                gtk_tree_model_get (GTK_TREE_MODEL (profile_dialog_list), &iter,
                                ID_COLUMN, &cur_id, -1);
        }

        gtk_list_store_set (profile_dialog_list, &iter, NAME_COLUMN, name, -1);
}

static void rebuild_profile_list (void)
{
        GSList *profile_list, *cur;
        GtkWidget *profile_view;
        GtkTreeSelection *selection;
        GtkTreePath *path;
        GtkTreeIter iter;
        static GSList *notifiers = NULL;

        if (notifiers != NULL) {
                for (cur = notifiers; cur != NULL; cur = cur->next) {
                        preferences_notify_remove (GPOINTER_TO_UINT
                                        (cur->data));
                }
                g_slist_free (notifiers);
                notifiers = NULL;
        }

        debug ("rebuilding list\n");

        profile_view = glade_xml_get_widget (warlock_xml, "profile_view");

        /* remember the currently selected highlight */
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (profile_view));
        if (profile_dialog_list != NULL && gtk_tree_selection_get_selected
                        (selection, NULL, &iter)) {
                path = gtk_tree_model_get_path (GTK_TREE_MODEL
                                (profile_dialog_list), &iter);
        } else {
                path = gtk_tree_path_new_first ();
        }

        /* create and fill the profile dialog list */
        profile_dialog_list = gtk_list_store_new (N_COLUMNS, G_TYPE_STRING,
                        G_TYPE_INT, G_TYPE_BOOLEAN);
        gtk_tree_view_set_model (GTK_TREE_VIEW (profile_view), 
                        GTK_TREE_MODEL (profile_dialog_list));

        profile_list = preferences_get_list (preferences_get_global_key
                        (PREF_PROFILES_INDEX), PREFERENCES_VALUE_INT);
        for (cur = profile_list; cur != NULL; cur = cur->next) {
                GtkTreeIter iter;
                int id;
                char *name;
                char *key;

                id = GPOINTER_TO_INT (cur->data);
                key = preferences_get_profile_key (id, PREF_PROFILE_NAME);
                name = preferences_get_string (key);
                gtk_list_store_append (profile_dialog_list, &iter);
                gtk_list_store_set (profile_dialog_list, &iter,
                                NAME_COLUMN, name, ID_COLUMN, id, -1);

                notifiers = g_slist_append (notifiers, GUINT_TO_POINTER
                                (preferences_notify_add
                                 (key, changed_name,
                                  GINT_TO_POINTER (id))));

                if (name == NULL) {
                        GtkTreeViewColumn *column;

                        path = gtk_tree_model_get_path (GTK_TREE_MODEL
                                        (profile_dialog_list), &iter);
                        column = gtk_tree_view_get_column (GTK_TREE_VIEW
                                        (profile_view), NAME_COLUMN);
                        gtk_widget_grab_focus (profile_view);
                        gtk_tree_view_set_cursor (GTK_TREE_VIEW (profile_view),
                                        path, column, TRUE);
                }
        }

        gtk_tree_selection_select_path (selection, path);
        automated = FALSE;
        load_profile (SGE_NONE, NULL);

        gtk_widget_show (profile_view);
}

static void changed_profile_list (const char *key, gpointer user_data)
{
        rebuild_profile_list ();
}

static void load_profile (SgeState state, gpointer user_data)
{
        char *password;
        char *username;
        char *game_name;
        char *char_name;
        GtkWidget *table;
        GtkWidget *label;
        GtkWidget *next_button;
        GtkWidget *prev_button;
        GtkWidget *button_box;
        GtkWidget *status_label;
        gboolean finished = FALSE;
        int table_pos;
        int id;

        profile_clear ();

        id = get_id ();
        if (id == -1) {
                return;
        }

        if (state == SGE_LOAD) {
                warlock_connection_init ();
                close_window ();
                return;
        }

        username = preferences_get_string (preferences_get_profile_key (id,
                                PREF_PROFILE_USERNAME));
        password = preferences_get_string (preferences_get_profile_key (id,
                                PREF_PROFILE_PASSWORD));
        game_name = preferences_get_string (preferences_get_profile_key (id,
                                PREF_PROFILE_GAME));
        char_name = preferences_get_string (preferences_get_profile_key (id,
                                PREF_PROFILE_CHARACTER));

        if (state == SGE_NONE) {
                if (username == NULL || password == NULL || game_name == NULL
                                || char_name == NULL) {
                        state = SGE_ACCOUNT;
                }
        }

        if (state == SGE_ACCOUNT || state == SGE_BAD_PASSWORD
                        || state == SGE_BAD_ACCOUNT
                        || state == SGE_INVALID_ACCOUNT) {
                user_widget = gtk_entry_new ();
                pass_widget = gtk_entry_new ();
                gtk_entry_set_visibility (GTK_ENTRY (pass_widget), FALSE);

                if (username != NULL) {
                        gtk_entry_set_text (GTK_ENTRY (user_widget), username);
                        g_free (username);
                }

                if (password != NULL) {
                        gtk_entry_set_text (GTK_ENTRY (pass_widget), password);
                        g_free (password);
                }
                finished = TRUE;
        }

        if (state >= SGE_MENU) {
                int i;

                g_assert (username != NULL);
                g_assert (password != NULL);

                user_widget = gtk_label_new (username);

                // block the password
                for (i = 0; password[i] != '\0'; i++) {
                        password[i] = '*';
                }
                pass_widget = gtk_label_new (password);
                g_free (username);
                g_free (password);
        }

        if (state >= SGE_ACCOUNT) {
                table = glade_xml_get_widget (warlock_xml, "profile_table");
                table_pos = 0;

                label = gtk_label_new ("Username:");
                gtk_table_attach_defaults (GTK_TABLE (table), label,
                                0, 1, table_pos, table_pos + 1);
                gtk_table_attach_defaults (GTK_TABLE (table), user_widget,
                        1, 2, table_pos, table_pos + 1);
                table_pos++;

                label = gtk_label_new ("Password:");
                gtk_table_attach_defaults (GTK_TABLE (table), label,
                                0, 1, table_pos, table_pos + 1);
                gtk_table_attach_defaults (GTK_TABLE (table), pass_widget,
                                1, 2, table_pos, table_pos + 1);
                table_pos++;
        }

        if (state == SGE_MENU) {
                GSList *cur;
                int i, pos;

                game_widget = gtk_combo_box_new_text ();
                pos = 0;
                for (cur = user_data, i = 0; cur != NULL;
                                cur = cur->next, i++) {
                        gtk_combo_box_append_text (GTK_COMBO_BOX (game_widget),
                                        cur->data);
                        if (game_name != NULL
                                        && strcmp (cur->data, game_name) == 0) {
                                pos = i;
                        }
                }
                gtk_combo_box_set_active (GTK_COMBO_BOX (game_widget), pos);
        }

        if (state > SGE_MENU) {
                g_assert (game_name != NULL);
                game_widget = gtk_label_new (game_name);
                g_free (game_name);
        }

        if (state >= SGE_MENU) {
                label = gtk_label_new ("Game:");
                gtk_table_attach_defaults (GTK_TABLE (table), label,
                                0, 1, table_pos, table_pos + 1);
                gtk_table_attach_defaults (GTK_TABLE (table), game_widget,
                                1, 2, table_pos, table_pos + 1);
                table_pos++;
        }

        if (state == SGE_CHARACTERS) {
                GSList *cur;
                int pos, i;

                char_widget = gtk_combo_box_new_text ();
                pos = 0;
                for (cur = user_data, i = 0; cur != NULL;
                                cur = cur->next, i++) {
                        gtk_combo_box_append_text (GTK_COMBO_BOX (char_widget),
                                        cur->data);
                        if (char_name != NULL
                                        && strcmp (cur->data, char_name) == 0) {
                                pos = i;
                        }
                }
                gtk_combo_box_set_active (GTK_COMBO_BOX (char_widget), pos);
        }
        
        if (state > SGE_CHARACTERS) {
                char_widget = gtk_label_new (char_name);
                g_free (char_name);
        }

        if (state >= SGE_CHARACTERS) {
                label = gtk_label_new ("Character:");
                gtk_table_attach_defaults (GTK_TABLE (table), label, 0, 1,
                                table_pos, table_pos + 1);
                gtk_table_attach_defaults (GTK_TABLE (table), char_widget,
                                1, 2, table_pos, table_pos + 1);
                table_pos++;
        }

        button_box = gtk_hbox_new (FALSE, 6);
        prev_button = NULL;

        switch (state) {
                case SGE_ACCOUNT:
                case SGE_BAD_PASSWORD:
                case SGE_BAD_ACCOUNT:
                case SGE_INVALID_ACCOUNT:
                        next_button = gtk_button_new_with_mnemonic ("_Connect");
                        gtk_box_pack_end (GTK_BOX (button_box), next_button,
                                        FALSE, TRUE, 6);
                        break;

                case SGE_NONE:
                        next_button = gtk_button_new_with_mnemonic ("_Connect");
                        prev_button = gtk_button_new_with_mnemonic ("_Edit");
                        gtk_box_pack_start (GTK_BOX (button_box), prev_button,
                                        FALSE, TRUE, 6);
                        gtk_box_pack_end (GTK_BOX (button_box), next_button,
                                        FALSE, TRUE, 6);
                        break;

                case SGE_CHARACTERS:
                        next_button = gtk_button_new_with_mnemonic ("_Start");
                        gtk_box_pack_end (GTK_BOX (button_box), next_button,
                                        FALSE, TRUE, 6);
                        break;

                default:
                        next_button = gtk_button_new_with_mnemonic ("_Next");
                        gtk_box_pack_end (GTK_BOX (button_box), next_button,
                                        FALSE, TRUE, 6);
        }

        /* attach the buttons to the table */
        gtk_table_attach_defaults (GTK_TABLE (table), button_box,
                        0, 2, table_pos, table_pos + 1);
        table_pos++;

        switch (state) {
                case SGE_BAD_PASSWORD:
                        status_label = gtk_label_new ("Invalid password.");
                        automated = FALSE;
                        state = SGE_ACCOUNT;
                        break;

                case SGE_INVALID_ACCOUNT:
                        status_label = gtk_label_new ("Invalid user name.");
                        automated = FALSE;
                        state = SGE_ACCOUNT;
                        break;

                case SGE_BAD_ACCOUNT:
                        status_label = gtk_label_new ("Billing problem.");
                        automated = FALSE;
                        state = SGE_ACCOUNT;
                        break;

                default:
                        status_label = gtk_label_new ("");
                        break;
        }

        /* attach the status label to the table */
        gtk_table_attach_defaults (GTK_TABLE (table), status_label,
                        0, 2, table_pos, table_pos + 1);
        table_pos++;

        gtk_widget_show_all (table);

        /* FIXME: when a button is pressed, we won't get a reaction from the
         * server right away. during that time, the buttons should be
         * insensitive and a notice should be displayed. sometime in the future
         * we should have this timeout
         */
        if (automated) {
                run_next (state);
        } else {
                /* attach listeners to the buttons if we aren't automated */
                g_signal_connect (next_button, "clicked", G_CALLBACK
                                (next_clicked), GINT_TO_POINTER (state));
                if (prev_button != NULL) {
                        g_signal_connect (prev_button, "clicked", G_CALLBACK
                                        (prev_clicked),
                                        GINT_TO_POINTER (state));
                }
        }

}

static void destroy_widget (GtkWidget *widget, gpointer user_data)
{
        gtk_widget_destroy (widget);
}

static void profile_clear (void)
{
        GtkTable *table;

        table = GTK_TABLE (glade_xml_get_widget (warlock_xml, "profile_table"));
        gtk_container_forall (GTK_CONTAINER (table), destroy_widget, NULL);
}

static void selection_changed (GtkTreeSelection *selection, gpointer user_data)
{
        GtkTreeIter iter;

        debug ("selection changed\n");
        if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
                profile_clear ();
        }

        automated = FALSE;
        load_profile (SGE_NONE, NULL);
}

void profile_dialog_init (void)
{
        GtkWidget *profile_view;
        GtkCellRenderer *renderer;
        GtkTreeViewColumn *column;
        GtkTreeSelection *selection;

        profile_view = glade_xml_get_widget (warlock_xml, "profile_view");

        renderer = gtk_cell_renderer_text_new ();
        g_signal_connect ((gpointer)renderer, "edited",
                        G_CALLBACK (cell_edited), NULL);
        g_object_set (G_OBJECT (renderer), "editable", TRUE,
                        "editable-set", TRUE, NULL);
        column = gtk_tree_view_column_new_with_attributes (_("Profiles"),
                        renderer, "text", 0, NULL);
        gtk_tree_view_append_column (GTK_TREE_VIEW (profile_view), column);
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (profile_view));
        gtk_tree_selection_set_mode (selection, GTK_SELECTION_SINGLE);
        g_signal_connect (selection, "changed", G_CALLBACK (selection_changed),
                        NULL);

        preferences_notify_add (preferences_get_global_key
                        (PREF_PROFILES_INDEX), changed_profile_list, NULL);

        rebuild_profile_list ();
}

void on_profile_new_clicked (GtkButton *button, gpointer user_data)
{
        GSList *list;
        int id;

        debug ("adding profile\n");

        list = preferences_get_list (preferences_get_global_key
                        (PREF_PROFILES_INDEX), PREFERENCES_VALUE_INT);

        // Make sure we get a unique id
        id = g_random_int_range (0, 0xFFFFFF);
        while (g_slist_find (list, GINT_TO_POINTER (id))) {
                id++;
        }

        list = g_slist_append (list, GINT_TO_POINTER (id));
        preferences_set_list (preferences_get_global_key (PREF_PROFILES_INDEX),
                        PREFERENCES_VALUE_INT, list);
}

void on_profile_delete_clicked (GtkButton *button, gpointer user_data)
{
        GSList *list;
        int id;

        id = get_id ();
        debug ("removing profile: %X\n", id);

        list = preferences_get_list (preferences_get_global_key
                        (PREF_PROFILES_INDEX), PREFERENCES_VALUE_INT);
        list = g_slist_remove_all (list, GINT_TO_POINTER (id));

        preferences_set_list (preferences_get_global_key (PREF_PROFILES_INDEX),
                        PREFERENCES_VALUE_INT, list);
}

static void run_next (SgeState state)
{
        const char *username, *password;
        char *game_name, *char_name, *key;
        GtkTreeModel *model;
        GtkTreeIter iter;
        int id;

        id = get_id ();

        switch (state) {
                case SGE_ACCOUNT:
                        username = gtk_entry_get_text (GTK_ENTRY (user_widget));
                        password = gtk_entry_get_text (GTK_ENTRY (pass_widget));

                        key = preferences_get_profile_key (id,
                                        PREF_PROFILE_USERNAME);
                        preferences_set_string (key, username);
                        g_free (key);
                        key = preferences_get_profile_key (id,
                                        PREF_PROFILE_PASSWORD);
                        preferences_set_string (key, password);
                        g_free (key);
                        sge_data = sge_init (username, password, load_profile);
                        break;

                case SGE_MENU:
                        /* get game name */
                        g_assert (gtk_combo_box_get_active_iter (GTK_COMBO_BOX
                                        (game_widget), &iter));
                        model = gtk_combo_box_get_model (GTK_COMBO_BOX
                                        (game_widget));
                        gtk_tree_model_get (model, &iter, 0, &game_name, -1);

                        key = preferences_get_profile_key (id,
                                        PREF_PROFILE_GAME);
                        preferences_set_string (key, game_name);
                        g_free (key);
                        sge_pick_game (sge_data, gtk_combo_box_get_active
                                        (GTK_COMBO_BOX (game_widget)));
                        g_free (game_name);
                        break;

                case SGE_CHARACTERS:
                        /* get character name */
                        gtk_combo_box_get_active_iter (GTK_COMBO_BOX
                                        (char_widget), &iter);
                        model = gtk_combo_box_get_model (GTK_COMBO_BOX
                                        (char_widget));
                        gtk_tree_model_get (model, &iter,
                                        0, &char_name, -1);
                        key = preferences_get_profile_key (id,
                                        PREF_PROFILE_CHARACTER);
                        preferences_set_string (key, char_name);
                        g_free (key);

                        sge_pick_character (sge_data, gtk_combo_box_get_active
                                        (GTK_COMBO_BOX (char_widget)));
                        break;

                case SGE_NONE:
                        automated = TRUE;
                        load_profile (SGE_ACCOUNT, NULL);
                        break;

                default:
                        g_assert_not_reached ();
        }
}

static void next_clicked (GtkButton *button, gpointer user_data)
{
        SgeState state;

        state = GPOINTER_TO_INT (user_data);
        run_next (state);
}

static void prev_clicked (GtkButton *button, gpointer user_data)
{
        SgeState state;
        char *key, *username, *password, *game_name;
        int id;

        id = get_id ();

        state = GPOINTER_TO_INT (user_data);

        switch (state) {
                case SGE_ACCOUNT:
                        break;

                case SGE_MENU:
                        key = preferences_get_profile_key (id,
                                        PREF_PROFILE_USERNAME);
                        username = preferences_get_string (key);
                        g_free (key);
                        key = preferences_get_profile_key (id,
                                        PREF_PROFILE_PASSWORD);
                        password = preferences_get_string (key);
                        g_free (key);
                        sge_data = sge_init (username, password, load_profile);
                        break;

                case SGE_CHARACTERS:
                        key = preferences_get_profile_key (id,
                                        PREF_PROFILE_GAME);
                        game_name = preferences_get_string (key);
                        g_free (key);
                        sge_pick_game (sge_data, gtk_combo_box_get_active
                                        (GTK_COMBO_BOX (game_widget)));
                        g_free (game_name);
                        break;

                case SGE_NONE:
                        automated = FALSE;
                        load_profile (SGE_ACCOUNT, NULL);
                        break;

                default:
                        g_assert_not_reached ();
        }
}

static int get_id (void)
{
        GtkWidget *view;
        GtkTreeIter iter;
        GtkTreeSelection *selection;
        int id;

        view = glade_xml_get_widget (warlock_xml, "profile_view");
        selection = gtk_tree_view_get_selection (GTK_TREE_VIEW (view));
        if (!gtk_tree_selection_get_selected (selection, NULL, &iter)) {
                return -1;
        }
        gtk_tree_model_get (GTK_TREE_MODEL (profile_dialog_list), &iter,
                        ID_COLUMN, &id, -1);

        return id;
}

static void close_window (void)
{
        automated = FALSE;
        load_profile (SGE_NONE, NULL);
        gtk_widget_hide (glade_xml_get_widget (warlock_xml, "profile_dialog"));
}

gboolean on_profile_dialog_delete_event (GtkWidget *widget, GdkEvent *event,
                gpointer user_data)
{
        close_window ();
        return TRUE;
}

void on_profile_close_clicked (GtkButton *button, gpointer user_data)
{
        close_window ();
}
