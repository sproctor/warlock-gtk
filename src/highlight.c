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

#include "warlock.h"
#include "debug.h"
#include "warlockstring.h"
#include "highlight.h"
#include "preferences.h"
#include "helpers.h"

/****
 * this value is higher than the highest highlight priority.
 */
static int next_priority = 0;

typedef struct _WarlockHighlight WarlockHighlight;

struct _WarlockHighlight {
        guint id;
        GRegex *regex;
        GSList *notify_connections;
};

static GSList *highlight_list = NULL;
GtkTextTagTable *highlight_tag_table = NULL;

static void changed_text (const char *key, gpointer user_data)
{
        GtkTextTag *tag;
        GdkRGBA *color;
        char *name;

        name = user_data;

        tag = gtk_text_tag_table_lookup (highlight_tag_table, name);
        g_assert (tag != NULL);

        color = preferences_get_color (key);

	g_object_set (G_OBJECT (tag), "foreground-rgba", color, NULL);
}

static void changed_background (const char *key, gpointer user_data)
{
        GtkTextTag *tag;
        GdkRGBA *color;
        char *name;

        name = user_data;

        tag = gtk_text_tag_table_lookup (highlight_tag_table, name);
        g_assert (tag != NULL);

        color = preferences_get_color (key);

	g_object_set (G_OBJECT (tag), "background-rgba", color, NULL);
}

static void changed_font (const char *key, gpointer user_data)
{
        GtkTextTag *tag;
        char *font;
        char *name;

        name = user_data;

        tag = gtk_text_tag_table_lookup (highlight_tag_table, name);
        g_assert (tag != NULL);

        font = preferences_get_string (key);

	g_object_set (G_OBJECT (tag), "font", font, NULL);
        g_free (font);
}

static gchar *mangle_name (guint id, guint n)
{
        return g_strdup_printf ("%06X.%d", id, n);
}

static void highlight_add_tag (const char *name, const GdkRGBA *text,
                const GdkRGBA *background, const char *font)
{
        GtkTextTag *tag;

        g_assert (highlight_tag_table != NULL);

        tag = gtk_text_tag_table_lookup (highlight_tag_table, name);

        g_assert (tag == NULL);

        tag = gtk_text_tag_new (name);

	g_object_set (G_OBJECT (tag),
			"foreground-rgba", text,
			"background-rgba", background,
			"font", font, NULL);
        gtk_text_tag_table_add (highlight_tag_table, tag);
        gtk_text_tag_set_priority (tag, next_priority);
        next_priority++;
}

static void highlight_match_one (WString *w_string, int id,
                const GRegex *regex)
{
        GMatchInfo *match_info;

        g_regex_match_full (regex, w_string->string->str,
                        w_string->string->len, 0, 0, &match_info, NULL);

        while (g_match_info_matches (match_info)) {
                gint count, i;

                count = g_match_info_get_match_count (match_info);
                for (i = 0; i < count && i < HIGHLIGHT_MATCHES; i++) {
                        char *tag_name;
                        gint start, end;

                        tag_name = mangle_name (id, i);
                        /* fetch_pos returns start == -1 for groups that did
                         * not participate in the match */
                        if (gtk_text_tag_table_lookup (highlight_tag_table,
                                                tag_name)
                                        && g_match_info_fetch_pos (match_info,
                                                i, &start, &end)
                                        && start != -1) {
                                w_string_add_tag (w_string, tag_name, start,
                                                end);
                        }
                        g_free (tag_name);
                }
                g_match_info_next (match_info, NULL);
        }

        g_match_info_free (match_info);
}

void highlight_match (WString *w_string)
{
        GSList *cur;

        for (cur = highlight_list; cur != NULL; cur = cur->next) {
                WarlockHighlight *highlight;

                highlight = cur->data;
		if (highlight->id != -1 && highlight->regex != NULL) {
			highlight_match_one (w_string, highlight->id,
                                        highlight->regex);
		}
        }
}

