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

#ifndef _ENTRY_H
#define _ENTRY_H

void warlock_entry_init (GtkWidget *widget);
void warlock_history_next (void);
void warlock_history_prev (void);
void warlock_history_last (void);
void warlock_entry_append (char *str);
void warlock_entry_append_c (char c);
gboolean warlock_entry_is_focus (void);
void warlock_entry_grab_focus (void);
int warlock_entry_get_position (void);
void warlock_entry_set_position (int position);
void warlock_entry_set_text (char *str);
gboolean warlock_entry_give_key_event (GdkEventKey *event);
void warlock_entry_submit (void);
const char *warlock_entry_get_text (void);
void warlock_entry_clear (void);

#endif
