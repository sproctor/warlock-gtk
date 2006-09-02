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

#ifndef _WARLOCKVIEW_H
#define _WARLOCKVIEW_H

#include "warlockstring.h"

typedef gboolean (*WarlockViewListener) (const char *line);

void warlock_views_init (void);
void warlock_view_show (const char *name);
void warlock_view_hide (const char *name);
void echo_f (const char *fmt, ...);
void echo (const char *str);
void do_prompt (void);
void warlock_view_append (const char *name, const WString *w_string);
void warlock_view_end_line (const char *name);
char *warlock_view_get_text (const char *name);
GtkWidget *warlock_view_get_scrolled_window (const char *name);
void warlock_view_add_listener (const char *name, WarlockViewListener listener);

#endif