static void set_priority_tags (int id, gint priority)
{
	GtkTextTag *tag;
        int i, p;

        p = priority;
        for (i = 0; i < HIGHLIGHT_MATCHES; i++) {
                tag = gtk_text_tag_table_lookup (highlight_tag_table,
                        mangle_name (id, i));

                if (tag != NULL) {
                        if (p != priority + i) {
                                debug ("Missing a tag\n");
                        }

                        gtk_text_tag_set_priority (tag, p);
                }
        }
}

static void highlight_add_tags (guint id)
{
	/*
	 * don't call this function if you think there might be a tag by this
	 * name already, call highlight_check_tags first if that's the case
	 */
	int i;

        for (i = 0; i < HIGHLIGHT_MATCHES; i++) {
                GdkRGBA *text, *background;
                char *font;

                text = preferences_get_color (
                                preferences_get_highlight_match_key (id, i,
                                        PREF_HIGHLIGHT_MATCH_TEXT_COLOR));
                background = preferences_get_color (
                                preferences_get_highlight_match_key (id, i,
                                        PREF_HIGHLIGHT_MATCH_BASE_COLOR));
                font = preferences_get_string (
                                preferences_get_highlight_match_key (id, i,
                                        PREF_HIGHLIGHT_MATCH_FONT));
                highlight_add_tag (mangle_name (id, i), text, background, font);
        }
}

static GRegex *highlight_compile_regex (const char *string,
		gboolean case_sensitive)
{
	GRegex *regex;
	GRegexCompileFlags flags;
        GError *err = NULL;

        if (string == NULL || *string == '\0') {
                return NULL;
        }

        flags = G_REGEX_MULTILINE | G_REGEX_OPTIMIZE;

	if (!case_sensitive) {
		flags |= G_REGEX_CASELESS;
	}

	regex = g_regex_new (string, flags, 0, &err);

        if (regex == NULL) {
		// TODO change the following into a dialog
                g_warning ("Error compiling regex: %s\n", err->message);
                g_error_free (err);
                return NULL;
        }
	return regex;
}

/* get the priority of the match of string[id] */
static gint get_priority (guint id, guint n)
{
	GtkTextTag *tag;

        g_assert (n < HIGHLIGHT_MATCHES);
        tag = gtk_text_tag_table_lookup (highlight_tag_table,
                        mangle_name (id, n));
        g_assert (tag != NULL);
	return gtk_text_tag_get_priority (tag);
}

static void change_string (const char *key, gpointer user_data)
{
        gboolean case_sensitive;
        char *string;
        WarlockHighlight *highlight;

        highlight = user_data;

        string = preferences_get_string (preferences_get_highlight_key
                        (highlight->id, PREF_HIGHLIGHT_STRING));
        case_sensitive = preferences_get_bool (preferences_get_highlight_key
                        (highlight->id, PREF_HIGHLIGHT_CASE_SENSITIVE));
        if (highlight->regex != NULL) {
                g_regex_unref (highlight->regex);
        }
        highlight->regex = highlight_compile_regex (string, case_sensitive);
        g_free (string);
}

static void highlight_remove_tags (guint id)
{
        guint n;

        for (n = 0; n < HIGHLIGHT_MATCHES; n++) {
                GtkTextTag *tag;
                char *name;

                name = mangle_name (id, n);
                tag = gtk_text_tag_table_lookup (highlight_tag_table, name);
                g_assert (tag != NULL);
                gtk_text_tag_table_remove (highlight_tag_table, tag);
                debug ("removed tag %s\n", name);
                next_priority--;
        }
}

static void highlight_swap_with_next (GSList *highlight_link)
{
        int priority;
        GSList *temp;

        priority = get_priority (((WarlockHighlight*)highlight_link->data)->id,
                        0);
        g_assert (priority != -1);
        debug ("changing priority\n");
        set_priority_tags (((WarlockHighlight*)highlight_link->next->data)->id,
                        priority);

        temp = highlight_link->next;
        highlight_list = g_slist_remove_link (highlight_list, highlight_link);
        highlight_link->next = temp->next;
        temp->next = highlight_link;
}

