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

#ifndef _STATUS_H
#define _STATUS_H

void status_init (GtkWidget *status_table);
void status_set (const char *status_string);
gboolean is_hidden (void);
gboolean is_standing (void);
gboolean is_sitting (void);
gboolean is_kneeling (void);
gboolean is_lying (void);
gboolean is_dead (void);
gboolean is_bleeding (void);
gboolean is_stunned (void);
gboolean is_webbed (void);
gboolean is_joined (void);

#endif
