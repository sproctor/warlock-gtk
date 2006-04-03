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

#include <gtk/gtk.h>
#include <glade/glade.h>
#include <glib/gprintf.h>

#include "warlock.h"
#include "warlockview.h"
#include "highlight.h"
#include "debug.h"
#include "script.h"
#include "preferences.h"
#include "log.h"

#ifdef CONFIG_SPIDERMONKEY
#include "jsscript.h"
#endif

typedef struct _WarlockView WarlockView;

struct _WarlockView {
        const char      *title;
        GtkWidget 	*text_view;
        GtkTextBuffer 	*text_buffer;
	GtkWidget	*widget;
	GtkWidget	*pane;
	gboolean	shown;
	WarlockView	*prev;
	WarlockView	*next;
        Preference      gconf_key;
        GtkTextMark     *mark;
};

/* external global variables */
extern GladeXML *warlock_xml;
extern GtkTextTagTable *highlight_tag_table;
extern GtkTextTagTable *text_tag_table;
extern gboolean script_running;

/* local variables */
static WarlockView *top_view = NULL;

static GList *views = NULL;
static WarlockView *main_view = NULL,
        *arrival_view = NULL,
        *death_view = NULL,
        *familiar_view = NULL,
	*thought_view = NULL;

static WString *main_buffer = NULL,
        *arrival_buffer = NULL,
        *death_buffer = NULL,
        *familiar_buffer = NULL,
	*thought_buffer = NULL;

// are we on the line immediately after a prompt?
static gboolean prompting = FALSE;

static int max_buffer_size = -1;
static guint buffer_size_notification = 0;

static void change_text_color (const char *key, gpointer user_data)
{
        GtkWidget *text_view;
        GdkColor *color;

        text_view = user_data;
        color = preferences_get_color (key);
	gtk_widget_modify_text (text_view, GTK_STATE_NORMAL, color);
}

static void change_base_color (const char *key, gpointer user_data)
{
        GtkWidget *text_view;
        GdkColor *color;

        text_view = user_data;
        color = preferences_get_color (key);
	gtk_widget_modify_base (text_view, GTK_STATE_NORMAL, color);
}

static void change_font (const char *key, gpointer user_data)
{
        GtkWidget *text_view;
        PangoFontDescription *font;

        text_view = user_data;
	font = preferences_get_font (key);
	gtk_widget_modify_font (text_view, font);
}

static WarlockView* warlock_view_new (GtkWidget *text_view)
{
        PangoFontDescription *font;
        GdkColor *color;
	WarlockView *warlock_view;
        GtkTextIter iter;
        
        gtk_text_view_set_wrap_mode (GTK_TEXT_VIEW (text_view),
                        GTK_WRAP_WORD_CHAR);
        gtk_text_view_set_editable (GTK_TEXT_VIEW (text_view), FALSE);

        warlock_view = g_new (WarlockView, 1);

	views = g_list_append (views, warlock_view);

	warlock_view->text_buffer = gtk_text_buffer_new (highlight_tag_table);
	warlock_view->text_view = text_view;
	gtk_text_view_set_buffer (GTK_TEXT_VIEW (text_view),
			warlock_view->text_buffer);

	warlock_view->widget = NULL;
	warlock_view->pane = NULL;
	warlock_view->prev = NULL;
	warlock_view->next = NULL;
	warlock_view->shown = TRUE;
        gtk_text_buffer_get_end_iter (warlock_view->text_buffer, &iter);
        warlock_view->mark = gtk_text_buffer_create_mark
                (warlock_view->text_buffer, NULL, &iter, TRUE);

        /* set the text color */
        color = preferences_get_color (preferences_get_key
                        (PREF_DEFAULT_TEXT_COLOR));
        if (color == NULL) {
                color = g_new (GdkColor, 1);
                gdk_color_parse ("white", color);
        }
	gtk_widget_modify_text (text_view, GTK_STATE_NORMAL, color);
        g_free (color);

        /* set the background color*/
        color = preferences_get_color (preferences_get_key
                        (PREF_DEFAULT_BASE_COLOR));
        if (color == NULL) {
                color = g_new (GdkColor, 1);
                gdk_color_parse ("black", color);
        }
	gtk_widget_modify_base (text_view, GTK_STATE_NORMAL, color);
        g_free (color);

        /* set the font */
        font = preferences_get_font (preferences_get_key (PREF_DEFAULT_FONT));
        if (font == NULL) {
                font = pango_font_description_from_string ("sans");
        }
	gtk_widget_modify_font (text_view, font);

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

        return warlock_view;
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
static void warlock_view_append (WarlockView *view, WString *string)
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

#ifdef CONFIG_SPIDERMONKEY
	// JS script stuff
	js_script_got_line (string->string->str);
#endif

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
}