/* remove highlight_link from the global highlight_list */
static void
highlight_remove (GSList *highlight_link)
{
        WarlockHighlight *highlight;
        GSList *cur;
        char *key;
        guint i, id;

        g_assert (highlight_link != NULL);
        highlight = highlight_link->data;
        g_assert (highlight != NULL);

        id = highlight->id;

        debug ("removing element: %X\n", id);

        /* remove the notifications on this highlight */
        for (cur = highlight->notify_connections; cur != NULL;
                        cur = cur->next) {
                preferences_notify_remove (GPOINTER_TO_INT (cur->data));
        }

        /* remove the tags, and delete the highlight from the list */
        highlight_remove_tags (id);
        highlight_list = g_slist_delete_link (highlight_list, highlight_link);

        /* unset the gconf keys */
        preferences_unset (preferences_get_highlight_key
                        (id, PREF_HIGHLIGHT_STRING));

        for (i = 0; i < HIGHLIGHT_MATCHES; i++) {
                preferences_unset (preferences_get_highlight_match_key (id, i,
                                        PREF_HIGHLIGHT_MATCH_TEXT_COLOR));
                preferences_unset (preferences_get_highlight_match_key (id, i,
                                        PREF_HIGHLIGHT_MATCH_BASE_COLOR));
                preferences_unset (preferences_get_highlight_match_key (id, i,
                                        PREF_HIGHLIGHT_MATCH_FONT));
        }

        preferences_unset (preferences_get_highlight_key
                        (id, PREF_HIGHLIGHT_CASE_SENSITIVE));
        key = g_strdup_printf ("%s/%06X", preferences_get_key (PREF_HIGHLIGHTS),
                        id);
        preferences_unset (key);
        g_free (key);

        if (highlight->regex != NULL) {
                g_regex_unref (highlight->regex);
        }
        g_slist_free (highlight->notify_connections);
        g_free (highlight);
}

static void highlight_add (guint id)
{
        WarlockHighlight *highlight;
        const char *string;
        gboolean case_sensitive;
        guint i;
        char *name;

        string = preferences_get_string (preferences_get_highlight_key (id,
                                PREF_HIGHLIGHT_STRING));
        case_sensitive = preferences_get_bool (preferences_get_highlight_key
                        (id, PREF_HIGHLIGHT_CASE_SENSITIVE));

        highlight = g_new (WarlockHighlight, 1);
        highlight->id = id;
        highlight->regex = highlight_compile_regex (string, case_sensitive);
        highlight->notify_connections = NULL;

        highlight_list = g_slist_append (highlight_list, highlight);

        highlight_add_tags (id);

        highlight->notify_connections = g_slist_append
                (highlight->notify_connections, GINT_TO_POINTER
                 (preferences_notify_add (preferences_get_highlight_key
                                          (id, PREF_HIGHLIGHT_STRING),
                                          change_string, highlight)));
        highlight->notify_connections = g_slist_append
                (highlight->notify_connections, GINT_TO_POINTER
                 (preferences_notify_add (preferences_get_highlight_key
                                          (id, PREF_HIGHLIGHT_CASE_SENSITIVE),
                                          change_string, highlight)));

        for (i = 0; i < HIGHLIGHT_MATCHES; i++) {
                name = mangle_name (id, i);
                highlight->notify_connections = g_slist_append
                        (highlight->notify_connections, GINT_TO_POINTER
                         (preferences_notify_add
                          (preferences_get_highlight_match_key
                           (id, i, PREF_HIGHLIGHT_MATCH_TEXT_COLOR),
                           changed_text, name)));

                highlight->notify_connections = g_slist_append
                        (highlight->notify_connections, GINT_TO_POINTER
                         (preferences_notify_add
                          (preferences_get_highlight_match_key
                           (id, i, PREF_HIGHLIGHT_MATCH_BASE_COLOR),
                           changed_background, name)));

                highlight->notify_connections = g_slist_append
                        (highlight->notify_connections, GINT_TO_POINTER
                         (preferences_notify_add
                          (preferences_get_highlight_match_key
                           (id, i, PREF_HIGHLIGHT_MATCH_FONT),
                           changed_font, name)));
        }
}

