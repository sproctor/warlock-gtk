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

#include <stdarg.h>
#include <ctype.h>
#include <string.h>

#include <gtk/gtk.h>
#include <glib/gprintf.h>

#include "warlock.h"
#include "warlockview.h"
#include "highlight.h"
#include "debug.h"
#include "script.h"
#include "preferences.h"
#include "log.h"

typedef struct _WarlockView WarlockView;

struct _WarlockView {
        const char	*title;
	const char	*name;
        GtkWidget	*text_view;
        GtkTextBuffer	*text_buffer;
	GtkWidget	*widget;
	GtkWidget	*scrolled_window;
        char		*shown_key;
        GtkTextMark	*mark;
	WString		*buffer;
	GtkWidget	*menuitem;
	GSList		*listeners;
};

/* external global variables */
extern GtkTextTagTable	*highlight_tag_table;
extern GtkTextTagTable	*text_tag_table;
extern gboolean		 script_running;

/* local variables */
static GList		*views = NULL;
static WarlockView	*main_view = NULL;

static int		 max_buffer_size = -1;
static guint		 buffer_size_notification = 0;
static gboolean		 prompting = FALSE;

static void
change_text_color (const char *key, gpointer user_data)
{
        GtkWidget *text_view;
        GdkRGBA *color;

        text_view = user_data;
        color = preferences_get_color (key);
	gtk_widget_override_color (text_view, GTK_STATE_NORMAL, color);
}

static void
change_base_color (const char *key, gpointer user_data)
{
        GtkWidget *text_view;
        GdkRGBA *color;

        text_view = user_data;
        color = preferences_get_color (key);
	gtk_widget_override_background_color (text_view, GTK_STATE_NORMAL,
			color);
}

static void
change_font (const char *key, gpointer user_data)
{
        GtkWidget *text_view;
        PangoFontDescription *font;

        text_view = user_data;
	font = preferences_get_font (key);
	gtk_widget_override_font (text_view, font);
}

static void
warlock_view_create_text_view (WarlockView *warlock_view)
{
        PangoFontDescription *font;
        GdkRGBA *color;
        GtkTextIter iter;
	GtkWidget *text_view;
        
        text_view = gtk_text_view_new ();
        gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (text_view),
                        GTK_WRAP_WORD_CHAR);
        gtk_text_view_set_editable (GTK_TEXT_VIEW (text_view), FALSE);


	warlock_view->text_buffer = gtk_text_buffer_new (highlight_tag_table);
	warlock_view->text_view = text_view;
	gtk_text_view_set_buffer (GTK_TEXT_VIEW (text_view),
			warlock_view->text_buffer);

        gtk_text_buffer_get_end_iter (warlock_view->text_buffer, &iter);
        warlock_view->mark = gtk_text_buffer_create_mark
                (warlock_view->text_buffer, NULL, &iter, TRUE);

        /* set the text color */
        color = preferences_get_color (preferences_get_key
                        (PREF_DEFAULT_TEXT_COLOR));
        if (color == NULL) {
                color = g_new (GdkRGBA, 1);
                gdk_rgba_parse (color, "white");
        }
	gtk_widget_override_color (text_view, GTK_STATE_NORMAL, color);
        g_free (color);

        /* set the background color*/
        color = preferences_get_color (preferences_get_key
                        (PREF_DEFAULT_BASE_COLOR));
        if (color == NULL) {
                color = g_new (GdkRGBA, 1);
                gdk_rgba_parse (color, "black");
        }
	gtk_widget_override_background_color (text_view, GTK_STATE_NORMAL,
			color);
        g_free (color);

        /* set the font */
        font = preferences_get_font (preferences_get_key (PREF_DEFAULT_FONT));
        if (font == NULL) {
                font = pango_font_description_from_string ("sans");
        }
	gtk_widget_override_font (text_view, font);

        /* listen to gconf and change the text color when the gconf
         * value changes */
        preferences_notify_add (preferences_get_key (PREF_DEFAULT_TEXT_COLOR),
                        change_text_color, text_view);

        /* listen for background change */
        preferences_notify_add (preferences_get_key (PREF_DEFAULT_BASE_COLOR),
                        change_base_color, text_view);

        /* listen for font change */
        preferences_notify_add (preferences_get_key (PREF_DEFAULT_FONT),
                        change_font, text_view);
}

