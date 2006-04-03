#include <gtk/gtk.h>
#include <string.h>
#include "warlock_error.h"
#include "preferences.h"
#include "highlight_string.h"
#include "trigger.h"
#include "user_prefs.h"
#include "trigger_dialog.h"

static char *titles[] = { "Match", "Command" };
extern UserPrefs *user_prefs;

int trig_current_row;

GtkWidget *trig_window,
	*trig_list,
	*trig_match_entry,
	*trig_command_entry,
	*trig_add,
	*trig_delete,
	*trig_save,
	*trig_close;

gint trig_add_clicked (GtkWidget *wid, gpointer data)
{
	Trigger *trigger = g_new(Trigger, 1);
	char *match = gtk_editable_get_chars(GTK_EDITABLE(trig_match_entry),0,-1);
	char *command = gtk_editable_get_chars(GTK_EDITABLE(trig_command_entry),0,-1);

	if (strlen(match) == 0)
	{
		WARLOCK_WARN("You must specify a string to match");
		return TRUE;
	} else if (strlen(command) == 0)
	{
		WARLOCK_WARN("You must specify a command to send");
		return TRUE;
	}

	trigger->match = g_strdup(match);
	trigger->command = g_strdup(command);

	g_ptr_array_add(user_prefs->triggers, trigger);	
	trigger_dialog_reload_list();
	return TRUE;
}

gint trig_close_clicked (GtkWidget *wid, gpointer data)
{
	trigger_dialog_hide();
	return TRUE;
}

gint trig_save_clicked (GtkWidget *wid, gpointer data)
{

	Trigger *trigger = (Trigger *) user_prefs->triggers->pdata[trig_current_row];
	char *match = gtk_editable_get_chars(GTK_EDITABLE(trig_match_entry),0,-1);
	char *command = gtk_editable_get_chars(GTK_EDITABLE(trig_command_entry),0,-1);

	if (strlen(match) == 0)
	{
		WARLOCK_WARN("You must specify a string to match");
		return TRUE;
	} else if (strlen(command) == 0)
	{
		WARLOCK_WARN("You must specify a command to send");
		return TRUE;
	}

	g_free(trigger->match);
	trigger->match = g_strdup(match);
	trigger->command = g_strdup(command);
	trigger_dialog_reload_list();

	return TRUE;
}

gint trig_delete_clicked (GtkWidget *wid, gpointer data)
{
	Trigger *trigger = (Trigger *) user_prefs->triggers->pdata[trig_current_row];

	g_free(trigger->match);
	g_free(trigger->command);
	g_ptr_array_remove(user_prefs->triggers, trigger);
	trigger_dialog_reload_list();

	return TRUE;
}

gint trig_list_select_row (GtkWidget *wid, gint row, gint col, GdkEventButton *event, gpointer data)
{
	char *match, *command;
	trig_current_row = row;
	gtk_clist_get_text(GTK_CLIST(trig_list), row, 0, &match);
	gtk_clist_get_text(GTK_CLIST(trig_list), row, 1, &command);

	gtk_entry_set_text(GTK_ENTRY(trig_match_entry), match);
	gtk_entry_set_text(GTK_ENTRY(trig_command_entry), command);

	return TRUE;
}

void trigger_dialog_reload_list()
{
	if (trig_list != NULL)
	{
		int i = 0;
		gtk_clist_clear(GTK_CLIST(trig_list));

		for(; i < user_prefs->triggers->len; i++)
		{
			Trigger *trigger = (Trigger *) user_prefs->triggers->pdata[i];
			char *d[] = {trigger->match, trigger->command};
	
			gtk_clist_append(GTK_CLIST(trig_list), d);
		}

		gtk_clist_optimal_column_width(GTK_CLIST(trig_list), 0);
	}
}

void trigger_dialog_init ()
{
	GtkWidget *hbox, *vbox, *hbox1;
	GtkWidget *trig_scroller;

	trig_window = gtk_window_new(GTK_WINDOW_TOPLEVEL);
	gtk_window_set_title(GTK_WINDOW(trig_window), "Triggers");

	trig_list = gtk_clist_new_with_titles(2, titles);

	trig_scroller = gtk_scrolled_window_new(NULL, NULL);
	gtk_scrolled_window_set_policy(GTK_SCROLLED_WINDOW(trig_scroller), GTK_POLICY_NEVER, GTK_POLICY_AUTOMATIC);

	trigger_dialog_reload_list();

	trig_match_entry = gtk_entry_new();
	trig_command_entry = gtk_entry_new();

	trig_add = gtk_button_new_with_label("Add");
	trig_close = gtk_button_new_with_label("Close");
	trig_delete = gtk_button_new_with_label("Delete");
	trig_save = gtk_button_new_with_label("Save");

	gtk_signal_connect(GTK_OBJECT(trig_list), "select-row", GTK_SIGNAL_FUNC(trig_list_select_row), NULL);
	gtk_signal_connect(GTK_OBJECT(trig_add), "clicked", GTK_SIGNAL_FUNC(trig_add_clicked), NULL);
	gtk_signal_connect(GTK_OBJECT(trig_close), "clicked", GTK_SIGNAL_FUNC(trig_close_clicked), NULL);
	gtk_signal_connect(GTK_OBJECT(trig_delete), "clicked", GTK_SIGNAL_FUNC(trig_delete_clicked), NULL);
	gtk_signal_connect(GTK_OBJECT(trig_save), "clicked", GTK_SIGNAL_FUNC(trig_save_clicked), NULL);

	hbox = gtk_hbox_new(FALSE, 0);
	hbox1 = gtk_hbox_new(FALSE, 0);
	vbox = gtk_vbox_new(FALSE, 0);

	pack(hbox1, trig_delete, FALSE, FALSE, 0);

	pack(hbox, trig_match_entry, TRUE, TRUE, 0);
	pack(hbox, trig_command_entry, TRUE, TRUE, 0);
	pack(hbox, trig_save, FALSE, FALSE, 0);
	pack(hbox, trig_add, FALSE, FALSE, 0);

	gtk_container_add(GTK_CONTAINER(trig_scroller), trig_list);
	pack(vbox, trig_scroller, TRUE, TRUE, 0);
	pack(vbox, hbox1, FALSE, FALSE, 0);
	pack(vbox, hbox, FALSE, FALSE, 0);
	pack(vbox, trig_close, FALSE, FALSE, 0);

	gtk_container_add(GTK_CONTAINER(trig_window), vbox);
}

void trigger_dialog_show ()
{
	if ( trig_window != NULL )
	{
		gtk_widget_show_all(trig_window);
	}
}

void trigger_dialog_hide ()
{
	if ( trig_window != NULL )
	{
		gtk_widget_hide_all(trig_window);
	}
}
