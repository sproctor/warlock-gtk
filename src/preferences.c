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
 * Preferences are stored in an INI-style key file at
 * $XDG_CONFIG_HOME/warlock-gtk/warlock.ini (typically
 * ~/.config/warlock-gtk/warlock.ini).
 *
 * The rest of the program addresses preferences with GConf-style absolute
 * path keys, e.g. "/apps/warlock/Default/highlights/00000A/3/text-color".
 * Internally each such key is split on its final '/' into a GKeyFile group
 * (everything before) and a key name (everything after).  This keeps the
 * dynamic, hierarchical key structure (per-highlight, per-profile,
 * per-window) working without needing a fixed schema.
 */

#ifdef HAVE_CONFIG_H
#  include <config.h>
#endif

#include <string.h>
#include <stdlib.h>

#include <gtk/gtk.h>

#include "highlight.h"
#include "debug.h"
#include "warlock.h"
#include "preferences.h"
#include "helpers.h"

static GKeyFile *warlock_key_file = NULL;
static char *config_path = NULL;
static Profile *profile = NULL;
static GHashTable *notifiers = NULL;    /* connection id -> struct NotifyData */
static guint next_cnxn = 1;
static gboolean suppress_save = FALSE;
static gboolean prefs_dirty = FALSE;

struct NotifyData {
        guint id;
        char *key;
        PreferencesNotifyFunc func;
        gpointer user_data;
};

/****
 * helpers
 ****/

/* join a directory and a key with a '/' separator */
static char *concat_key (const char *dir, const char *key)
{
        return g_strconcat (dir, "/", key, NULL);
}

/* split an absolute key into a GKeyFile group (allocated, caller frees) and a
 * key name (a pointer into the original string) */
static void split_key (const char *key, char **group, const char **name)
{
        const char *slash;

        slash = strrchr (key, '/');
        if (slash == NULL) {
                *group = g_strdup ("");
                *name = key;
        } else {
                *group = g_strndup (key, slash - key);
                *name = slash + 1;
        }
}

static void notifier_free (gpointer data)
{
        struct NotifyData *notifier = data;

        g_free (notifier->key);
        g_free (notifier);
}

/* call every notifier registered for the exact key that changed */
static void preferences_notify_key (const char *key)
{
        GHashTableIter iter;
        gpointer value;
        GSList *matched = NULL, *cur;

        if (notifiers == NULL) {
                return;
        }

        /* snapshot the matching notifiers first: a callback may add or remove
         * notifications (e.g. change_index adds/removes highlights) which would
         * otherwise invalidate the hash table iterator */
        g_hash_table_iter_init (&iter, notifiers);
        while (g_hash_table_iter_next (&iter, NULL, &value)) {
                struct NotifyData *nd = value;

                if (strcmp (nd->key, key) == 0) {
                        matched = g_slist_prepend (matched, nd);
                }
        }

        for (cur = matched; cur != NULL; cur = cur->next) {
                struct NotifyData *nd = cur->data;

                nd->func (key, nd->user_data);
        }
        g_slist_free (matched);
}

static void preferences_write (void)
{
        GError *err = NULL;

        if (!g_key_file_save_to_file (warlock_key_file, config_path, &err)) {
                g_warning ("Couldn't save preferences to %s: %s", config_path,
                                err->message);
                g_error_free (err);
        }
}

/* called by every setter; coalesces writes during bulk updates */
static void preferences_changed (void)
{
        prefs_dirty = TRUE;
        if (!suppress_save) {
                preferences_write ();
                prefs_dirty = FALSE;
        }
}

static gboolean preferences_has_key (const char *key)
{
        char *group;
        const char *name;
        gboolean has;

        split_key (key, &group, &name);
        has = g_key_file_has_key (warlock_key_file, group, name, NULL);
        g_free (group);

        return has;
}

/****
 * default values (previously supplied by the GConf schema)
 ****/

static void default_int (Preference id, int value)
{
        char *key = preferences_get_key (id);

        if (!preferences_has_key (key)) {
                preferences_set_int (key, value);
        }
        g_free (key);
}

static void default_bool (Preference id, gboolean value)
{
        char *key = preferences_get_key (id);

        if (!preferences_has_key (key)) {
                preferences_set_bool (key, value);
        }
        g_free (key);
}

static void default_string (Preference id, const char *value)
{
        char *key = preferences_get_key (id);

        if (!preferences_has_key (key)) {
                preferences_set_string (key, value);
        }
        g_free (key);
}