// cut off the beginning lines if we have too many
static void
warlock_view_trim (WarlockView *view)
{
        int size;

        // less than zero means unlimited
        if (max_buffer_size < 0)
                return;

        size = gtk_text_buffer_get_char_count (view->text_buffer);

        if (size > max_buffer_size) {
                GtkTextIter start, end;

                gtk_text_buffer_get_start_iter (view->text_buffer, &start);
                gtk_text_buffer_get_iter_at_offset (view->text_buffer, &end,
                                size - max_buffer_size);
                gtk_text_buffer_delete (view->text_buffer, &start, &end);
        }
}

// append a string to the WarlockView
// string needs to be valid UTF-8, and gets mangled, sorry.
static void
view_append (WarlockView *view, WString *string)
{
        GtkTextIter iter;
        GtkTextBuffer *buffer;
        GtkTextIter start, end;
        GList *list_current;
        GdkRectangle rect;
        int y, height;
        gboolean scroll;

        g_assert (view != NULL);

        buffer = view->text_buffer;
        list_current = NULL;

        // Get the end of the buffer
        gtk_text_buffer_get_end_iter (buffer, &iter);

        // test if we should scroll
        gtk_text_view_get_visible_rect (GTK_TEXT_VIEW (view->text_view), &rect);
        gtk_text_view_get_line_yrange (GTK_TEXT_VIEW(view->text_view), &iter,
                        &y, &height);
        if(((y + height) - (rect.y + rect.height)) > height){
                scroll = FALSE;
	} else {
                scroll = TRUE;
        }

        // highlighting stuff
        highlight_match (string);

        // FIXME the following lines should be done through hooks

        // script stuff
        script_match_string (string->string->str);

	// log it
	warlock_log (string->string->str);

        // Put the mark there that will stay in the same place when we insert
        // text as a reference for highlighting
        gtk_text_buffer_move_mark (buffer, view->mark, &iter);

        gtk_text_buffer_insert (buffer, &iter, string->string->str, -1);

        // markup the buffer with the tags from our string
        for (list_current = string->highlights; list_current != NULL;
                        list_current = list_current->next) {
                WHighlight *tmp = list_current->data;
                debug ("tag: %s offset: %d length: %d\n", tmp->tag_name,
                                tmp->offset, tmp->length);
                gtk_text_buffer_get_iter_at_mark (buffer, &start, view->mark);
                gtk_text_iter_forward_chars (&start, tmp->offset);
                end = start;
                gtk_text_iter_forward_chars (&end, tmp->length);
                gtk_text_buffer_apply_tag_by_name (buffer, tmp->tag_name,
                                &start, &end);
        }

        if (scroll) {
                // scroll the end mark on the screen.
                gtk_text_iter_set_line_offset (&iter, 0);
                gtk_text_buffer_move_mark (buffer, view->mark, &iter);
                gtk_text_view_scroll_to_mark (GTK_TEXT_VIEW (view->text_view),
                                view->mark, 0, TRUE, 1.0, 0.0);
        }

        // Cut off beginning lines that don't fit in the buffer.
        warlock_view_trim (view);

	if (view == main_view)
		prompting = FALSE;
}

// window resized signal handler
static gboolean
warlock_view_resized (GtkWindow *window, GdkEvent *event, gpointer data)
{
	int width, height;
	char *name = data;

	width = event->configure.width;
	height = event->configure.height;

	preferences_set_int (preferences_get_window_key (name, "width"), width);
	preferences_set_int (preferences_get_window_key (name, "height"),
			height);

	return FALSE;
}

