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

#ifndef __WARLOCKFONTBUTTON_H__
#define __WARLOCKFONTBUTTON_H__

#include <gtk/gtk.h>

G_BEGIN_DECLS

#define WARLOCK_FONT_BUTTON_TYPE               (warlock_font_button_get_type \
                ())
#define WARLOCK_FONT_BUTTON(obj)               (G_TYPE_CHECK_INSTANCE_CAST \
                ((obj), WARLOCK_FONT_BUTTON_TYPE, WarlockFontButton))
#define WARLOCK_FONT_BUTTON_CLASS(klass)       (G_TYPE_CHECK_CLASS_CAST \
                ((klass), WARLOCK_FONT_BUTTON_TYPE, WarlockFontButtonClass))
#define IS_WARLOCK_FONT_BUTTON(obj)            (G_TYPE_CHECK_INSTANCE_TYPE \
                ((obj), WARLOCK_FONT_BUTTON_TYPE))
#define IS_WARLOCK_FONT_BUTTON_CLASS(klass)    (G_TYPE_CHECK_CLASS_TYPE \
                ((klass), WARLOCK_FONT_BUTTON_TYPE))

typedef struct _WarlockFontButton      WarlockFontButton;
typedef struct _WarlockFontButtonClass WarlockFontButtonClass;

struct _WarlockFontButton {
        GtkHBox         box;

        gboolean        active;
        char*           font;
        GtkWidget*      font_button;
        GtkWidget*      check_button;
};

struct _WarlockFontButtonClass {
        GtkHBoxClass parent_class;

        void (* warlock_font_button) (WarlockFontButton *wcb);
};

GType           warlock_font_button_get_type    (void);
GtkWidget*      warlock_font_button_new         (void);
void            warlock_font_button_set_font_name (WarlockFontButton *button,
                const char *font);
char*           warlock_font_button_get_font_name (WarlockFontButton *button);
void            warlock_font_button_set_active (WarlockFontButton *button,
                gboolean active);
gboolean        warlock_font_button_get_active (WarlockFontButton *button);

G_END_DECLS

#endif /* __WARLOCKFONTBUTTON_H__ */
