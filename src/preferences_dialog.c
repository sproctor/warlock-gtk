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

#include "preferences.h"
#include "preferences_dialog.h"
#include "warlock.h"
#include "debug.h"
#include "helpers.h"
#include "warlockcolorbutton.h"
#include "warlockfontbutton.h"
#include "warlockview.h"

extern GladeXML *warlock_xml;

static void
on_checkbutton_toggled (GtkToggleButton *togglebutton, gpointer user_data)
{
	Preference pref;

	pref = GPOINTER_TO_INT (user_data);
        preferences_set_bool (preferences_get_key (pref),
                        gtk_toggle_button_get_active (togglebutton));
}

static void
init_toggle (char *name, Preference pref)
{
        gboolean bval;
        GtkWidget *widget;

        widget = glade_xml_get_widget (warlock_xml, name);

        bval = preferences_get_bool (preferences_get_key (pref));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), bval);

	g_signal_connect (G_OBJECT (widget), "toggled", G_CALLBACK
			(on_checkbutton_toggled), GINT_TO_POINTER (pref));
}

static void
on_spinbutton_value_changed (GtkSpinButton *spinbutton,
                gpointer user_data)
{
	Preference pref;

	pref = GPOINTER_TO_INT (user_data);
        preferences_set_int (preferences_get_key (pref),
                        gtk_spin_button_get_value_as_int (spinbutton));
}

static void
init_spin (char *name, Preference pref)
{
        GtkWidget *widget;
        int ival;

        widget = glade_xml_get_widget (warlock_xml, name);

        ival = preferences_get_int (preferences_get_key (pref));
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), (double)ival);

	g_signal_connect (G_OBJECT (widget), "value_changed", G_CALLBACK
			(on_spinbutton_value_changed), GINT_TO_POINTER (pref));
}

static void
on_entry_changed (GtkEditable *editable, gpointer user_data)
{
	Preference pref;

	pref = GPOINTER_TO_INT (user_data);
        preferences_set_string (preferences_get_key (pref),
                        gtk_entry_get_text (GTK_ENTRY (editable)));
}

static void
init_entry (char *name, Preference pref)
{
        GtkWidget *widget;
        char *sval;

        widget = glade_xml_get_widget (warlock_xml, name);

        sval = preferences_get_string (preferences_get_key (pref));
	if (sval != NULL) {
		gtk_entry_set_text (GTK_ENTRY (widget), sval);
	}

	g_signal_connect (G_OBJECT (widget), "changed", G_CALLBACK
			(on_entry_changed), GINT_TO_POINTER (pref));
}

static void
on_path_filechooserbutton_selection_changed (GtkFileChooser *filechooser,
                gpointer user_data)
{
        char *filename, *key, *old_filename;
	Preference pref;

	pref = GPOINTER_TO_INT (user_data);
        key = preferences_get_key (pref);

        filename = gtk_file_chooser_get_filename (filechooser);
        if (filename == NULL) {
		debug ("%s changed to NULL\n", key);
                return;
        }

        debug ("%s changed: %s\n", key, filename);

        // update the setting if it needs to be
	// do we really need to do this?
	old_filename = preferences_get_string (key);
        if (old_filename == NULL || strcmp (filename, old_filename) != 0) {
                preferences_set_string (key, filename);
        }
	g_free (old_filename);
        g_free (key);

        g_free (filename);
}

static void
init_file (char *name, Preference pref)
{
        GtkWidget *widget;
        char *sval;

        widget = glade_xml_get_widget (warlock_xml, name);

        sval = preferences_get_string (preferences_get_key (pref));
	debug("sval: %s\n", sval);
	if (sval != NULL) {
		// if we got a relative path, make it relative to home dir
		if (!g_path_is_absolute (sval)) {
			char *tmp;

			tmp = sval;
			sval = g_build_filename (g_get_user_data_dir (),
					sval, NULL);
			g_free (tmp);
		}
		assure_directory (sval);
		gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (widget), sval);
	}

	g_signal_connect (G_OBJECT (widget), "selection_changed", G_CALLBACK
			(on_path_filechooserbutton_selection_changed),
			GINT_TO_POINTER (pref));
}

static void
on_color_button_color_set (WarlockColorButton *widget, gpointer user_data)
{
        GdkColor *color;
        char *key;
	Preference pref;

        debug ("color button color set\n");
        color = warlock_color_button_get_color (widget);

	pref = GPOINTER_TO_INT (user_data);
        key = preferences_get_key (pref);
        preferences_set_color (key, color);
        g_free (key);
        if (color != NULL) {
                gdk_color_free (color);
        }
}

