/* Warlock Front End
 * Copyright (C) 2001 Chris Schantz.
 * Copyright 2005 Sean Proctor, Mashall Culpepper
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

#include <gtk/gtk.h>

#include "warlock.h"
#include "compass.h"
#include "debug.h"

#define ON   0
#define OFF  1

#define NORTH 'A'
#define NORTHEAST 'B'
#define EAST 'C'
#define SOUTHEAST 'D'
#define SOUTH 'E'
#define SOUTHWEST 'F'
#define WEST 'G'
#define NORTHWEST 'H'
#define UP 'I'
#define DOWN 'J'
#define OUT 'K'

static const char* file_table[3][4] = {
        {
                "northwest",
                "north",
                "northeast",
                "up"
        },
        {
                "west",
                NULL,
                "east",
                "out"
        },
        {
                "southwest",
                "south",
                "southeast",
                "down"
        }
};

static GdkPixbuf *pixbuf_table[3][4][2];
static GtkWidget *image_table[3][4];
static gboolean on_table[3][4];

static void compass_init_item (GtkWidget *compass_table, guint x, guint y)
{
        GError *err;

        if (file_table[y][x] != NULL) {
		char *on_file, *off_file, tmp[256];

		g_snprintf (tmp, 256, "%s_on.xpm", file_table[y][x]);
		on_file = g_build_filename (PACKAGE_DATA_DIR, PACKAGE, tmp,
				NULL);
		err = NULL;
		pixbuf_table[y][x][ON] = gdk_pixbuf_new_from_file
			(on_file, &err);
		print_error (err);
		g_free (on_file);

		g_snprintf (tmp, 256, "%s.xpm", file_table[y][x]);
		off_file = g_build_filename (PACKAGE_DATA_DIR, PACKAGE, tmp,
				NULL);
		err = NULL;
		pixbuf_table[y][x][OFF] = gdk_pixbuf_new_from_file
			(off_file, &err);
		print_error (err);
		g_free (off_file);
        } else {
		char *filename;

		filename = g_build_filename (PACKAGE_DATA_DIR, PACKAGE,
				"blank.xpm", NULL);
                err = NULL;
                pixbuf_table[y][x][OFF] = gdk_pixbuf_new_from_file (filename,
				&err);
                print_error (err);
                pixbuf_table[y][x][ON] = NULL;
		g_free (filename);
        }

        image_table[y][x] = gtk_image_new_from_pixbuf (pixbuf_table[y][x][OFF]);

        gtk_table_attach_defaults (GTK_TABLE (compass_table),
                        image_table[y][x], x, x + 1, y, y + 1);

        on_table[y][x] = OFF;
}

void compass_init (GtkWidget *compass_table)
{
        int x, y;
        int cols, rows;

        g_assert (compass_table != NULL);

        cols = (GTK_TABLE (compass_table))->ncols;
        rows = (GTK_TABLE (compass_table))->nrows;

        for (y = 0; y < rows; y++)
                for (x = 0; x < cols; x++)
                        compass_init_item (compass_table, x, y);
        gtk_widget_show_all (compass_table);
}

static void compass_toggle_item (guint x, guint y, gboolean on)
{
        g_assert (pixbuf_table[y][x][on] != NULL);

        gtk_image_set_from_pixbuf (GTK_IMAGE (image_table[y][x]),
                        pixbuf_table[y][x][on]);
        on_table[y][x] = on;
}

void compass_set_directions(char *directions)
{
        guint x, y, i;
        gboolean new_table[3][4];

        for (y = 0; y < 3; y++)
                for (x = 0; x < 4; x++)
                        new_table[y][x] = OFF;

        for (i = 0; directions[i] != '\0' && directions[i] != ' '; i++) {
                switch(directions[i]) {
                        case NORTH:
                                x = 1;
                                y = 0;
                                break;

                        case NORTHWEST:
                                x = 0;
                                y = 0;
                                break;

                        case WEST:
                                x = 0;
                                y = 1;
                                break;

                        case SOUTHWEST:
                                x = 0;
                                y = 2;
                                break;

                        case SOUTH:
                                x = 1;
                                y = 2;
                                break;

                        case SOUTHEAST:
                                x = 2;
                                y = 2;
                                break;

                        case EAST:
                                x = 2;
                                y = 1;
                                break;

                        case NORTHEAST:
                                x = 2;
                                y = 0;
                                break;

                        case OUT:
                                x = 3;
                                y = 1;
                                break;

                        case UP:
                                x = 3;
                                y = 0;
                                break;

                        case DOWN:
                                x = 3;
                                y = 2;
                                break;

                        default:
				debug ("Directions: %d\n", directions[i]);
                                g_assert_not_reached ();
                                break;
                }
                new_table[y][x] = ON;
        }	

        for (y = 0; y < 3; y++)
                for (x = 0; x < 4; x++)
                        if (new_table[y][x] != on_table[y][x])
                                compass_toggle_item (x, y, new_table[y][x]);
}