static void change_index (const char *key, gpointer user_data)
{
        GSList *list, *cur, *highlight;

        debug ("calling change_index\n");

        list = preferences_get_list (preferences_get_key
                        (PREF_HIGHLIGHTS_INDEX), PREFERENCES_VALUE_INT);

        /* find out if two elements are swapped, and switch priorities
         * and places in the highlight_list if they are */
        if (g_slist_length (highlight_list) == g_slist_length (list)) {
                for (cur = list, highlight = highlight_list;
                                cur != NULL && highlight != NULL;
                                cur = cur->next, highlight = highlight->next) {
                        if (cur->next != NULL && highlight->next != NULL
                                        && GPOINTER_TO_INT (cur->next->data) ==
                                        ((WarlockHighlight*)highlight->data)->id
                                        && highlight->next &&
                                        ((WarlockHighlight*)highlight->next
                                         ->data)->id
                                        == GPOINTER_TO_INT (cur->data)) {
                                highlight_swap_with_next (highlight);
                                break;
                        }
                }
        }

        /* if the old list longer, find and remove the extra highlight */
        if (g_slist_length (highlight_list) == g_slist_length (list) + 1) {
                for (cur = list, highlight = highlight_list;
                                highlight != NULL;
                                cur = cur->next, highlight = highlight->next) {
                        guint id;

                        id = ((WarlockHighlight*)highlight->data)->id;
                        if (cur == NULL || (id != GPOINTER_TO_INT (cur->data) 
                                                && (cur->next == NULL
                                                        || id != GPOINTER_TO_INT
                                                        (cur->next->data)))) {
                                highlight_remove (highlight);
                                break;
                        }
                }
        }

        /* if an item was added */
        if (g_slist_length (highlight_list) == g_slist_length (list) - 1) {
                for (cur = list, highlight = highlight_list;
                                cur != NULL;
                                cur = cur->next, highlight = highlight->next) {
                        guint id;

                        id = (guint)GPOINTER_TO_INT (cur->data);
                        if (highlight == NULL ||
                                        (((WarlockHighlight*)highlight->data)
                                         ->id != id &&
                                         (highlight->next == NULL ||
                                          id == ((WarlockHighlight*)highlight
                                                  ->next->data)->id))) {
                                highlight_add (id);
                                break;
                        }
                }
        }
}

static void
add_general_tag (char* tag_name, Preference text_pref,
                Preference background_pref, Preference font_pref)
{
        GdkRGBA *fg_color, *bg_color;
        char *font;
        char *key;

        /* initialize tag preferences */

        /* text color */
        key = preferences_get_key (text_pref);
        fg_color = preferences_get_color (key);
        preferences_notify_add (key, changed_text, tag_name);
        g_free (key);

        /* base color */
        key = preferences_get_key (background_pref);
        bg_color = preferences_get_color (key);
        preferences_notify_add (key, changed_background, tag_name);
        g_free (key);

        /* font */
        key = preferences_get_key (font_pref);
        font = preferences_get_string (key);
        preferences_notify_add (key, changed_font, tag_name);
        g_free (key);

        highlight_add_tag (tag_name, fg_color, bg_color, font);
}

void highlight_init (void)
{
        GSList *highlights, *cur;

        highlight_tag_table = gtk_text_tag_table_new();

        /* setup the tags */
        add_general_tag (TITLE_TAG, PREF_TITLE_TEXT_COLOR,
                        PREF_TITLE_BASE_COLOR, PREF_TITLE_FONT);
        add_general_tag (MONSTER_TAG, PREF_MONSTER_TEXT_COLOR,
                        PREF_MONSTER_BASE_COLOR, PREF_MONSTER_FONT);
        add_general_tag (ECHO_TAG, PREF_ECHO_TEXT_COLOR, PREF_ECHO_BASE_COLOR,
                        PREF_ECHO_FONT);

        /* add the user defined highlights */
        preferences_notify_add (preferences_get_key (PREF_HIGHLIGHTS_INDEX),
                        change_index, NULL);

        highlights = preferences_get_list (preferences_get_key
                        (PREF_HIGHLIGHTS_INDEX), PREFERENCES_VALUE_INT);

        for (cur = highlights; cur != NULL; cur = cur->next) {
                highlight_add (GPOINTER_TO_INT (cur->data));
        }
}
