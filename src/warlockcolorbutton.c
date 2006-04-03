/* Warlock Front End
 * Copyright Sean Proctor, Marshall Culpepper 2003 - 2004
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

#include "warlockcolorbutton.h"

/* local datatypes */
enum {
        COLOR_SET_SIGNAL,
        LAST_SIGNAL
};

/* local function prototypes */
static void warlock_color_button_class_init (WarlockColorButtonClass *klass);
static void warlock_color_button_init (WarlockColorButton *button);
static void warlock_color_button_set_color_real (WarlockColorButton *button,
                const GdkColor *color);

/* local variables */
static guint warlock_color_button_signals[LAST_SIGNAL] = { 0 };

/****************************************************************
 * Global Functions                                             *
 ****************************************************************/

GType
warlock_color_button_get_type (void)
{
        static GType wcb_type = 0;

        if (!wcb_type) {
                static const GTypeInfo wcb_info = {
                        sizeof (WarlockColorButtonClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) warlock_color_button_class_init,
                        NULL, /* class_finalize */
                        NULL, /* class_data */
                        sizeof (WarlockColorButton),
                        0, /* n_preallocs */
                        (GInstanceInitFunc) warlock_color_button_init,
                };

                wcb_type = g_type_register_static (GTK_TYPE_HBOX,
                                "warlock_color_button",
                                &wcb_info,
                                0);
        }

        return wcb_type;
}

GtkWidget*
warlock_color_button_new (void)
{
        return GTK_WIDGET (g_object_new (warlock_color_button_get_type (),
                                NULL));
}

void
warlock_color_button_set_color (WarlockColorButton *button,
                const GdkColor *color)
{
        if (color != NULL) {
                warlock_color_button_set_active (button, TRUE);
                warlock_color_button_set_color_real (button, color);
        } else {
                warlock_color_button_set_active (button, FALSE);
        }
}

GdkColor*
warlock_color_button_get_color (WarlockColorButton *button)
{
        if (button->active) {
                return gdk_color_copy (button->color);
        } else {
                return NULL;
        }
}

void
warlock_color_button_set_active (WarlockColorButton *button, gboolean active)
{
        if (active) {
                button->active = TRUE;
                gtk_widget_set_sensitive (button->color_button, TRUE);

                g_signal_handlers_block_matched (G_OBJECT
                                (button->check_button), G_SIGNAL_MATCH_DATA,
                                0, 0, NULL, NULL, button);
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                (button->check_button), TRUE);
                g_signal_handlers_unblock_matched (G_OBJECT
                                (button->check_button), G_SIGNAL_MATCH_DATA,
                                0, 0, NULL, NULL, button);
        } else {
                GdkColor color;

                button->active = FALSE;
                gtk_widget_set_sensitive (button->color_button, FALSE);

                gdk_color_parse ("black", &color);
                warlock_color_button_set_color_real (button, &color);

                g_signal_handlers_block_matched (G_OBJECT
                                (button->check_button), G_SIGNAL_MATCH_DATA,
                                0, 0, NULL, NULL, button);
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                (button->check_button), FALSE);
                g_signal_handlers_unblock_matched (G_OBJECT
                                (button->check_button), G_SIGNAL_MATCH_DATA,
                                0, 0, NULL, NULL, button);
        }
}

gboolean warlock_color_button_get_active (WarlockColorButton *button)
{
        return button->active;
}

/*********************************************************************
 * Local signal handler functions                                    *
 *********************************************************************/

static void
warlock_color_button_color_changed (GtkColorButton *colorbutton,
                WarlockColorButton *button)
{
        GdkColor color;

        gtk_color_button_get_color (colorbutton, &color);
        warlock_color_button_set_color (button, &color);

        g_signal_emit (G_OBJECT (button),
                        warlock_color_button_signals[COLOR_SET_SIGNAL], 0);
}

static void
warlock_color_button_toggled (GtkToggleButton *togglebutton,
                WarlockColorButton *button)
{
        gboolean state;

        state = gtk_toggle_button_get_active (togglebutton);
        warlock_color_button_set_active (button, state);

        g_signal_emit (G_OBJECT (button),
                        warlock_color_button_signals[COLOR_SET_SIGNAL], 0);
}

/**********************************************************************
 * Local functions                                                    *
 **********************************************************************/

static void
warlock_color_button_class_init (WarlockColorButtonClass *klass)
{
        warlock_color_button_signals[COLOR_SET_SIGNAL] =
                g_signal_new ("color-set",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_STRUCT_OFFSET (WarlockColorButtonClass,
                                        warlock_color_button),
                                NULL, NULL,
                                g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

static void
warlock_color_button_init (WarlockColorButton *button)
{
        button->check_button = gtk_check_button_new ();
        button->color_button = gtk_color_button_new ();
        button->active = FALSE;
        button->color = g_new (GdkColor, 1);

        gtk_box_pack_start (GTK_BOX (button), button->check_button, FALSE,
                        TRUE, 0);
        gtk_box_pack_start (GTK_BOX (button), button->color_button, FALSE,
                        TRUE, 0);

        g_signal_connect (G_OBJECT (button->check_button), "toggled",
                        G_CALLBACK (warlock_color_button_toggled), button);
        g_signal_connect (G_OBJECT (button->color_button), "color-set",
                        G_CALLBACK (warlock_color_button_color_changed),
                        button);

        warlock_color_button_set_active (button, FALSE);

        gtk_widget_show (button->color_button);
        gtk_widget_show (button->check_button);
}

static void
warlock_color_button_set_color_real (WarlockColorButton *button,
                const GdkColor *color)
{
        g_assert (color != NULL);

        memcpy (button->color, color, sizeof (GdkColor));
        g_signal_handlers_block_matched (G_OBJECT (button->color_button),
                        G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, button);
        gtk_color_button_set_color (GTK_COLOR_BUTTON (button->color_button),
                        color);
        g_signal_handlers_unblock_matched (G_OBJECT (button->color_button),
                        G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, button);
}
