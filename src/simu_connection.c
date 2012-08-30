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

static gboolean
handle_server_data (SimuConnection *conn)
{
        gchar *string = NULL;
        GError *err;
        GIOStatus status;
        gboolean rv;

        g_return_val_if_fail (conn->channel != NULL, FALSE);

        err = NULL;
        status = g_io_channel_read_line (conn->channel, &string, NULL,
                        NULL, &err);

        switch (status) {
                case G_IO_STATUS_NORMAL:
                        g_assert (err == NULL);

                        debug ("got this string: %s", string);

                        if (conn->line_handler != NULL) {
                                rv = conn->line_handler (string,
                                                conn->line_data);
                        } else {
                                rv = TRUE;
                        }

                        g_free (string);

                        return rv;

                case G_IO_STATUS_AGAIN:
                        g_warning ("Got status AGAIN from server. Ignoring.");
                        print_error (err);
                        return TRUE;

                case G_IO_STATUS_EOF:
                        debug ("got an EOF; line: %s\n", string);
                        print_error (err);
                        conn->quit_status = SIMU_EOF;

                        return FALSE;

                case G_IO_STATUS_ERROR:
                        debug ("got a connection error\n");
                        conn->quit_status = SIMU_ERROR;
                        conn->error = err;

                        return FALSE;

                default:
                        g_assert_not_reached ();
                        return FALSE;
        }
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

static gpointer
input_loop (gpointer user_data)
{
        SimuConnection *conn;

        conn = user_data;
        while (handle_server_data (conn))
                ;

        debug ("leaving input loop\n");
        simu_connection_shutdown (conn);
        if (conn->quit_handler != NULL) {
                conn->quit_handler (conn->quit_status, conn->error,
                                conn->quit_data);
        }

        return NULL;
}

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
		return NULL;
	}

        host = gethostbyname (conn->server);
	if (host == NULL) {
		g_warning ("Could not lookup hostname\n");
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
                // FIXME: handle this case more gracefully
                g_warning ("Couldn't open connection to server. "
                                "Check your internet connection.");
		return NULL;
        }

#ifdef __WIN32__
        conn->channel = g_io_channel_win32_new_socket (sock);
#else
        conn->channel = g_io_channel_unix_new (sock);
#endif

        g_io_channel_set_encoding (conn->channel, NULL, NULL);

        if (conn->init_func != NULL) {
                conn->init_func (conn->init_data);
        }

        conn->thread = g_thread_create (input_loop, conn, TRUE, NULL);

	return NULL;
}



SimuConnection *
simu_connection_init (const char *server, int port,
		SimuInitFunc init_func, gpointer init_data,
		SimuLineHandler line_handler, gpointer line_data,
		SimuQuitHandler quit_handler, gpointer quit_data)
{
	SimuConnection *conn = g_new (SimuConnection, 1);

        if (!g_thread_supported ()) g_thread_init (NULL);

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

	g_thread_create (init_connect, (gpointer)conn, FALSE, NULL);

        return conn;
}
