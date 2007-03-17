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
notify_toggle (const char *key, gpointer user_data)
{
        GtkWidget *widget;
        gboolean bval;

        widget = user_data;
        bval = preferences_get_bool (key);
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), bval);
}

static void
init_toggle (char *name, Preference key)
{
        gboolean bval;
        GtkWidget *widget;

        widget = glade_xml_get_widget (warlock_xml, name);

        bval = preferences_get_bool (preferences_get_key (key));
        gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON (widget), bval);

        preferences_notify_add (preferences_get_key (key), notify_toggle,
                        widget);
}

static void
notify_spin (const char *key, gpointer user_data)
{
        GtkWidget *widget;
        int ival;

        widget = user_data;
        ival = preferences_get_int (key);
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), (double)ival);
}

static void
init_spin (char *name, Preference key)
{
        GtkWidget *widget;
        int ival;

        widget = glade_xml_get_widget (warlock_xml, name);

        ival = preferences_get_int (preferences_get_key (key));
        gtk_spin_button_set_value (GTK_SPIN_BUTTON (widget), (double)ival);

        preferences_notify_add (preferences_get_key (key), notify_spin,
                        widget);
}

static void
notify_entry (const char *key, gpointer user_data)
{
        GtkWidget *widget;
        const char *sval;

        widget = user_data;
        sval = preferences_get_string (key);
	if (sval == NULL)
		sval = "";
	if (strcmp (gtk_entry_get_text (GTK_ENTRY (widget)), sval) != 0)
		gtk_entry_set_text (GTK_ENTRY (widget), sval);
}

static void
init_entry (char *name, Preference key)
{
        GtkWidget *widget;
        char *sval;

        widget = glade_xml_get_widget (warlock_xml, name);

        sval = preferences_get_string (preferences_get_key (key));
	if (sval != NULL) {
		gtk_entry_set_text (GTK_ENTRY (widget), sval);
	}

        preferences_notify_add (preferences_get_key (key), notify_entry,
                        widget);
}

static void
notify_file (const char *key, gpointer user_data)
{
        GtkWidget *widget;
        char *sval;
        char *filename;

        widget = user_data;
        sval = preferences_get_string (key);

        filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER (widget));
        if (filename == NULL) {
                return;
        }

        if (strcmp (sval, filename) != 0) {
		if (!g_path_is_absolute (sval)) {
			char *tmp;

			tmp = sval;
			sval = g_build_filename (g_get_user_data_dir (),
					sval, NULL);
			g_free (tmp);
		}
		gtk_file_chooser_set_filename (GTK_FILE_CHOOSER (widget),
				sval);
        }
	g_free (sval);
}

static void
init_file (char *name, Preference key)
{
        GtkWidget *widget;
        char *sval;

        widget = glade_xml_get_widget (warlock_xml, name);

        sval = preferences_get_string (preferences_get_key (key));
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

        preferences_notify_add (preferences_get_key (key), notify_file,
                        widget);
}

static void
on_color_button_color_set (WarlockColorButton *widget, gpointer user_data)
{
        GdkColor *color;
        char *key;

        debug ("color button color set\n");
        color = warlock_color_button_get_color (widget);

        key = preferences_get_key (GPOINTER_TO_INT (user_data));
        preferences_set_color (key, color);
        g_free (key);
        if (color != NULL) {
                gdk_color_free (color);
        }
}

static void
on_font_button_font_set (WarlockFontButton *widget, gpointer user_data)
{
        char *key;
        char *font_name;

        font_name = warlock_font_button_get_font_name (widget);

        key = preferences_get_key (GPOINTER_TO_INT (user_data));
        preferences_set_string (key, font_name);
        g_free (key);
        g_free (font_name);
}

static void
notify_color (const char *key, gpointer user_data)
{
        GtkWidget *widget;
        GdkColor *color;

        widget = user_data;
        color = preferences_get_color (key);
        warlock_color_button_set_color (WARLOCK_COLOR_BUTTON (widget), color);
        if (color != NULL) {
                g_free (color);
        }
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

        preferences_notify_add (key, notify_color, widget);
        g_signal_connect (widget, "color-set", G_CALLBACK
                        (on_color_button_color_set), GINT_TO_POINTER (pref));
        g_free (key);
        gtk_widget_show (widget);
}