static void preferences_set_defaults (void)
{
        suppress_save = TRUE;

        default_int (PREF_WINDOW_WIDTH, 800);
        default_int (PREF_WINDOW_HEIGHT, 600);
        default_int (PREF_TEXT_BUFFER_SIZE, 1000000);
        default_int (PREF_COMMAND_SIZE, 3);
        default_int (PREF_COMMAND_HISTORY_SIZE, 100);

        default_bool (PREF_ECHO, TRUE);
        default_bool (PREF_AUTO_SNEAK, TRUE);
        default_bool (PREF_ARRIVAL_VIEW, TRUE);

        default_string (PREF_SCRIPT_PREFIX, ".");
        default_string (PREF_SCRIPT_PATH, ".warlock/scripts");
        default_string (PREF_LOG_PATH, ".warlock/logs");

        default_string (PREF_DEFAULT_TEXT_COLOR, "#d3d3d3");
        default_string (PREF_DEFAULT_BASE_COLOR, "#000000");
        default_string (PREF_DEFAULT_FONT, "monospace 9");
        default_string (PREF_TITLE_TEXT_COLOR, "#ffffff");
        default_string (PREF_MONSTER_TEXT_COLOR, "#ffff00");

        suppress_save = FALSE;
        if (prefs_dirty) {
                preferences_write ();
                prefs_dirty = FALSE;
        }
}

void preferences_init (void)
{
        GError *err = NULL;
        char *dir;

        config_path = g_build_filename (g_get_user_config_dir (),
                        "warlock-gtk", "warlock.ini", NULL);

        dir = g_path_get_dirname (config_path);
        g_mkdir_with_parents (dir, 0700);
        g_free (dir);

        warlock_key_file = g_key_file_new ();
        if (!g_key_file_load_from_file (warlock_key_file, config_path,
                                G_KEY_FILE_KEEP_COMMENTS, &err)) {
                /* a missing file on first run is expected */
                if (!g_error_matches (err, G_FILE_ERROR, G_FILE_ERROR_NOENT)) {
                        g_warning ("Couldn't load preferences from %s: %s",
                                        config_path, err->message);
                }
                g_clear_error (&err);
        }

        profile = g_new (Profile, 1);
        profile->name = g_strdup ("Default");
        profile->path = concat_key (PREFS_PREFIX, profile->name);

        notifiers = g_hash_table_new_full (g_direct_hash, g_direct_equal, NULL,
                        notifier_free);

        preferences_set_defaults ();
}

/****
 * key builders
 ****/

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

        return concat_key (PREFS_PREFIX, key);
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

        return concat_key (profile->path, key);
}

char *
preferences_get_full_key (const char *key)
{
	return concat_key (profile->path, key);
}

char *
preferences_get_window_key (const char *name, const char *key)
{
	char *dir, *full;

	dir = concat_key (PREFS_WINDOWS_PREFIX, name);
	full = concat_key (dir, key);
	g_free (dir);

	return full;
}

char *preferences_get_highlight_key (guint highlight_id, Preference pref_id)
{
        char *dir, *base, *prefix, *full;
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
        base = preferences_get_key (PREF_HIGHLIGHTS);
        prefix = concat_key (base, dir);
        full = concat_key (prefix, key);
        g_free (base);
        g_free (dir);
        g_free (prefix);

        return full;
}

char *preferences_get_highlight_match_key (guint highlight_id, guint match_id,
                Preference pref_id)
{
        char *dir, *base, *prefix, *full;
        const char *key;

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
        base = preferences_get_key (PREF_HIGHLIGHTS);
        prefix = concat_key (base, dir);
        full = concat_key (prefix, key);
        g_free (base);
        g_free (dir);
        g_free (prefix);

        return full;
}

char *preferences_get_profile_key (guint profile_id, Preference pref_id)
{
        char *dir, *base, *prefix, *full;
        const char *key;

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
        base = preferences_get_global_key (PREF_PROFILES);
        prefix = concat_key (base, dir);
        full = concat_key (prefix, key);
        g_free (base);
        g_free (dir);
        g_free (prefix);

        return full;
}

/****
 * getters / setters
 ****/

void preferences_set_bool (const char *key, gboolean b)
{
        char *group;
        const char *name;

        split_key (key, &group, &name);
        g_key_file_set_boolean (warlock_key_file, group, name, b);
        g_free (group);

        preferences_changed ();
        preferences_notify_key (key);
}

gboolean preferences_get_bool (const char *key)
{
        char *group;
        const char *name;
        gboolean b;
        GError *err = NULL;

        split_key (key, &group, &name);
        b = g_key_file_get_boolean (warlock_key_file, group, name, &err);
        g_free (group);

        if (err != NULL) {
                g_error_free (err);
                return FALSE;
        }

        return b;
}

void preferences_set_int (const char *key, gint i)
{
        char *group;
        const char *name;

        split_key (key, &group, &name);
        g_key_file_set_integer (warlock_key_file, group, name, i);
        g_free (group);

        preferences_changed ();
        preferences_notify_key (key);
}

