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

#include <sys/types.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#ifdef __WIN32__
#include <winsock.h>
#define socket_t SOCKET
#else
#include <sys/socket.h>
#include <netdb.h>
#define socket_t int
#endif

#include <glib.h>

#include "simu_connection.h"
#include "debug.h"

gsize
simu_connection_send (SimuConnection *conn, char *to_send)
{
        gsize written = 0; 
        GIOStatus status;
        GError *err;

        if (to_send == NULL || conn == NULL || conn->channel == NULL) {
                return 0;
        }

        debug ("Sending: %s\n", to_send);

        err = NULL;
        status = g_io_channel_write_chars (conn->channel, to_send, -1, &written,
                        &err);

        debug ("%" G_GSIZE_FORMAT " bytes written\n", written);

        switch (status) {
                case G_IO_STATUS_NORMAL:
                        g_assert (err == NULL);
                        g_io_channel_flush (conn->channel, &err);
                        print_error (err);
                        break;

                case G_IO_STATUS_EOF:
                        debug ("got an EOF while trying to write\n");
                        conn->quit_status = SIMU_EOF;
                        print_error (err);
                        break;

                case G_IO_STATUS_AGAIN:
                        debug ("got an AGAIN while trying to write\n");
                        print_error (err);
                        break;

                case G_IO_STATUS_ERROR:
                        debug ("got an error while trying to write\n");
                        conn->quit_status = SIMU_ERROR;
                        conn->error = err;
                        break;

                default:
                        g_assert_not_reached ();
        }

        return written;
}

void
simu_connection_shutdown (SimuConnection *conn)
{
        GError *err;

        debug ("shutting down the connection\n");

        g_return_if_fail (conn->channel != NULL);

        err = NULL;
        g_io_channel_shutdown (conn->channel, TRUE, &err);
        print_error (err);
        conn->channel = NULL;
}

/* Reads and dispatches every complete line currently buffered, then waits for
 * more.  Runs on the main loop, so the line handler (which touches the UI) runs
 * on the main thread - no GDK locking required.  Returns G_SOURCE_REMOVE and
 * tears the connection down once it ends. */
static gboolean
handle_server_data (GIOChannel *source, GIOCondition condition,
                gpointer user_data)
{
        SimuConnection *conn = user_data;
        gboolean ended = FALSE;

        do {
                gchar *string = NULL;
                GError *err = NULL;
                GIOStatus status;

                status = g_io_channel_read_line (conn->channel, &string, NULL,
                                NULL, &err);

                switch (status) {
                        case G_IO_STATUS_NORMAL:
                                debug ("got this string: %s", string);
                                if (conn->line_handler != NULL
                                                && !conn->line_handler (string,
                                                        conn->line_data)) {
                                        ended = TRUE;
                                }
                                g_free (string);
                                break;

                        case G_IO_STATUS_AGAIN:
                                /* no complete line buffered yet; wait for the
                                 * watch to fire again */
                                g_free (string);
                                return G_SOURCE_CONTINUE;

                        case G_IO_STATUS_EOF:
                                debug ("got an EOF\n");
                                conn->quit_status = SIMU_EOF;
                                ended = TRUE;
                                break;

                        case G_IO_STATUS_ERROR:
                                debug ("got a connection error\n");
                                conn->quit_status = SIMU_ERROR;
                                conn->error = err;
                                ended = TRUE;
                                break;

                        default:
                                g_assert_not_reached ();
                }
        } while (!ended);

        debug ("leaving input loop\n");
        conn->watch_id = 0;
        simu_connection_shutdown (conn);
        if (conn->quit_handler != NULL) {
                conn->quit_handler (conn->quit_status, conn->error,
                                conn->quit_data);
        }

        return G_SOURCE_REMOVE;
}

/* Runs on the main loop once the connect thread finishes.  Either the channel
 * is up (start watching it) or the connection failed (notify via quit). */
static gboolean
connection_established (gpointer user_data)
{
        SimuConnection *conn = user_data;

        if (conn->channel == NULL) {
                if (conn->quit_handler != NULL) {
                        conn->quit_handler (conn->quit_status, conn->error,
                                        conn->quit_data);
                }
                return G_SOURCE_REMOVE;
        }

        if (conn->init_func != NULL) {
                conn->init_func (conn->init_data);
        }

        conn->watch_id = g_io_add_watch (conn->channel,
                        G_IO_IN | G_IO_HUP | G_IO_ERR, handle_server_data, conn);

        return G_SOURCE_REMOVE;
}

/* Does the blocking DNS lookup and connect off the main thread, then hands the
 * channel back to the main loop. */
static gpointer
init_connect (gpointer user_data)
{
	SimuConnection *conn;
        struct hostent *host;
	struct sockaddr_in sin;
        socket_t sock;

        conn = user_data;

#ifdef __WIN32__
        WORD version = MAKEWORD (1,1);
        WSADATA wsaData;

        WSAStartup (version, &wsaData);
#endif

        sock = socket (AF_INET, SOCK_STREAM, 0);
	if (sock == -1) {
		g_warning ("Could not open socket.\n");
                conn->quit_status = SIMU_ERROR;
                g_idle_add (connection_established, conn);
		return NULL;
	}

        host = gethostbyname (conn->server);
	if (host == NULL) {
		g_warning ("Could not lookup hostname\n");
                conn->quit_status = SIMU_ERROR;
                g_idle_add (connection_established, conn);
		return NULL;
	}

        memset(&sin, 0, sizeof (struct sockaddr_in));
        sin.sin_family = AF_INET;
        sin.sin_port = htons (conn->port);
        sin.sin_addr.s_addr = ((struct in_addr *)(host->h_addr))->s_addr;

        if (connect(sock, (struct sockaddr*)&sin,
				sizeof (struct sockaddr)) < 0) {
#ifdef __WIN32__
                closesocket (sock);
#else
                close (sock);
#endif
                g_warning ("Couldn't open connection to server. "
                                "Check your internet connection.");
                conn->quit_status = SIMU_ERROR;
                g_idle_add (connection_established, conn);
		return NULL;
        }

#ifdef __WIN32__
        conn->channel = g_io_channel_win32_new_socket (sock);
#else
        conn->channel = g_io_channel_unix_new (sock);
#endif

        g_io_channel_set_encoding (conn->channel, NULL, NULL);
        /* non-blocking so the main-loop read never stalls the UI */
        g_io_channel_set_flags (conn->channel, G_IO_FLAG_NONBLOCK, NULL);

        g_idle_add (connection_established, conn);

	return NULL;
}



SimuConnection *
simu_connection_init (const char *server, int port,
		SimuInitFunc init_func, gpointer init_data,
		SimuLineHandler line_handler, gpointer line_data,
		SimuQuitHandler quit_handler, gpointer quit_data)
{
	SimuConnection *conn = g_new (SimuConnection, 1);

	conn->server = server;
	conn->port = port;
	conn->init_func = init_func;
	conn->init_data = init_data;
        conn->line_handler = line_handler;
        conn->line_data = line_data;
        conn->quit_handler = quit_handler;
        conn->quit_data = quit_data;
        conn->quit_status = SIMU_QUIT;

        conn->channel = NULL;
        conn->thread = NULL;
        conn->watch_id = 0;
        conn->error = NULL;

	g_thread_unref (g_thread_new ("warlock-connect", init_connect, conn));

        return conn;
}