static void
on_font_button_font_set (WarlockFontButton *widget, gpointer user_data)
{
        char *key, *font_name;
	Preference pref;

        font_name = warlock_font_button_get_font_name (widget);

	pref = GPOINTER_TO_INT (user_data);
        key = preferences_get_key (pref);
        preferences_set_string (key, font_name);
        g_free (key);
        g_free (font_name);
}

static void
init_color (char *name, Preference pref)
{
        GtkWidget *widget;
        char *key;
        GdkColor *color;
        GtkWidget *box;

        box = glade_xml_get_widget (warlock_xml, name);
        widget = warlock_color_button_new ();

        gtk_box_pack_start (GTK_BOX (box), widget, FALSE, TRUE, 0);
        gtk_box_reorder_child (GTK_BOX (box), widget, 0);

        key = preferences_get_key (pref);
        color = preferences_get_color (key);

        warlock_color_button_set_color (WARLOCK_COLOR_BUTTON (widget), color);

        if (color != NULL) {
                g_free (color);
        }

        g_signal_connect (widget, "color-set", G_CALLBACK
                        (on_color_button_color_set), GINT_TO_POINTER (pref));
        g_free (key);
        gtk_widget_show (widget);
}

static void
init_font (char *name, Preference pref)
{
        GtkWidget *widget;
        GtkWidget *box;
        const char *sval;
        char *key;

        box = glade_xml_get_widget (warlock_xml, name);
        widget = warlock_font_button_new ();

        gtk_box_pack_start (GTK_BOX (box), widget, FALSE, TRUE, 0);
        gtk_box_reorder_child (GTK_BOX (box), widget, 0);

        key = preferences_get_key (pref);
        sval = preferences_get_string (key);

        if (sval != NULL) {
                warlock_font_button_set_font_name (WARLOCK_FONT_BUTTON (widget),
                                sval);
        }

        g_free (key);
        g_signal_connect (widget, "font-set", G_CALLBACK
                        (on_font_button_font_set), GINT_TO_POINTER (pref));
        gtk_widget_show (widget);
}

void
preferences_dialog_init (void)
{
	// General preferences
        init_toggle ("echo_checkbutton", PREF_ECHO);
        init_toggle ("sneak_checkbutton", PREF_AUTO_SNEAK);
        init_toggle ("log_checkbutton", PREF_AUTO_LOG);
        init_spin ("text_buffer_size_spinbutton", PREF_TEXT_BUFFER_SIZE);
        init_spin ("command_size_spinbutton", PREF_COMMAND_SIZE);
        init_spin ("history_size_spinbutton", PREF_COMMAND_HISTORY_SIZE);
        init_entry ("script_prefix_entry", PREF_SCRIPT_PREFIX);

	// Colors
        init_color ("monster_text_box", PREF_MONSTER_TEXT_COLOR);
        init_color ("monster_base_box", PREF_MONSTER_BASE_COLOR);
        init_font ("monster_font_box", PREF_MONSTER_FONT);
        init_color ("title_text_box", PREF_TITLE_TEXT_COLOR);
        init_color ("title_base_box", PREF_TITLE_BASE_COLOR);
        init_font ("title_font_box", PREF_TITLE_FONT);
        init_color ("echo_text_box", PREF_ECHO_TEXT_COLOR);
        init_color ("echo_base_box", PREF_ECHO_BASE_COLOR);
        init_font ("echo_font_box", PREF_ECHO_FONT);
        init_color ("default_text_box", PREF_DEFAULT_TEXT_COLOR);
        init_color ("default_base_box", PREF_DEFAULT_BASE_COLOR);
        init_font ("default_font_box", PREF_DEFAULT_FONT);

	// Paths
        init_file ("script_path_filechooserbutton", PREF_SCRIPT_PATH);
	init_file ("log_path_filechooserbutton", PREF_LOG_PATH);
}

EXPORT
void
on_preferences_close_button_clicked (GtkButton *button, gpointer user_data)
{
        gtk_widget_hide (glade_xml_get_widget (warlock_xml,
				"preferences_dialog"));
}

EXPORT
gboolean
on_preferences_dialog_delete_event (GtkWidget *widget, GdkEvent *event,
                gpointer user_data)
{
        gtk_widget_hide (glade_xml_get_widget (warlock_xml,
				"preferences_dialog"));

	return TRUE;
}