int preferences_get_int (const char *key)
{
        char *group;
        const char *name;
        int i;
        GError *err = NULL;

        split_key (key, &group, &name);
        i = g_key_file_get_integer (warlock_key_file, group, name, &err);
        g_free (group);

        if (err != NULL) {
                g_error_free (err);
                return 0;
        }

        return i;
}

void preferences_set_string (const char *key, const char *string)
{
        char *group;
        const char *name;

        if (string == NULL) {
                string = "";
        }

        split_key (key, &group, &name);
        g_key_file_set_string (warlock_key_file, group, name, string);
        g_free (group);

        preferences_changed ();
        preferences_notify_key (key);
}

char *preferences_get_string (const char *key)
{
        char *group;
        const char *name;
        char *str;

        split_key (key, &group, &name);
        str = g_key_file_get_string (warlock_key_file, group, name, NULL);
        g_free (group);

        if (str != NULL && *str == '\0') {
                g_free (str);
                return NULL;
        }

        return str;
}

void preferences_set_list (const char *key, PreferencesValue val, GSList *list)
{
        char *group;
        const char *name;
        guint len, i;
        GSList *cur;

        split_key (key, &group, &name);
        len = g_slist_length (list);

        if (val == PREFERENCES_VALUE_INT) {
                gint *arr = g_new (gint, len);

                for (cur = list, i = 0; cur != NULL; cur = cur->next, i++) {
                        arr[i] = GPOINTER_TO_INT (cur->data);
                }
                g_key_file_set_integer_list (warlock_key_file, group, name, arr,
                                len);
                g_free (arr);
        } else {
                const gchar **arr = g_new (const gchar *, len);

                for (cur = list, i = 0; cur != NULL; cur = cur->next, i++) {
                        arr[i] = cur->data;
                }
                g_key_file_set_string_list (warlock_key_file, group, name,
                                (const gchar * const *) arr, len);
                g_free (arr);
        }

        g_free (group);

        preferences_changed ();
        preferences_notify_key (key);
}

GSList *preferences_get_list (const char *key, PreferencesValue val)
{
        char *group;
        const char *name;
        GSList *list = NULL;
        gsize len = 0, i;

        split_key (key, &group, &name);

        if (val == PREFERENCES_VALUE_INT) {
                gint *arr = g_key_file_get_integer_list (warlock_key_file,
                                group, name, &len, NULL);

                for (i = 0; i < len; i++) {
                        list = g_slist_append (list, GINT_TO_POINTER (arr[i]));
                }
                g_free (arr);
        } else {
                gchar **arr = g_key_file_get_string_list (warlock_key_file,
                                group, name, &len, NULL);

                for (i = 0; i < len; i++) {
                        list = g_slist_append (list, g_strdup (arr[i]));
                }
                g_strfreev (arr);
        }

        g_free (group);

        return list;
}

void preferences_set_color (const char *key, const GdkRGBA *color)
{

	if (color != NULL) {
		char *str;
		str = gdk_rgba_to_string (color);
		preferences_set_string (key, str);
		g_free (str);
	} else {
		preferences_set_string (key, NULL);
	}
}

GdkRGBA *preferences_get_color (const char *key)
{
        char *str;
	GdkRGBA *rgba;

        str = preferences_get_string (key);
	if (str == NULL)
		return NULL;

	rgba = g_new (GdkRGBA, 1);
        if (gdk_rgba_parse (rgba, str)) {
                g_free (str);
		return rgba;
        }

        g_free (str);
        g_free (rgba);
	return NULL;
}

void preferences_set_font (const char *key, const PangoFontDescription *font)
{
        if (font != NULL) {
                char *str;

                str = pango_font_description_to_string (font);
                preferences_set_string (key, str);
                g_free (str);
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
        g_free (str);
        return font;
}

/****
 * change notification
 ****/

guint preferences_notify_add (const char *key, PreferencesNotifyFunc func,
                gpointer user_data)
{
        struct NotifyData *notify;

        notify = g_new (struct NotifyData, 1);
        notify->id = next_cnxn++;
        notify->key = g_strdup (key);
        notify->func = func;
        notify->user_data = user_data;

        g_hash_table_insert (notifiers, GUINT_TO_POINTER (notify->id), notify);

        return notify->id;
}

void preferences_notify_remove (guint cnxn)
{
        g_hash_table_remove (notifiers, GUINT_TO_POINTER (cnxn));
}

void
preferences_unset (const char *key)
{
        char *group;
        const char *name;

        split_key (key, &group, &name);
        g_key_file_remove_key (warlock_key_file, group, name, NULL);
        g_free (group);

        preferences_changed ();
}
