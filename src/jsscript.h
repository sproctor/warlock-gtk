/* Warlock Front End
 * Copyright 2003 Sean Proctor, Marshall Culpepper
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

#ifndef __JS_SCRIPT_H__
#define __JS_SCRIPT_H__

gboolean js_script_init (void);
void js_script_load (const char *filename, int argc, const char **argv);
void js_script_moved (void);
void js_script_got_line (const char* line);
gboolean js_script_stop (guint id);
void js_script_stop_all (void);
gboolean js_script_toggle (guint id);
void js_script_toggle_all (void);
char* js_script_get_prompt_string (void);

#endif /* __JS_SCRIPT_H__ */
