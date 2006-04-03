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

void warlock_views_init (void);
void arrival_view_show (void);
void arrival_view_hide (void);
void death_view_show (void);
void death_view_hide (void);
void thought_view_show (void);
void thought_view_hide (void);
void familiar_view_show (void);
void familiar_view_hide (void);
void echo_f (const char *fmt, ...);
void echo (const char *str);
void do_prompt (void);
void main_view_append (const WString *w_string);
void main_view_end_line (void);
char * main_view_get_text (void);
void arrival_view_append (const WString *string);
void arrival_view_end_line (void);
void death_view_append (const WString *string);
void death_view_end_line (void);
void familiar_view_append (const WString *string);
void familiar_view_end_line (void);
void thought_view_append (const WString *string);
void thought_view_end_line (void);

#endif
