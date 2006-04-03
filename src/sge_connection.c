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

#include <glib.h>
#include <gdk/gdk.h>

#include "simu_connection.h"
#include "sge_connection.h"
#include "profile_dialog.h"
#include "debug.h"

/* external variables */
extern char *host;
extern char *key;
extern int port;

/* function definitions */
static char *sge_encrypt_password (char *passwd, char *hash)
{
        int i;
        char *final;

        g_assert (passwd != NULL);
        g_assert (hash != NULL);

        final = g_new (char, 33);

        for (i = 0; i < 32 && passwd[i] != '\0' && hash[i] != '\0'; i++) {
                final[i] = (char)((hash[i] ^ (passwd[i] - 32)) + 32);
        }
        final[i] = '\0';

        return final;
}

static GSList *parse_M (char **input, SgeData *data)
{
        int i;
        GSList *game_names;

        game_names = NULL;
        for (i = 0; input[i] != NULL && input[i + 1] != NULL; i += 2) {
                data->games = g_slist_append (data->games, g_strdup (input[i]));
                game_names = g_slist_append (game_names,
                                g_strdup (input[i + 1]));
        }

        return game_names;
}

static GSList *parse_C (char **input, SgeData *data)
{
        int i;
        GSList *char_names;

        char_names = NULL;
        for (i = 4; input[i] != NULL && input[i + 1] != NULL; i += 2) {
                data->chars = g_slist_append (data->chars, g_strdup (input[i]));
                char_names = g_slist_append (char_names,
                                g_strdup (input[i + 1]));
        }

        return char_names;
}

static void parse_L (char **input)
{
        int i;

        for (i = 0; input[i] != NULL; i++) {
                char *value;

                value = strstr (input[i], "=") + 1;
                if (value == NULL) {
                        continue;
                }

                if (g_str_has_prefix (input[i], "GAMEHOST=")) {
                        host = g_strdup (value);
                } else if (g_str_has_prefix (input[i], "GAMEPORT=")) {
                        port = atoi (value);
                } else if (g_str_has_prefix (input[i], "KEY=")) {
                        key = g_strdup (value);
                }
        }
}

static gboolean sge_handle_line (char *str, gpointer user_data)
{
        char **input;
        char *to_send;
        int rv;
        SgeData *data;
        GSList *list;

        data = user_data;

        if (!data->logged_in) {
                char *encrypted_pass;

                encrypted_pass = sge_encrypt_password (data->password, str);
                to_send = g_strdup_printf ("A\t%s\t%s\n", data->username,
                                encrypted_pass);
                data->logged_in = TRUE;
                simu_connection_send (data->connection, to_send);
                g_free (to_send);
                return TRUE;
        }

        input = g_strsplit_set (str, "\t\n", -1);

        g_assert (input != NULL && input[0] != NULL);

        switch (input[0][0]) {
                case 'A': // we got logged in, now ask for a game menu
                        g_assert (input[1] != NULL);
                        if (input[1][0] != '\0') {
                                to_send = g_strdup_printf ("M\n");
                                simu_connection_send (data->connection,
                                                to_send);
                                g_free (to_send);
                        } else if (input[2] != NULL && strcmp (input[2],
                                                "PASSWORD") == 0) {
                                gdk_threads_enter ();
                                data->func (SGE_BAD_PASSWORD, NULL);
                                gdk_threads_leave ();
                        } else if (input[2] != NULL && strcmp (input[2],
                                                "NORECORD") == 0) {
                                gdk_threads_enter ();
                                data->func (SGE_INVALID_ACCOUNT, NULL);
                                gdk_threads_leave ();
                        } else if (input[2] != NULL && strcmp (input[2],
                                                "REJECT") == 0) {
                                gdk_threads_enter ();
                                data->func (SGE_BAD_ACCOUNT, NULL);
                                gdk_threads_leave ();
                        } else {
                                g_printerr ("Input string: %s", str);
                                g_assert_not_reached ();
                        }
                        rv = TRUE;
                        break;

                case 'M': // we got a game menu, let the client know.
                        list = parse_M (input + 1, data);
                        gdk_threads_enter ();
                        data->func (SGE_MENU, list);
                        gdk_threads_leave ();
                        g_slist_free (list);
                        rv = TRUE;
                        break;

                case 'G': /* our game as been selected, now ask for a list of
                             characters. */
                        simu_connection_send (data->connection, "C\n");
                        rv = TRUE;
                        break;
                        
                case 'C': // we got a list of characters, let the client know
                        list = parse_C (input + 1, data);
                        gdk_threads_enter ();
                        data->func (SGE_CHARACTERS, list);
                        gdk_threads_leave ();
                        g_slist_free (list);
                        rv = TRUE;
                        break;
                        
                case 'L': // we got the rest of the data to load the game
                        parse_L (input + 1);
                        gdk_threads_enter ();
                        data->func (SGE_LOAD, NULL);
                        gdk_threads_leave ();
                        rv = FALSE;
                        break;

                default:
                        g_assert_not_reached ();
                        rv = FALSE;
        }
        return rv;
}

void sge_pick_game (SgeData *data, int n)
{
        char to_send[50];
        char *game;

        debug ("picked game %d\n", n);

        game = (char*)g_slist_nth_data (data->games, n);
        debug ("game code: %s\n", game);
        g_snprintf (to_send, 50, "G\t%s\n", game);
        simu_connection_send (data->connection, to_send);
}

void sge_pick_character (SgeData *data, int n)
{
        char to_send[50];

        g_snprintf (to_send, 50, "L\t%s\tPLAY\n", (char*)g_slist_nth_data
                        (data->chars, n));
        simu_connection_send (data->connection, to_send);
}

static void sge_handle_init (gpointer user_data)
{
        SgeData *data;

        data = user_data;
        debug ("doing initial SGE connection\n");

        simu_connection_send (data->connection, "K\n");
}

/* func gets called with the state when something happens */
SgeData *sge_init (const char *username, const char *password, SgeFunc func)
{
        SgeData *data;

        data = g_new (SgeData, 1);

        data->username = g_strdup (username);
        data->password = g_strdup (password);
        data->logged_in = FALSE;
        data->games = NULL;
        data->chars = NULL;
        data->func = func;

        data->connection = simu_connection_init ("eaccess.play.net", 7900,
                        sge_handle_init, data, sge_handle_line, data,
                        NULL, NULL);

        return data;
}