// destroy the view
static void
view_hide (WarlockView *warlock_view)
{
        g_return_if_fail (warlock_view != NULL);
	if (warlock_view->widget == NULL) return;

	//if (!warlock_view->shown) return;
	 
        g_assert (GTK_IS_WIDGET (warlock_view->widget));

	gtk_widget_destroy (GTK_WIDGET (warlock_view->widget));
	warlock_view->widget = NULL;
}

static void
view_show (WarlockView *view)
{
	GtkWidget *frame;

        g_assert (view != NULL);

	/* FIXME: this is all an awful mess */
	if (view->widget == NULL) {
		int width, height;
		view->widget = gtk_window_new (GTK_WINDOW_TOPLEVEL);
		gtk_window_set_title (GTK_WINDOW (view->widget), view->title);
		frame = gtk_frame_new (view->title);
		gtk_container_add (GTK_CONTAINER (view->widget), frame);
		gtk_window_set_role (GTK_WINDOW (view->widget), view->name);
		width = preferences_get_int (preferences_get_window_key
				(view->name, "width"));
		height = preferences_get_int (preferences_get_window_key
				(view->name, "height"));
		gtk_window_set_default_size (GTK_WINDOW (view->widget), width,
				height);
		g_signal_connect (G_OBJECT (view->widget), "configure-event",
			G_CALLBACK (warlock_view_resized),
			(gpointer) view->name);
	} else {
		frame = view->widget;
	}

        view->scrolled_window = gtk_scrolled_window_new (NULL, NULL);
	view->buffer = w_string_new ("");
	warlock_view_create_text_view (view);

	/* save the status of the window */
	if (view->shown_key != NULL) {
		preferences_set_bool (view->shown_key, TRUE);
	}

        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW
			(view->scrolled_window), GTK_POLICY_NEVER,
			GTK_POLICY_ALWAYS);

        gtk_container_add (GTK_CONTAINER (view->scrolled_window),
			view->text_view);
        gtk_container_add (GTK_CONTAINER (frame),
			view->scrolled_window);

        gtk_widget_show_all (view->widget);
}

#if 0
static void
view_detach (EggDockItem *dockobject, gboolean b, WarlockView *view)
{
	debug ("view_detach called: %s\n", view->name);
	if (EGG_DOCK_OBJECT_ATTACHED (view->widget)) return;
	debug ("running\n");

	/* save state of the window */
	if (view->gconf_key != PREF_NONE) {
		preferences_set_bool (preferences_get_key (view->gconf_key),
				FALSE);
	}

	if (view->menuitem != NULL && gtk_check_menu_item_get_active
			(GTK_CHECK_MENU_ITEM (view->menuitem))) {
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM
				(view->menuitem), FALSE);
	}
}
#endif

static WarlockView *
warlock_view_init (const char *name, const char *title,
		const char *menu_name)
{
        WarlockView *warlock_view;
	gboolean shown;

        warlock_view = g_new (WarlockView, 1);
	warlock_view->widget = NULL;
	warlock_view->menuitem = NULL;
	warlock_view->name = name;
	warlock_view->title = title;

	views = g_list_append (views, warlock_view);

	if (menu_name != NULL) {
		warlock_view->shown_key = preferences_get_full_key 
			(g_strconcat (name, "-view", NULL));
		shown = preferences_get_bool (warlock_view->shown_key);
		warlock_view->menuitem = warlock_get_widget (menu_name);
		gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM
				(warlock_view->menuitem), shown);
	} else {
		warlock_view->shown_key = NULL;
		warlock_view->widget = warlock_get_widget ("main_window_frame");
		shown = TRUE;
		main_view = warlock_view;
	}

	if (shown) {
		view_show (warlock_view);
	} else {
		view_hide (warlock_view);
	}

/*
	g_signal_connect_after (G_OBJECT (warlock_view->widget), "detach",
			G_CALLBACK (view_detach), warlock_view);
*/

        return warlock_view;
}

static void
buffer_size_change (const char *key, gpointer user_data)
{
        GList *view;

        max_buffer_size = preferences_get_int (key);

        for (view = views; view != NULL; view = view->next) {
                warlock_view_trim (view->data);
        }
}

