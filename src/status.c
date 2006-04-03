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

#include <stdlib.h>
#include <string.h>

#include <gtk/gtk.h>
#include <glade/glade.h>

#include "warlock.h"
#include "status.h"
#include "debug.h"

/* external variables */
extern GladeXML *warlock_xml;

/* change this to change the size of the status icons in the interface */
#define STATUS_ICON_WIDTH       32
#define STATUS_ICON_HEIGHT	32

#define DEAD_2          'B' /* ? */
#define WEBBED          'C'
#define INVISIBLE       'D'
#define BOUND           'F'
#define LYING           'G'
#define SITTING         'H'
#define STUNNED         'I'
#define MUTED           'J'
#define UNKNOWN2        'K' /* this seems to be ever present for some
                               characters */
#define UNCONSCIOUS	'M'
#define HIDDEN          'N'
#define BLEEDING        'O'
#define JOINED          'P'
#define CALMED          'Q'
#define UNKNOWN3	'S'
#define UNKNOWN4        'T'
#define BANDAGED        'U'
#define DEAD            'W'

enum {
        STATUS_STANCE,
        STATUS_DAMAGE,
        STATUS_WEBBED,
        STATUS_HIDDEN,
        STATUS_JOINED,
        STATUS_INVIS,
        STATUS_MUTE,
	STATUS_UNCONSCIOUS,
        NUM_STATUS
};

/* stance_t, icon_t, and xpm_list are all in the same order */
typedef enum {
        STANCE_STANDING,
        STANCE_SITTING,
        STANCE_KNEELING,
        STANCE_LYING,
        NUM_STANCES
} stance_t;

typedef enum {
        ICON_STAND,
        ICON_SIT,
        ICON_KNEEL,
        ICON_LIE,
        ICON_BLANK,
        ICON_HIDDEN,
        ICON_WEBBED,
        ICON_BLEEDSTUN,
        ICON_BLEED,
        ICON_STUN,
        ICON_JOIN,
        ICON_DEAD,
        ICON_INVIS,
        ICON_MUTE,
	ICON_UNCONSCIOUS,
        NUM_ICON
} icon_t;

static char *xpm_list[] = {
        "stand.xpm",
        "sit.xpm",
        "kneel.xpm",
        "lie.xpm",
        "blank.xpm",
        "hide.xpm",
        "webbed.xpm",
        "bleedstun.xpm",
        "bleed.xpm",
        "stun.xpm",
        "join.xpm",
        "dead.xpm",
        "invis.png",
        "mute.png",
	"unconscious.png",
};

static gboolean dead, bleeding, stunned, webbed, hidden, joined; 
static stance_t stance = STANCE_LYING;
static GdkPixbuf *pixbuf_list[NUM_ICON];
static gint status[NUM_STATUS];
static GtkWidget *image_list[NUM_STATUS];

gboolean is_hidden (void)
{
        return hidden;
}

gboolean is_standing (void)
{
	return stance == STANCE_STANDING;
}

gboolean is_sitting (void)
{
	return stance == STANCE_SITTING;
}

gboolean is_kneeling (void)
{
	return stance == STANCE_KNEELING;
}

gboolean is_lying (void)
{
	return stance == STANCE_LYING;
}

gboolean is_dead (void)
{
	return dead;
}

gboolean is_bleeding (void)
{
	return bleeding;
}

gboolean is_stunned (void)
{
	return stunned;
}

gboolean is_webbed (void)
{
	return webbed;
}

gboolean is_joined (void)
{
	return joined;
}

static void status_init_pixbufs (void)
{
        int i;

        for (i = 0; i < NUM_ICON; i++) {
                GdkPixbuf *pixbuf;
                char *xpm;
                GError *err;

                xpm = g_build_filename (PACKAGE_DATA_DIR, PACKAGE, xpm_list[i],
				NULL);

                err = NULL;
                pixbuf = gdk_pixbuf_new_from_file (xpm, &err);
                if (err != NULL) {
                        print_error (err);
                        g_assert_not_reached ();
                }

                pixbuf_list[i] = gdk_pixbuf_scale_simple (pixbuf,
                                STATUS_ICON_WIDTH, STATUS_ICON_HEIGHT,
                                GDK_INTERP_BILINEAR);
                g_assert (pixbuf_list[i] != NULL);
                free (xpm);
        }
}

