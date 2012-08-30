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

#ifndef __WARLOCKCOLORBUTTON_H__
#define __WARLOCKCOLORBUTTON_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define WARLOCK_COLOR_BUTTON_TYPE               (warlock_color_button_get_type \
                ())
#define WARLOCK_COLOR_BUTTON(obj)               (G_TYPE_CHECK_INSTANCE_CAST \
                ((obj), WARLOCK_COLOR_BUTTON_TYPE, WarlockColorButton))
#define WARLOCK_COLOR_BUTTON_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST \
                ((klass), WARLOCK_COLOR_BUTTON_TYPE, WarlockColorButtonClass))
#define IS_WARLOCK_COLOR_BUTTON(obj)            (G_TYPE_CHECK_INSTANCE_TYPE \
                ((obj), WARLOCK_COLOR_BUTTON_TYPE))
#define IS_WARLOCK_COLOR_BUTTON_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE \
                ((klass), WARLOCK_COLOR_BUTTON_TYPE))

typedef struct _WarlockColorButton      WarlockColorButton;
typedef struct _WarlockColorButtonClass WarlockColorButtonClass;

struct _WarlockColorButton {
        GtkHBox         box;

        gboolean        active;
        GdkRGBA*	color;
        GtkWidget*      color_button;
        GtkWidget*      check_button;
};

struct _WarlockColorButtonClass {
        GtkHBoxClass parent_class;

        void (* warlock_color_button) (WarlockColorButton *wcb);
};

GType           warlock_color_button_get_type   (void);
GtkWidget*      warlock_color_button_new        (void);
void            warlock_color_button_set_color  (WarlockColorButton *button,
                const GdkRGBA *color);
GdkRGBA*       warlock_color_button_get_color  (WarlockColorButton *button);
void            warlock_color_button_set_active (WarlockColorButton *button,
                gboolean active);
gboolean        warlock_color_button_get_active (WarlockColorButton *button);

G_END_DECLS

#endif /* __WARLOCKCOLORBUTTON_H__ */
