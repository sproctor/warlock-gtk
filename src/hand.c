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

#include <gtk/gtk.h>

#include "hand.h"
#include "warlock.h"
#include "debug.h"

typedef struct {
	GtkWidget *label;
        gchar *text;
} Hand;

static Hand *left_hand = NULL;
static Hand *right_hand = NULL;

void hand_init (GtkWidget *left, GtkWidget *right)
{
        left_hand = g_new (Hand, 1);
        right_hand = g_new (Hand, 1);

        left_hand->label = left;
        right_hand->label = right;

        left_hand->text = "";
        right_hand->text = "";
}

static Hand *get_hand (int which)
{
        Hand *hand = NULL;

        if (which == LEFT_HAND) {
                hand = left_hand;
        } else if (which == RIGHT_HAND) {
                hand = right_hand;
        }

        g_assert (hand != NULL);

        return hand;
}

void hand_set_contents (int which, char *contents)
{
        if (contents == NULL) {
                contents = "";
        }

        debug ("hand contents: %s\n", contents);

	get_hand (which)->text = g_strdup(contents);

        gtk_label_set_text(GTK_LABEL(get_hand (which)->label), contents);
}

char *hand_get_contents (int which)
{
        return get_hand (which)->text;
}
