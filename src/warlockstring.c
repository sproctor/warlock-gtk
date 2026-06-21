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

#include <glib.h>

#include "warlockstring.h"

static void
w_highlight_free (WHighlight *highlight)
{
        g_free (highlight->tag_name);
        g_free (highlight);
}

WString *w_string_new (const char *string)
{
        WString *w_string;

        w_string = g_new (WString, 1);

        w_string->string = g_string_new (string);
        w_string->highlights = NULL;

        return w_string;
}

WString *w_string_new_len (const char *string, int len)
{
        WString *w_string;

        w_string = g_new (WString, 1);

        w_string->string = g_string_new_len (string, len);
        w_string->highlights = NULL;

        return w_string;
}

/* w_string: name of the string to add the tag to
 * tag_name: name of the tag to add
 * start_offset: number of characters into the string to add the tag
 * end_offset: number of characters into the string to end the tag, -1 for the
               end
 */
WString *w_string_add_tag (WString *w_string, const char *tag_name,
                int start_offset, int end_offset)
{
        WHighlight *highlight;

	if (w_string == NULL) {
		return NULL;
	}

        g_assert (start_offset != -1);
        if (end_offset == -1) {
                end_offset = w_string->string->len;
        }

	highlight = g_new (WHighlight, 1);

        highlight->tag_name = g_strdup (tag_name);
        highlight->offset = start_offset;
        highlight->length = end_offset - start_offset;

        w_string->highlights = g_list_append (w_string->highlights, highlight);

	return w_string;
}

void w_string_free (WString *w_string, gboolean free_string)
{
	if (w_string == NULL)
		return;

        g_string_free (w_string->string, free_string);
        g_list_free_full (w_string->highlights, (GDestroyNotify) w_highlight_free);
        g_free (w_string);
}

WString *w_string_dup (const WString *w_string)
{
        WString *new_string;
        GList *current;

	if (w_string == NULL) {
		return NULL;
	}

        new_string = g_new (WString, 1);

        new_string->string = g_string_new (w_string->string->str);
        new_string->highlights = NULL;

        /* deep copy: each WString owns its own highlight elements */
        for (current = w_string->highlights; current != NULL;
                        current = current->next) {
                WHighlight *src = current->data;
                WHighlight *copy = g_new (WHighlight, 1);

                copy->tag_name = g_strdup (src->tag_name);
                copy->offset = src->offset;
                copy->length = src->length;
                new_string->highlights = g_list_append (new_string->highlights,
                                copy);
        }

        return new_string;
}

WString *w_string_append (WString *w_string, const WString *append)
{
        GList *current_append;
        int len;

	if (append == NULL) {
		return w_string;
	}

        if (w_string == NULL) {
                return w_string_dup (append);
        }

        len = w_string->string->len;
        w_string->string = g_string_append (w_string->string,
                        append->string->str);

        for (current_append = append->highlights; current_append != NULL;
                        current_append = current_append->next) {
                WHighlight *tmp = g_new (WHighlight, 1);
                WHighlight *current_data = current_append->data;
                tmp->tag_name = g_strdup (current_data->tag_name);
                tmp->offset = current_data->offset + len;
                tmp->length = current_data->length;
		w_string->highlights = g_list_append (w_string->highlights,
				tmp);
        }

        return w_string;
}

WString *
w_string_append_str (WString *w_string, const char *str)
{
        if (w_string == NULL) {
                return w_string_new (str);
        }

	w_string->string = g_string_append (w_string->string, str);

        return w_string;
}

WString *w_string_append_c (WString *w_string, char c)
{
        if (w_string == NULL) {
                w_string = w_string_new ("");
        }

        w_string->string = g_string_append_c (w_string->string, c);

        return w_string;
}

WString *w_string_prepend_c (WString *w_string, char c)
{
        GList *current;

	if (w_string == NULL) {
		w_string = w_string_new ("");
	}

        w_string->string = g_string_prepend_c (w_string->string, c);

        for (current = w_string->highlights; current != NULL;
                        current = current->next) {
                ((WHighlight *)current->data)->offset++;
        }

        return w_string;
}