// destroy the view
static void warlock_view_hide (WarlockView *warlock_view)
{
        // FIXME: we leak widgets
        GtkWidget *new_container;
        GtkWidget *parent;
        WarlockView *prev_view;

        warlock_view->shown = FALSE;

        parent = gtk_widget_get_parent (warlock_view->widget);
        gtk_container_remove (GTK_CONTAINER (parent), warlock_view->widget);

        prev_view = warlock_view->prev;

        new_container = gtk_widget_get_parent (warlock_view->pane);
        gtk_container_remove (GTK_CONTAINER (warlock_view->pane),
                        prev_view->widget);
        gtk_container_remove (GTK_CONTAINER (new_container),
                        warlock_view->pane);

        if (warlock_view->next == NULL) {
                g_assert (warlock_view == top_view);
                gtk_container_add (GTK_CONTAINER (new_container),
                                prev_view->widget);

                prev_view->next = NULL;
                top_view = prev_view;
        } else {
                WarlockView *next_view;
                GtkWidget *box;

                g_assert (warlock_view != top_view);

                next_view = warlock_view->next;
                box = gtk_widget_get_parent (next_view->pane);

                gtk_container_remove (GTK_CONTAINER (box), next_view->pane);
                gtk_paned_add2 (GTK_PANED (next_view->pane),
                                prev_view->widget);
                gtk_container_add (GTK_CONTAINER (new_container),
                                next_view->pane);
                next_view->prev = prev_view;
                prev_view->next = next_view;
        }

        gtk_widget_show_all (new_container);
        g_object_unref (warlock_view->pane);

        /* save state of the window */
        preferences_set_bool (preferences_get_key (warlock_view->gconf_key),
                        FALSE);
}

static void warlock_view_show (WarlockView *warlock_view)
{
        GtkWidget *vbox;
        GtkWidget *top_view_parent;

        g_assert (warlock_view != NULL);
        g_assert (GTK_IS_WIDGET (warlock_view->widget));

        warlock_view->pane = g_object_ref (gtk_vpaned_new ());
        vbox = gtk_vbox_new (FALSE, 0);

        top_view_parent = gtk_widget_get_parent (top_view->widget);
        g_object_ref (top_view->widget);
        gtk_container_remove (GTK_CONTAINER (top_view_parent),
                        top_view->widget);
        gtk_paned_add2 (GTK_PANED (warlock_view->pane), top_view->widget);
        gtk_paned_add1 (GTK_PANED (warlock_view->pane), vbox);
        gtk_box_pack_start_defaults (GTK_BOX (vbox), warlock_view->widget);
        gtk_container_add (GTK_CONTAINER (top_view_parent),
                        warlock_view->pane);

        gtk_widget_show_all (warlock_view->pane);

        warlock_view->prev = top_view;
        warlock_view->next = NULL;
        warlock_view->shown = TRUE;
        top_view->next = warlock_view;
        top_view = warlock_view;

        /* save the status of the window */
        preferences_set_bool (preferences_get_key (warlock_view->gconf_key),
                        TRUE);
}

static WarlockView *
warlock_view_init (Preference key, const char *title, const char *menu_name)
{
        GtkWidget *text_view;
        GtkWidget *handle_box;
        GtkWidget *frame;
        GtkWidget *scrolled_window;
        GtkWidget *menu_item;
        WarlockView *warlock_view;

        handle_box = g_object_ref (gtk_handle_box_new ());
        frame = gtk_frame_new (title);
        scrolled_window = gtk_scrolled_window_new (NULL, NULL);
        text_view = gtk_text_view_new ();

        warlock_view = warlock_view_new (text_view);
        warlock_view->widget = handle_box;
        gtk_handle_box_set_shadow_type (GTK_HANDLE_BOX (handle_box),
                        GTK_SHADOW_NONE);

        warlock_view->shown = preferences_get_bool (preferences_get_key (key));
        warlock_view->gconf_key = key;

        gtk_scrolled_window_set_policy (GTK_SCROLLED_WINDOW (scrolled_window),
                        GTK_POLICY_NEVER, GTK_POLICY_ALWAYS);

        gtk_container_add (GTK_CONTAINER (scrolled_window), text_view);
        gtk_container_add (GTK_CONTAINER (frame), scrolled_window);
        gtk_container_add (GTK_CONTAINER (handle_box), frame);

        menu_item = glade_xml_get_widget (warlock_xml, menu_name);
        if (warlock_view->shown) {
                warlock_view_show (warlock_view);
        }
        gtk_check_menu_item_set_active (GTK_CHECK_MENU_ITEM (menu_item),
                        warlock_view->shown);

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

        main_view = warlock_view_new (glade_xml_get_widget (warlock_xml,
                                "main_text_view"));
        main_view->widget = glade_xml_get_widget (warlock_xml,
                        "main_view_frame");
        top_view = main_view;

        arrival_view = warlock_view_init (PREF_ARRIVAL_VIEW,
                     _("Arrivals and Departures"), "arrival_menu_item");
        thought_view = warlock_view_init (PREF_THOUGHT_VIEW, _("Thoughts"),
                        "thoughts_menu_item");
        death_view = warlock_view_init (PREF_DEATH_VIEW, _("Deaths"),
                        "deaths_menu_item");
        familiar_view = warlock_view_init (PREF_FAMILIAR_VIEW, _("Familiar"),
                        "familiar_menu_item");

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
        warlock_view_append (main_view, wstr);
}

