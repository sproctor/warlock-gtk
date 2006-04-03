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

#include <string.h>

#include <glib.h>
#include <glib/gstdio.h>
#include <gdk/gdk.h>

static const char *valid_directions[] = {
	"n", "no", "nor", "nort", "north",
	"s", "so", "sou", "sout", "south", 
	"e", "ea", "eas", "east",
	"w", "we", "wes", "west",
	"ne", "northe", "northea", "northeas", "northeast",
	"nw", "northw", "northwe", "northwes", "northwest",
	"sw", "southw", "southwe", "southwes", "southwest",
	"se", "southe", "southea", "southeas", "southeast",
        "o", "ou", "out",
        "u", "up",
        "d", "do", "dow", "down",
};

GdkColor *
gdk_color_from_string (const char *string)
{
        if (string == NULL || *string == '\0' || strcmp (string, "(null)") == 0
			|| strcmp (string, "NONE") == 0) {
		return NULL;
	} else {
		GdkColor *color;
		color = g_new (GdkColor, 1);
		gdk_color_parse (string, color);
		return color;
	}
}

char *
gdk_color_to_string (const GdkColor *color)
{
        if (color == NULL) {
                return NULL;
        } else {
                return g_strdup_printf ("#%04X%04X%04X", color->red,
                                color->green, color->blue);
        }
}

gboolean
is_direction (const char *dir)
{
	int i;

	for(i = 0; i < sizeof(valid_directions) / sizeof(const char*); i++) {
		if (strcmp(valid_directions[i], dir) == 0)
			return TRUE;
	}

	return FALSE;
}

void
assure_directory (const char *dir)
{
	char *path;

	if(g_file_test (dir, G_FILE_TEST_IS_DIR)) {
		return;
	}

	g_assert (!g_file_test (dir, G_FILE_TEST_EXISTS));

	path = g_path_get_dirname (dir);
	assure_directory (path);
	g_free (path);

	g_mkdir (dir, 0755);
}
