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

#ifndef _DEBUG_H
#define _DEBUG_H

#ifdef DEBUG
#define debug(format, args...) g_printerr (__FILE__ ":%d: " format, __LINE__ \
		, ## args)
#else
#define debug(format, args...)
#endif

#define print_error(err) \
        if (err != NULL) g_warning ("** " __FILE__ ":%d - Error %d: %s\n", \
                        __LINE__, err->code, err->message)

#define print_gconf_error(err, key) \
        if (err != NULL) g_warning ("** " __FILE__ ":%d - Key: %s - " \
                        "Error %d: %s\n", __LINE__, key, err->code, \
                        err->message)

#endif
