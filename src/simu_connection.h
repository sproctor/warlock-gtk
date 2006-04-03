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

#ifndef _SIMU_CONNECTION_H
#define _SIMU_CONNECTION_H

typedef enum {
        SIMU_ERROR,
        SIMU_EOF,
        SIMU_QUIT
} SimuQuit;

typedef gboolean (*SimuLineHandler)(gchar *str, gpointer user_data);
typedef void (*SimuQuitHandler)(SimuQuit reason, GError *err,
                gpointer user_data);
typedef void (*SimuInitFunc)(gpointer user_data);
typedef struct _SimuConnection SimuConnection;

struct _SimuConnection {
        GThread *thread;
        GIOChannel *channel;

        const char *server;
        int port;
        SimuInitFunc init_func;
        gpointer init_data;
        SimuLineHandler line_handler;
        gpointer line_data;
        SimuQuitHandler quit_handler;
        gpointer quit_data;
        SimuQuit quit_status;
        GError *error;
};

SimuConnection *simu_connection_init (const char *server, int port,
                SimuInitFunc init_func, gpointer init_data,
                SimuLineHandler line_handler, gpointer input_data,
                SimuQuitHandler quit_handler, gpointer hup_data);
int simu_connection_send (SimuConnection *conn, char *to_send);
void simu_connection_shutdown (SimuConnection *conn);

#endif