void status_init (GtkWidget *status_table)
{
	int i;

	status_init_pixbufs ();

        for (i = 0; i < NUM_STATUS; i++) {
                status[i] = ICON_BLANK;
                image_list[i] = gtk_image_new_from_pixbuf
                        (pixbuf_list[ICON_BLANK]);
                gtk_table_attach_defaults (GTK_TABLE (status_table),
                                image_list[i], i, i + 1, 0, 1);
        }
        gtk_widget_show_all (status_table);
}

static void
unrecognized_status (int i)
{
        static GSList *list = NULL;
        GtkWidget *dialog;
        GtkWidget *parent;

        // Don't display a message if we already have
        if (g_slist_find (list, GINT_TO_POINTER (i)) != NULL) {
                return;
        }

        /* add the status to the list so we don't show it again */
        list = g_slist_append (list, GINT_TO_POINTER (i));

        /* display a warning */
        parent = glade_xml_get_widget (warlock_xml, "main_window");
        dialog = gtk_message_dialog_new (GTK_WINDOW (parent),
                        GTK_DIALOG_DESTROY_WITH_PARENT,
                        GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, 
                        "Got and unrecognized status character: %c\n"
                        "Please submit a bug report at http://sourceforge.net/tracker/?group_id=12587&atid=112587"
                        " or send an email to sproctor@gmail.com\n"
                        "Please let us know what you were doing when you got "
                        "this message and any other information that might "
                        "help us figure out what this status character means.",
                        i);

        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
}

void status_set (const char *str)
{
        /*
         * FIXME we're starting off with blank rather than standing, that's bad
         * We're staying blank too.
         */
	int i;
        int new_status[NUM_STATUS];
        char *status_string;

        status_string = g_strstrip (g_strdup (str));
	dead = bleeding = stunned = webbed = hidden = joined = FALSE;

        stance = STANCE_STANDING;

        for (i = 0; i < NUM_STATUS; i++)
                new_status[i] = ICON_BLANK;

	for (i = 0; status_string[i] != '\0'; i++) {
		switch (status_string[i]) {
			case WEBBED:
				webbed = TRUE;
                                new_status[STATUS_WEBBED] = ICON_WEBBED;
                                break;

                        case LYING:
                                stance = STANCE_LYING;
                                new_status[STATUS_STANCE] = ICON_LIE;
                                break;

			case SITTING:
                                if (stance == STANCE_LYING) {
                                        stance = STANCE_KNEELING;
				} else {
                                        stance = STANCE_SITTING;
				}
				break;

			case BLEEDING:
                                bleeding = TRUE;

                                if (dead)
                                        break;

                                if (stunned) {
                                        new_status[STATUS_DAMAGE] =
                                                ICON_BLEEDSTUN;
				} else {
                                        new_status[STATUS_DAMAGE] = ICON_BLEED;
				}
				break;

			case HIDDEN:
				hidden = TRUE;
                                new_status[STATUS_HIDDEN] = ICON_HIDDEN;
				break;

			case STUNNED:
                                stunned = TRUE;

                                if (dead)
                                        break;

                                if (bleeding) {
                                        new_status[STATUS_DAMAGE] =
                                                ICON_BLEEDSTUN;
				} else {
                                        new_status[STATUS_DAMAGE] = ICON_STUN;
				}
				break;

                        case JOINED:
				joined = TRUE;
                                new_status[STATUS_JOINED] = ICON_JOIN;
                                break;

                        case INVISIBLE:
                                new_status[STATUS_INVIS] = ICON_INVIS;
                                break;

                        case MUTED:
                                new_status[STATUS_MUTE] = ICON_MUTE;
                                break;

			case UNCONSCIOUS:
				new_status[STATUS_UNCONSCIOUS] =
					ICON_UNCONSCIOUS;
				break;

			case DEAD:
			case DEAD_2:
				dead = TRUE;
                                new_status[STATUS_DAMAGE] = ICON_DEAD;
				break;

                        case ' ':
                        case 'K':
                                break;

                        default:
                                unrecognized_status (status_string[i]);
                                break;
                }
        }

        // stance is already in the proper order here
        new_status[STATUS_STANCE] = stance;
        
        for (i = 0; i < NUM_STATUS; i++) {
                if (new_status[i] != status[i]) {
                        gtk_image_set_from_pixbuf (GTK_IMAGE (image_list[i]),
                                 pixbuf_list[new_status[i]]);
                        status[i] = new_status[i];
                }
        }
}
