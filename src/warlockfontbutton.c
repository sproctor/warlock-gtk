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

#include "warlockfontbutton.h"

/* local datatypes */
enum {
        FONT_SET_SIGNAL,
        LAST_SIGNAL
};

/* local function prototypes */
static void warlock_font_button_class_init (WarlockFontButtonClass *klass);
static void warlock_font_button_init (WarlockFontButton *button);
static void warlock_font_button_set_font_name_real (WarlockFontButton *button,
                const char *font);

/* local variables */
static guint warlock_font_button_signals[LAST_SIGNAL] = { 0 };

/****************************************************************
 * Global Functions                                             *
 ****************************************************************/

GType
warlock_font_button_get_type (void)
{
        static GType wcb_type = 0;

        if (!wcb_type) {
                static const GTypeInfo wcb_info = {
                        sizeof (WarlockFontButtonClass),
                        NULL, /* base_init */
                        NULL, /* base_finalize */
                        (GClassInitFunc) warlock_font_button_class_init,
                        NULL, /* class_finalize */
                        NULL, /* class_data */
                        sizeof (WarlockFontButton),
                        0, /* n_preallocs */
                        (GInstanceInitFunc) warlock_font_button_init,
			NULL
                };

                wcb_type = g_type_register_static (GTK_TYPE_HBOX,
                                "warlock_font_button",
                                &wcb_info,
                                0);
        }

        return wcb_type;
}

GtkWidget*
warlock_font_button_new (void)
{
        return GTK_WIDGET (g_object_new (warlock_font_button_get_type (),
                                NULL));
}

void
warlock_font_button_set_font_name (WarlockFontButton *button, const char *font)
{
        if (font != NULL) {
                warlock_font_button_set_active (button, TRUE);
                warlock_font_button_set_font_name_real (button, font);
        } else {
                warlock_font_button_set_active (button, FALSE);
        }
}

char*
warlock_font_button_get_font_name (WarlockFontButton *button)
{
        if (button->active) {
                return g_strdup (button->font);
        } else {
                return NULL;
        }
}

void
warlock_font_button_set_active (WarlockFontButton *button, gboolean active)
{
        if (active) {
                button->active = TRUE;
                gtk_widget_set_sensitive (button->font_button, TRUE);

                g_signal_handlers_block_matched (G_OBJECT
                                (button->check_button),
                                G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL,
                                button);
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                (button->check_button), TRUE);
                g_signal_handlers_unblock_matched (G_OBJECT
                                (button->check_button),
                                G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL,
                                button);
        } else {
                button->active = FALSE;
                gtk_widget_set_sensitive (button->font_button, FALSE);

                warlock_font_button_set_font_name_real (button, "Sans 12");

                g_signal_handlers_block_matched (G_OBJECT
                                (button->check_button),
                                G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, button);
                gtk_toggle_button_set_active (GTK_TOGGLE_BUTTON
                                (button->check_button), FALSE);
                g_signal_handlers_unblock_matched (G_OBJECT
                                (button->check_button),
                                G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, button);
        }
}

gboolean warlock_font_button_get_active (WarlockFontButton *button)
{
        return button->active;
}

/*********************************************************************
 * Local signal handler functions                                    *
 *********************************************************************/

static void
warlock_font_button_font_changed (GtkFontButton *fontbutton,
                WarlockFontButton *button)
{
        const char *font_name;

        font_name = gtk_font_button_get_font_name (fontbutton);
        warlock_font_button_set_font_name (button, font_name);

        g_signal_emit (G_OBJECT (button),
                        warlock_font_button_signals[FONT_SET_SIGNAL], 0);
}

static void
warlock_font_button_toggled (GtkToggleButton *togglebutton,
                WarlockFontButton *button)
{
        gboolean state;

        state = gtk_toggle_button_get_active (togglebutton);
        warlock_font_button_set_active (button, state);

        g_signal_emit (G_OBJECT (button),
                        warlock_font_button_signals[FONT_SET_SIGNAL], 0);
}

/**********************************************************************
 * Local functions                                                    *
 **********************************************************************/

static void
warlock_font_button_class_init (WarlockFontButtonClass *klass)
{
        warlock_font_button_signals[FONT_SET_SIGNAL] =
                g_signal_new ("font-set",
                                G_TYPE_FROM_CLASS (klass),
                                G_SIGNAL_RUN_FIRST | G_SIGNAL_ACTION,
                                G_STRUCT_OFFSET (WarlockFontButtonClass,
                                        warlock_font_button),
                                NULL, NULL,
                                g_cclosure_marshal_VOID__VOID, G_TYPE_NONE, 0);
}

static void
warlock_font_button_init (WarlockFontButton *button)
{
        button->check_button = gtk_check_button_new ();
        button->font_button = gtk_font_button_new ();
        button->active = FALSE;
        button->font = NULL;

        gtk_box_pack_start (GTK_BOX (button), button->check_button, FALSE,
                        TRUE, 0);
        gtk_box_pack_start (GTK_BOX (button), button->font_button, FALSE,
                        TRUE, 0);

        g_signal_connect (G_OBJECT (button->check_button), "toggled",
                        G_CALLBACK (warlock_font_button_toggled), button);
        g_signal_connect (G_OBJECT (button->font_button), "font-set",
                        G_CALLBACK (warlock_font_button_font_changed),
                        button);

        warlock_font_button_set_active (button, FALSE);
        gtk_widget_show (button->font_button);
        gtk_widget_show (button->check_button);
}

static void
warlock_font_button_set_font_name_real (WarlockFontButton *button,
                const char *font)
{
        g_assert (font != NULL);

        if (button->font != NULL) {
                g_free (button->font);
        }

        button->font = g_strdup (font);
        g_signal_handlers_block_matched (G_OBJECT (button->font_button),
                        G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, button);
        gtk_font_button_set_font_name (GTK_FONT_BUTTON (button->font_button),
                        font);
        g_signal_handlers_unblock_matched (G_OBJECT (button->font_button),
                        G_SIGNAL_MATCH_DATA, 0, 0, NULL, NULL, button);
}