void do_prompt (void)
{
        // FIXME figure out some way to abstract this function
        GString *string;
#ifdef CONFIG_SPIDERMONKEY
        char* js_prompt_string;
#endif

        if (prompting)
                return;

        string = g_string_new ("");

        if (script_running) {
                g_string_append (string, "[script]");
        }

#ifdef CONFIG_SPIDERMONKEY
        js_prompt_string = js_script_get_prompt_string ();
        if (js_prompt_string != NULL) {
                g_string_append (string, js_prompt_string);
                g_free (js_prompt_string);
        }
#endif

        g_string_append_c (string, '>');
        warlock_view_append (main_view, w_string_new (string->str));
        prompting = TRUE;
        g_string_free (string, TRUE);
}

void main_view_append (const WString *w_string)
{
        main_buffer = w_string_append (main_buffer, w_string);
}

void main_view_end_line (void)
{
        if (prompting) {
                main_buffer = w_string_prepend_c (main_buffer, '\n');
                prompting = FALSE;
        }
        main_buffer = w_string_append_c (main_buffer, '\n');
        warlock_view_append (main_view, main_buffer);
        w_string_free (main_buffer, TRUE);
        main_buffer = NULL;
}

char *
main_view_get_text (void)
{
	GtkTextIter start, end;

	gtk_text_buffer_get_start_iter (main_view->text_buffer, &start);
	gtk_text_buffer_get_end_iter (main_view->text_buffer, &end);

	return gtk_text_buffer_get_text (main_view->text_buffer, &start, &end,
			FALSE);
}

void arrival_view_show (void)
{
        warlock_view_show (arrival_view);
}

void arrival_view_hide (void)
{
        warlock_view_hide (arrival_view);
}

void arrival_view_append (const WString *string)
{
        arrival_buffer = w_string_append (arrival_buffer, string);
}

void arrival_view_end_line (void)
{
        if (arrival_view != NULL && arrival_view->shown) {
                arrival_buffer = w_string_append_c (arrival_buffer, '\n');
                warlock_view_append (arrival_view, arrival_buffer);
        } else {
                main_view_append (arrival_buffer);
                main_view_end_line ();
        }
        w_string_free (arrival_buffer, TRUE);
        arrival_buffer = NULL;
}

void death_view_show (void)
{
        warlock_view_show (death_view);
}

void death_view_hide (void)
{
        warlock_view_hide (death_view);
}

void death_view_append (const WString *string)
{
        death_buffer = w_string_append (death_buffer, string);
}

void death_view_end_line (void)
{
        if (death_view != NULL && death_view->shown) {
                death_buffer = w_string_append_c (death_buffer, '\n');
                warlock_view_append (death_view, death_buffer);
        } else {
                main_view_append (death_buffer);
                main_view_end_line ();
        }
        w_string_free (death_buffer, TRUE);
        death_buffer = NULL;
}

void familiar_view_show (void)
{
        warlock_view_show (familiar_view);
}

void familiar_view_hide (void)
{
        warlock_view_hide (familiar_view);
}

void familiar_view_append (const WString *string)
{
        familiar_buffer = w_string_append (familiar_buffer, string);
}

void familiar_view_end_line (void)
{
        if (familiar_view != NULL && familiar_view->shown) {
                familiar_buffer = w_string_append_c (familiar_buffer, '\n');
                warlock_view_append (familiar_view, familiar_buffer);
        } else {
                w_string_add_tag (familiar_buffer, MONSTER_TAG, 0, -1);
                main_view_append (familiar_buffer);
                main_view_end_line ();
        }
        w_string_free (familiar_buffer, TRUE);
        familiar_buffer = NULL;
}

void thought_view_show (void)
{
        warlock_view_show (thought_view);
}

void thought_view_hide (void)
{
        warlock_view_hide (thought_view);
}

void thought_view_append (const WString *string)
{
        thought_buffer = w_string_append (thought_buffer, string);
}

void thought_view_end_line (void)
{
        if (thought_view != NULL && thought_view->shown) {
                thought_buffer = w_string_append_c (thought_buffer, '\n');
                warlock_view_append (thought_view, thought_buffer);
        } else {
                w_string_add_tag (thought_buffer, MONSTER_TAG, 0, -1);
                main_view_append (thought_buffer);
                main_view_end_line ();
        }
        w_string_free (thought_buffer, TRUE);
        thought_buffer = NULL;
}
