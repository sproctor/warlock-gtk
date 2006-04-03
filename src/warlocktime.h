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

#ifndef _WARLOCK_TIME_H
#define _WARLOCK_TIME_H

void warlock_time_init (void);
void new_roundtime (int end);
void warlock_set_time (int time);
double warlock_get_time (void);
void warlock_roundtime_wait (gboolean *script_running);
void warlock_pause_wait (int t, gboolean *script_running);
int warlock_get_roundtime_left (void);

#endif
