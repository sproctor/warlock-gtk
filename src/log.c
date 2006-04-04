/* Warlock Front End
 * Copyright 2006 Sean Proctor, Marshall Culpepper
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

#include <time.h>

#include <gtk/gtk.h>
#include <glade/glade.h>

#include "debug.h"
#include "preferences.h"
#include "warlockview.h"

/* external global variables */
extern GladeXML *warlock_xml;

/* local variables */
static GIOChannel *log_file = NULL;

static char *
warlock_log_get_name (void)
{
	char *name;
	time_t t;

	name = g_new (char, 16);

	t = time (NULL);
	strftime (name, 16, "%G%m%d%H%M%S", localtime (&t));

	return name;
}

static void
save_log (const char *filename)
{
	GIOChannel *file;
	GError *err;
	gsize size;
	char *history;

	err = NULL;
	file = g_io_channel_new_file (filename, "w", &err);
	if (file == NULL) {
		print_error (err);
		return;
	}
	print_error (err);
	g_assert (err == NULL);

	history = warlock_view_get_text (NULL);

	g_io_channel_write_chars (file, history, -1, &size, &err);
	debug ("%d characters written to file\n", size);
	print_error (err);

	g_free (history);

	g_io_channel_close (file);
	g_io_channel_unref (file);
}

void
warlock_log (const char *str)
{
	if (log_file != NULL) {
		GError *err;
		gsize size;

		err = NULL;
		g_io_channel_write_chars (log_file, str, -1, &size, &err);
		print_error (err);

		err = NULL;
		g_io_channel_flush (log_file, &err);
		print_error (err);
	}
}

// to be called at program start and anytime the value of auto-log changes
static void
log_toggle (void)
{
	char *key;
	gboolean autolog;

	key = preferences_get_key (PREF_AUTO_LOG);
	autolog = preferences_get_bool (key);
	g_free (key);

	if (autolog && log_file == NULL) {
		GError *err;
		char *filename, *name, *path, *path_key;

		path_key = preferences_get_key (PREF_LOG_PATH);
		path = preferences_get_string (path_key);
		g_free (path_key);

		name = warlock_log_get_name ();

		filename = g_build_filename (path, name, NULL);

		g_free (name);
		g_free (path);

		err = NULL;
		log_file = g_io_channel_new_file (filename, "a", &err);
		if (log_file == NULL) {
			echo_f ("Error: \"%s\" for file \"%s\".", err->message,
					filename);
		}
	} else if (!autolog && log_file != NULL) {
		g_io_channel_close (log_file);
		g_io_channel_unref (log_file);
		log_file = NULL;
	}
}

static void
log_notify (const char *key, gpointer user_data)
{
	log_toggle ();
}

void
warlock_log_init (void)
{
	char *key;

	log_toggle ();

	key = preferences_get_key (PREF_AUTO_LOG);

	preferences_notify_add (key, log_notify, NULL);
}

/************************************************************************
 * Event handlers							*
 ************************************************************************/

void
on_save_history_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	char *name, *log_path, *filename;

	name = warlock_log_get_name ();
	log_path = preferences_get_string (preferences_get_key (PREF_LOG_PATH));
	filename = g_build_filename (log_path, name, NULL);

	save_log (filename);

	g_free (filename);
	g_free (name);
	g_free (log_path);
}

void
on_save_history_as_activate (GtkMenuItem *menuitem, gpointer user_data)
{
	GtkWidget *dialog;
	char *log_path, *name, *key;

	dialog = gtk_file_chooser_dialog_new ("Save File", GTK_WINDOW
			(glade_xml_get_widget (warlock_xml, "main_window")),
			GTK_FILE_CHOOSER_ACTION_SAVE,
			GTK_STOCK_CANCEL, GTK_RESPONSE_CANCEL,
			GTK_STOCK_SAVE, GTK_RESPONSE_ACCEPT,
			NULL);
	/* TODO enable this when we start requiring 2.8
	gtk_file_chooser_set_do_overwrite_confirmation (GTK_FILE_CHOOSER
			(dialog), TRUE);*/

	key = preferences_get_key (PREF_LOG_PATH);
	log_path = preferences_get_string (key);
	g_free (key);
	gtk_file_chooser_set_current_folder (GTK_FILE_CHOOSER (dialog),
			log_path);
	g_free (log_path);

	name = warlock_log_get_name ();
	gtk_file_chooser_set_current_name (GTK_FILE_CHOOSER (dialog), name);
	g_free (name);

	if (gtk_dialog_run (GTK_DIALOG (dialog)) == GTK_RESPONSE_ACCEPT) {
		char *filename;

		filename = gtk_file_chooser_get_filename (GTK_FILE_CHOOSER
				(dialog));
		save_log (filename);
		g_free (filename);
	}

	gtk_widget_destroy (dialog);
}
