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

/*
 * highlights are stored in /apps/warlock/highlights, then a number
 * ex. /apps/warlock/highlights/4
 * /apps/warlock/highlights/4/case_sensitive - bool, case sensitive
 * /apps/warlock/highlights/4/string - string, the regex to match
 * /apps/warlock/highlights/4/text_info - list, a list of text attributes for each match in the regex
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <stdlib.h>
#include <sys/stat.h>
#include <sys/types.h>

#include <gtk/gtk.h>
#include <gconf/gconf-client.h>

#include "highlight.h"
#include "debug.h"
#include "warlock.h"
#include "preferences.h"
#include "helpers.h"

static gboolean notifiers_equal (gconstpointer a, gconstpointer b);
static void notifier_free (gpointer data);

static GConfClient *warlock_gconf_client = NULL;
static Profile *profile = NULL;
static GHashTable *notifier_data = NULL;

void preferences_init (void)
{
        warlock_gconf_client = gconf_client_get_default ();

        gconf_client_add_dir (warlock_gconf_client, PREFS_PREFIX,
                        GCONF_CLIENT_PRELOAD_NONE, NULL);

        profile = g_new (Profile, 1);
        profile->name = "Default";
        profile->path = gconf_concat_dir_and_key (PREFS_PREFIX, profile->name);

        notifier_data = g_hash_table_new_full (g_direct_hash, notifiers_equal,
                        NULL, notifier_free);
}

char *preferences_get_global_key (Preference id)
{
        const char *key;

        switch (id) {
                case PREF_GLOBAL_NAMES:
                        key = "global-names";
                        break;

                case PREF_PROFILES:
                        key = "profiles";
                        break; 
                case PREF_PROFILES_INDEX:
                        key = "profiles/index";
                        break;

                default:
                        g_assert_not_reached ();
                        return NULL;
        }

        return gconf_concat_dir_and_key (PREFS_PREFIX, key);
}

char *preferences_get_key (Preference id)
{
        const char *key;

        switch (id) {
		case PREF_AUTO_LOG:
			key = "auto-log";
			break;

                case PREF_AUTO_SNEAK:
                        key = "auto-sneak";
                        break;

                case PREF_ECHO:
                        key = "echo";
                        break;

                case PREF_WINDOW_WIDTH:
                        key = "window-width";
                        break;

                case PREF_WINDOW_HEIGHT:
                        key = "window-height";
                        break;

                case PREF_SCRIPT_PREFIX:
                        key = "script-prefix";
                        break;

                case PREF_DEFAULT_TEXT_COLOR:
                        key = "default-text-color";
                        break;

                case PREF_DEFAULT_BASE_COLOR:
                        key = "default-base-color";
                        break;

                case PREF_DEFAULT_FONT:
                        key = "default-font";
                        break;

                case PREF_TITLE_TEXT_COLOR:
                        key = "title-text-color";
                        break;

                case PREF_TITLE_BASE_COLOR:
                        key = "title-base-color";
                        break;

                case PREF_TITLE_FONT:
                        key = "title-font";
                        break;

                case PREF_MONSTER_TEXT_COLOR:
                        key = "monster-text-color";
                        break;

                case PREF_MONSTER_BASE_COLOR:
                        key = "monster-base-color";
                        break;

                case PREF_MONSTER_FONT:
                        key = "monster-font";
                        break;

                case PREF_ECHO_TEXT_COLOR:
                        key = "echo-text-color";
                        break;

                case PREF_ECHO_BASE_COLOR:
                        key = "echo-base-color";
                        break;

                case PREF_ECHO_FONT:
                        key = "echo-font";
                        break;

                case PREF_TEXT_BUFFER_SIZE:
                        key = "text-buffer-size";
                        break;

                case PREF_COMMAND_SIZE:
                        key = "command-size";
                        break;

                case PREF_COMMAND_HISTORY_SIZE:
                        key = "command-history-size";
                        break;

                case PREF_SCRIPT_PATH:
                        key = "script-path";
                        break;

                case PREF_LOG_PATH:
                        key = "log-path";
                        break;

                case PREF_ARRIVAL_VIEW:
                        key = "arrival-view";
                        break;

                case PREF_THOUGHT_VIEW:
                        key = "thought-view";
                        break;

                case PREF_DEATH_VIEW:
                        key = "death-view";
                        break;

                case PREF_FAMILIAR_VIEW:
                        key = "familiar-view";
                        break;

                case PREF_HIGHLIGHTS:
                        key = "highlights";
                        break;

                case PREF_HIGHLIGHTS_INDEX:
                        key = "highlights/index";
                        break;

                case PREF_MACROS:
                        key = "macros";
                        break;

                case PREF_COMMAND_HISTORY:
                        key = "command-history";
                        break;

                default:
                        g_assert_not_reached ();
                        return NULL;
        }

        return gconf_concat_dir_and_key (profile->path, key);
}

char *preferences_get_highlight_key (guint highlight_id, Preference pref_id)
{
        char *dir;
        const char *key;

        switch (pref_id) {
                case PREF_HIGHLIGHT_CASE_SENSITIVE:
                        key = "case-sensitive";
                        break;

                case PREF_HIGHLIGHT_STRING:
                        key = "string";
                        break;

                default:
                        g_assert_not_reached ();
                        return NULL;
        }

        dir = g_strdup_printf ("%06X", highlight_id);
        return gconf_concat_dir_and_key (gconf_concat_dir_and_key
                        (preferences_get_key (PREF_HIGHLIGHTS), dir), key);
}

char *preferences_get_highlight_match_key (guint highlight_id, guint match_id,
                Preference pref_id)
{
        const char *key;
        char *dir;

        switch (pref_id) {
                case PREF_HIGHLIGHT_MATCH_TEXT_COLOR:
                        key = "text-color";
                        break;

                case PREF_HIGHLIGHT_MATCH_BASE_COLOR:
                        key = "base-color";
                        break;

                case PREF_HIGHLIGHT_MATCH_FONT:
                        key = "font";
                        break;

                default:
                        g_assert_not_reached ();
                        return NULL;
        }

        dir = g_strdup_printf ("%06X/%d", highlight_id, match_id);
        return gconf_concat_dir_and_key (gconf_concat_dir_and_key
                        (preferences_get_key (PREF_HIGHLIGHTS), dir), key);
}

char *preferences_get_profile_key (guint profile_id, Preference pref_id)
{
        const char *key;
        char *dir;

        switch (pref_id) {
                case PREF_PROFILE_NAME:
                        key = "name";
                        break;

                case PREF_PROFILE_USERNAME:
                        key = "username";
                        break;

                case PREF_PROFILE_PASSWORD:
                        key = "password";
                        break;

                case PREF_PROFILE_GAME:
                        key = "game";
                        break;

                case PREF_PROFILE_CHARACTER:
                        key = "character";
                        break;

                default:
                        g_assert_not_reached ();
                        return NULL;
        }

        dir = g_strdup_printf ("%06X", profile_id);
        return gconf_concat_dir_and_key (gconf_concat_dir_and_key
                        (preferences_get_global_key (PREF_PROFILES), dir),
                        key);
}

void preferences_set_bool (const char *key, gboolean b)
{
        GError *err;

        err = NULL;
        gconf_client_set_bool (warlock_gconf_client, key, b, &err);
        print_error (err);
}

gboolean preferences_get_bool (const char *key)
{
        GError *err;
        gboolean b;

        err = NULL;
        b = gconf_client_get_bool (warlock_gconf_client, key, &err);
        print_gconf_error (err, key);

        return b;
}

void preferences_set_int (const char *key, gint i)
{
        GError *err;

        err = NULL;
        gconf_client_set_int (warlock_gconf_client, key, i, &err);
        print_error (err);
}

int preferences_get_int (const char *key)
{
        GError *err;
        int i;

        err = NULL;
        i = gconf_client_get_int (warlock_gconf_client, key, &err);
        print_error (err);

        return i;
}

void preferences_set_string (const char *key, const char *string)
{
        GError *err;

        if (string == NULL) {
                string = "";
        }

        err = NULL;
        gconf_client_set_string (warlock_gconf_client, key, string, &err);
        print_error (err);
}

char *preferences_get_string (const char *key)
{
        GError *err;
        char *str;

        err = NULL;
        str = gconf_client_get_string (warlock_gconf_client, key, &err);
        print_gconf_error (err, key);

        if (str != NULL && *str == '\0') {
                g_free (str);
                return NULL;
        } else {
                return str;
        }
}

void preferences_set_list (const char *key, PreferencesValue val, GSList *list)
{
        GError *err;
        int gval;

        switch (val) {
                case PREFERENCES_VALUE_INT:
                        gval = GCONF_VALUE_INT;
                        break;

                case PREFERENCES_VALUE_STRING:
                        gval = GCONF_VALUE_STRING;
                        break;
        }

        err = NULL;
        gconf_client_set_list (warlock_gconf_client, key, gval, list, &err);
        print_error (err);
}

GSList *preferences_get_list (const char *key, PreferencesValue val)
{
        GError *err;
        GSList *list;
        int gval;

        switch (val) {
                case PREFERENCES_VALUE_INT:
                        gval = GCONF_VALUE_INT;
                        break;

                case PREFERENCES_VALUE_STRING:
                        gval = GCONF_VALUE_STRING;
                        break;
        }

        err = NULL;
        list = gconf_client_get_list (warlock_gconf_client, key, gval, &err);
        print_gconf_error (err, key);

        return list;
}

void preferences_set_color (const char *key, const GdkColor *color)
{
        char *str;

        str = gdk_color_to_string (color);
        preferences_set_string (key, str);
        if (str != NULL) {
                g_free (str);
        }
}

GdkColor *preferences_get_color (const char *key)
{
        char *str;

        str = preferences_get_string (key);

        return gdk_color_from_string (str);
}

void preferences_set_font (const char *key, const PangoFontDescription *font)
{
        if (font != NULL) {
                char *str;

                str = pango_font_description_to_string (font);
                preferences_set_string (key, str);
        } else {
                preferences_set_string (key, NULL);
        }
}

PangoFontDescription *preferences_get_font (const char *key)
{
        char *str;
        PangoFontDescription *font;

        str = preferences_get_string (key);

        if (str == NULL) {
                return NULL;
        }

        font = pango_font_description_from_string (str);
        return font;
}

struct NotifyData {
        gpointer user_data;
        PreferencesNotifyFunc func;
        char *key;
};

static void preferences_notify_function (GConfClient *client, guint cnxn_id,
                GConfEntry *entry, gpointer user_data)
{
        struct NotifyData *ndata;

        ndata = user_data;
        ndata->func(ndata->key, ndata->user_data);
}

guint preferences_notify_add (const char *key, PreferencesNotifyFunc func,
                gpointer user_data)
{
        struct NotifyData *notify;
        GError *err;
        guint cnxn;

        notify = g_new (struct NotifyData, 1);
        notify->user_data = user_data;
        notify->func = func;
        notify->key = g_strdup (key);

        err = NULL;
        cnxn = gconf_client_notify_add (warlock_gconf_client, key,
                        preferences_notify_function, notify, NULL, &err);
        print_error (err);

        g_hash_table_insert (notifier_data, GINT_TO_POINTER(cnxn), notify);

        return cnxn;
}

void preferences_notify_remove (guint cnxn)
{
        gconf_client_notify_remove (warlock_gconf_client, cnxn);
}

static gboolean
notifiers_equal(gconstpointer a, gconstpointer b)
{
        return a == b;
}

static void
notifier_free (gpointer data)
{
        struct NotifyData *notifier;

        notifier = data;
        g_free(notifier->key);
        g_free(notifier);
}

void
preferences_unset (const char *key)
{
        GError *err;

        err = NULL;
        gconf_client_unset (warlock_gconf_client, key, &err);
        print_error (err);
}
