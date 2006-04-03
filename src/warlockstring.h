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

#ifndef _WARLOCKSTRING_H
#define _WARLOCKSTRING_H

typedef struct {
        const char *tag_name;
        int offset;
        int length;
} WHighlight;

typedef struct {
        GString *string;
        GList *highlights;
} WString;

WString *w_string_new (const char *string);
WString *w_string_new_len (const char *string, int len);
WString *w_string_add_tag (WString *w_string, const char *tag_name,
                int start_offset, int end_offset);
void w_string_free (WString *w_string, gboolean free_string);
WString *w_string_dup (const WString *w_string);
WString *w_string_append (WString *w_string, const WString *append);
WString *w_string_append_c (WString *w_string, char c);
WString *w_string_prepend_c (WString *w_string, char c);

#endif