static void
notify_font (const char *key, gpointer user_data)
{
        GtkWidget *widget;
        const char *sval;

        widget = user_data;
        sval = preferences_get_string (key);
        warlock_font_button_set_font_name (WARLOCK_FONT_BUTTON (widget), sval);
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

        preferences_notify_add (key, notify_font, widget);
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

// TODO change the following functions to all use the same function
//      and map them via a hash or whatever, so we can get rid of the
//      thousand functions below

EXPORT
void
on_echo_checkbutton_toggled (GtkToggleButton *togglebutton, gpointer user_data)
{
        preferences_set_bool (preferences_get_key (PREF_ECHO),
                        gtk_toggle_button_get_active (togglebutton));
}

EXPORT
void
on_sneak_checkbutton_toggled (GtkToggleButton *togglebutton, gpointer user_data)
{
        preferences_set_bool (preferences_get_key (PREF_AUTO_SNEAK),
                        gtk_toggle_button_get_active (togglebutton));
}

EXPORT
void
on_log_checkbutton_toggled (GtkToggleButton *togglebutton, gpointer user_data)
{
        preferences_set_bool (preferences_get_key (PREF_AUTO_LOG),
                        gtk_toggle_button_get_active (togglebutton));
}

EXPORT
void
on_text_buffer_size_spinbutton_value_changed (GtkSpinButton *spinbutton,
                gpointer user_data)
{
        preferences_set_int (preferences_get_key (PREF_TEXT_BUFFER_SIZE),
                        gtk_spin_button_get_value_as_int (spinbutton));
}

EXPORT
void
on_command_size_spinbutton_value_changed (GtkSpinButton *spinbutton,
                gpointer user_data)
{
        preferences_set_int (preferences_get_key (PREF_COMMAND_SIZE),
                        gtk_spin_button_get_value_as_int (spinbutton));
}

EXPORT
void
on_history_size_spinbutton_value_changed (GtkSpinButton *spinbutton,
                gpointer user_data)
{
        preferences_set_int (preferences_get_key (PREF_COMMAND_HISTORY_SIZE),
                        gtk_spin_button_get_value_as_int (spinbutton));
}

EXPORT
void
on_script_prefix_entry_changed (GtkEditable *editable, gpointer user_data)
{
        preferences_set_string (preferences_get_key (PREF_SCRIPT_PREFIX),
                        gtk_entry_get_text (GTK_ENTRY (editable)));
}

EXPORT
void
on_script_path_filechooserbutton_selection_changed (GtkFileChooser *filechooser,
                gpointer user_data)
{
        char *filename, *key, *old_filename;

        debug ("script filename changed\n");

        filename = gtk_file_chooser_get_filename (filechooser);
        if (filename == NULL) {
                return;
        }

        // update the setting if it needs to be
        key = preferences_get_key (PREF_SCRIPT_PATH);
	old_filename = preferences_get_string (key);
        if (old_filename == NULL || strcmp (filename, old_filename) != 0) {
                preferences_set_string (key, filename);
        }
	g_free (old_filename);
        g_free (key);

        g_free (filename);
}

EXPORT
void
on_log_path_filechooserbutton_selection_changed (GtkFileChooser *filechooser,
                gpointer user_data)
{
        char *filename, *key, *sval;

        debug ("log filename changed\n");

        filename = gtk_file_chooser_get_filename (filechooser);
        if (filename == NULL) {
                return;
        }

        // update the setting if it needs to be
        key = preferences_get_key (PREF_LOG_PATH);
	sval = preferences_get_string (key);
        if (sval == NULL || strcmp (filename, preferences_get_string (key))
			!= 0) {
                preferences_set_string (key, filename);
        }
	g_free (sval);
        g_free (key);

        g_free (filename);
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