/*
static void
warlock_views_finish (void)
{
        preferences_notify_remove (buffer_size_notification);
}
*/

void
warlock_views_init (void)
{
        char *key;

        main_view = warlock_view_init ("main", _("Main"), NULL);

        warlock_view_init ("arrival", _("Arrivals and Departures"),
			"arrival_menu_item");
        warlock_view_init ("thoughts", _("Thoughts"), "thoughts_menu_item");
        warlock_view_init ("death", _("Deaths"), "deaths_menu_item");
        warlock_view_init ("familiar", _("Familiar"), "familiar_menu_item");

        // max buffer size init
        key = preferences_get_key (PREF_TEXT_BUFFER_SIZE);
        max_buffer_size = preferences_get_int (key);
        buffer_size_notification = preferences_notify_add (key,
                        buffer_size_change, NULL);

        //g_atexit (warlock_views_finish);
}

void
echo_f (const char *fmt, ...)
{
        va_list list;
        char *str;

        va_start (list, fmt);
        g_vasprintf (&str, fmt, list);
        va_end (list);

	echo (str);

	g_free (str);
}

void
echo (const char *str)
{
        WString *wstr;

        wstr = w_string_new (str);

        w_string_add_tag (wstr, ECHO_TAG, 0, -1);
        view_append (main_view, wstr);
}

static WarlockView *
get_view (const char *name)
{
	GList *cur;

	if (name == NULL) {
		return main_view;
	}

	for (cur = views; cur != NULL; cur = cur->next) {
		WarlockView *view;

		view = cur->data;
		if (strcmp (view->name, name) == 0) {
			return view;
		}
	}

	return NULL;
}

void
do_prompt (void)
{
        // FIXME figure out some way to abstract this function
        WString *string;

	script_got_prompt ();

	if (prompting)
		return;

	// If we have some text to display, do it
	if (main_view->buffer != NULL) {
		warlock_view_end_line (NULL);
	}

        string = w_string_new ("");

        if (script_running) {
                w_string_append_str (string, "[script:");
		w_string_append_str (string, g_strdup_printf ("%d", script_get_linenum()));
		w_string_append_str (string, "]");
        }

        w_string_append_c (string, '>');


	debug ("prompt: %s\n", string->string->str);

        view_append (main_view, string);
        w_string_free (string, TRUE);
	prompting = TRUE;
}

void
warlock_view_append (const char *name, const WString *w_string)
{
	WarlockView *view;

	view = get_view (name);
	if (view == NULL || view->widget == NULL) {
		view = main_view;
	}
	view->buffer = w_string_append (view->buffer, w_string);
}

void
warlock_view_end_line (const char *name)
{
	WarlockView *view;

	view = get_view (name);
	if (view == NULL || view->widget == NULL) {
		view = main_view;
	}
        view->buffer = w_string_append_c (view->buffer, '\n');
	// new line to insert after the prompt
	if (prompting) {
		view->buffer = w_string_prepend_c (view->buffer, '\n');
	}
        view_append (view, view->buffer);
        w_string_free (view->buffer, TRUE);
        view->buffer = NULL;
}

char *
warlock_view_get_text (const char *name)
{
	GtkTextIter start, end;
	WarlockView *view;

	view = get_view (name);
	gtk_text_buffer_get_start_iter (view->text_buffer, &start);
	gtk_text_buffer_get_end_iter (view->text_buffer, &end);

	return gtk_text_buffer_get_text (view->text_buffer, &start, &end,
			FALSE);
}

GtkWidget *
warlock_view_get_scrolled_window (const char *name)
{
	WarlockView *view;

	view = get_view (name);
	if (view == NULL)
		return NULL;
	
	return view->scrolled_window;
}

void
warlock_view_show (const char *name)
{
        view_show (get_view (name));
}

void
warlock_view_hide (const char *name)
{
        view_hide (get_view (name));
}

void
warlock_view_add_listener (const char *name, WarlockViewListener listener)
{
}
