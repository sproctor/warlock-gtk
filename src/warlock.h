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

#ifndef _WARLOCK_H
#define _WARLOCK_H

#define _(x)		(x)

#ifdef __WIN32__
# define EXPORT __declspec (dllexport)
#else
# define EXPORT 
#endif

enum {
	LEFT_HAND = 1,
	RIGHT_HAND
};

void warlock_exit (void);
void warlock_disconnect (void);
void warlock_send (const char *fmt, ...);
void warlock_bell (void);
void warlock_init (void);
void warlock_connection_init (void);
GtkWidget *warlock_get_widget (const char *widget_name);

#endif
