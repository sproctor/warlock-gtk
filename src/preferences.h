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

#ifndef _PREFERENCES_H
#define _PREFERENCES_H

#define PREFS_PREFIX                    "/apps/warlock"

typedef enum {
	PREF_NONE,
        PREF_GLOBAL_NAMES,
        PREF_PROFILES,
        PREF_PROFILES_INDEX,

        PREF_PROFILE_NAME,
        PREF_PROFILE_USERNAME,
        PREF_PROFILE_PASSWORD,
        PREF_PROFILE_GAME,
        PREF_PROFILE_CHARACTER,

        PREF_AUTO_LOG,
        PREF_AUTO_SNEAK,
        PREF_ECHO,
        PREF_WINDOW_WIDTH,
        PREF_WINDOW_HEIGHT,
        PREF_SCRIPT_PREFIX,
        PREF_DEFAULT_TEXT_COLOR,
        PREF_DEFAULT_BASE_COLOR,
        PREF_DEFAULT_FONT,
        PREF_TITLE_TEXT_COLOR,
        PREF_TITLE_BASE_COLOR,
        PREF_TITLE_FONT,
        PREF_MONSTER_TEXT_COLOR,
        PREF_MONSTER_BASE_COLOR,
        PREF_MONSTER_FONT,
        PREF_ECHO_TEXT_COLOR,
        PREF_ECHO_BASE_COLOR,
        PREF_ECHO_FONT,
        PREF_TEXT_BUFFER_SIZE,
        PREF_COMMAND_SIZE,
        PREF_COMMAND_HISTORY_SIZE,
        PREF_ARRIVAL_VIEW,
        PREF_THOUGHT_VIEW,
        PREF_DEATH_VIEW,
        PREF_FAMILIAR_VIEW,
        PREF_HIGHLIGHTS,
        PREF_HIGHLIGHTS_INDEX,
        PREF_MACROS,
        PREF_COMMAND_HISTORY,

        PREF_SCRIPT_PATH,
        PREF_LOG_PATH,

        PREF_HIGHLIGHT_CASE_SENSITIVE,
        PREF_HIGHLIGHT_STRING,

        PREF_HIGHLIGHT_MATCH_TEXT_COLOR,
        PREF_HIGHLIGHT_MATCH_BASE_COLOR,
        PREF_HIGHLIGHT_MATCH_FONT
} Preference;

typedef enum {
        PREFERENCES_VALUE_INT,
        PREFERENCES_VALUE_STRING
} PreferencesValue;

typedef void (*PreferencesNotifyFunc)(const char *key, gpointer user_data);
typedef struct _Profile Profile;

struct _Profile {
        char *name;
        char *path;
};

void preferences_init (void);

char *preferences_get_global_key (Preference id);
char *preferences_get_key (Preference id);
char *preferences_get_highlight_key (guint highlight_id, Preference pref_id);
char *preferences_get_highlight_match_key (guint highlight_id, guint match_id,
                Preference pref_id);
char *preferences_get_profile_key (guint profile_id, Preference pref_id);

void preferences_set_bool (const char *key, gboolean b);
gboolean preferences_get_bool (const char *key);
void preferences_set_int (const char *key, int i);
int preferences_get_int (const char *key);
void preferences_set_string (const char *key, const char *string);
char *preferences_get_string (const char *key);
void preferences_set_list (const char *key, PreferencesValue val, GSList *list);
GSList *preferences_get_list (const char *key, PreferencesValue val);
void preferences_set_color (const char *key, const GdkRGBA *color);
GdkRGBA *preferences_get_color (const char *key);
void preferences_set_font (const char *key, const PangoFontDescription *font);
PangoFontDescription *preferences_get_font (const char *key);

guint preferences_notify_add (const char *key, PreferencesNotifyFunc func,
                gpointer user_data);
void preferences_notify_remove (guint cnxn);
void preferences_unset (const char *key);

#endif
