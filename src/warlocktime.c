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

#include <gtk/gtk.h>

#include "warlock.h"
#include "script.h"
#include "debug.h"
#include "warlocktime.h"

static GtkWidget *rt_bar = NULL;;
static GtkWidget *time_label = NULL;

static GCond *timer_cond = NULL;
static GMutex *timer_mutex = NULL;

static GTimer *timer = NULL;

static double initial_game_time = 0.0;
static int roundtime_length = 0;
static int roundtime_end = 0;

int warlock_get_roundtime_left (void)
{
	int left;
	
	left = roundtime_end - (int)(warlock_get_time ());

	if (left < 0)
		return 0;

	return left;
}

static void roundtime_update (void)
{
        gchar *text;
        int now_left;
        static int roundtime_left = -1;

        g_assert (roundtime_length >= 0);

        if (roundtime_length == 0)
                return;

        now_left = roundtime_end - (int)(warlock_get_time ());

        if (now_left == roundtime_left)
                return;

        if (now_left >= 0) {
                roundtime_left = now_left;
        } else {
                roundtime_left = 0;
        }

        debug ("length: %d, left: %d\n", roundtime_length, roundtime_left);

        if (roundtime_left > roundtime_length) {
                roundtime_length = roundtime_left;
        }

        text = g_strdup_printf (_("RoundTime: %d"), roundtime_left);
        gtk_progress_bar_set_text (GTK_PROGRESS_BAR (rt_bar), text);
        g_free (text);

        gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (rt_bar),
                        (double)roundtime_left / roundtime_length);

        if (roundtime_left == 0) {
                roundtime_length = 0;
        }

}

static void write_label (void)
{
	int hours, minutes, seconds, tmp_time;
	char *howlong;

	tmp_time = (int)g_timer_elapsed (timer, NULL);
	seconds = tmp_time % 60;
	tmp_time /= 60;
	minutes = tmp_time % 60;
	hours = tmp_time / 60;

	howlong = g_strdup_printf("[%d:%02d:%02d]", hours, minutes, seconds);
	
	gtk_label_set_text(GTK_LABEL(time_label), howlong);
	g_free (howlong);
}

static gboolean update_time (gpointer data)
{

        g_mutex_lock (timer_mutex);
        roundtime_update ();
        g_cond_broadcast (timer_cond);
        g_mutex_unlock (timer_mutex);
	write_label ();

        return TRUE;
}

void warlock_time_init (void)
{
        rt_bar = warlock_get_widget ("roundtime_bar");
        time_label = warlock_get_widget ("time_label");

        timer = g_timer_new ();
        timer_mutex = g_mutex_new ();
        timer_cond = g_cond_new ();
        gtk_progress_bar_set_fraction (GTK_PROGRESS_BAR (rt_bar), 0.0);
        g_timeout_add(100, update_time, NULL);
}

void new_roundtime (int end)
{
        roundtime_end = end;
        roundtime_length = end - (int)(warlock_get_time ());
        if (roundtime_length <= 0) {
                roundtime_length = 1;
        }
}

void warlock_set_time (int time)
{
        double current_time;

        current_time = warlock_get_time ();

	if (current_time < time) {
		initial_game_time += (double)time - current_time;
	} else if (current_time > time + 1) {
		initial_game_time -= current_time - (double)(time + 1);
	}
}

/* if script running becomes false, we quit on the next second */
/* do NOT call from main thread */
void warlock_roundtime_wait (gboolean *script_running)
{
	g_mutex_lock (timer_mutex);
	while (warlock_get_time () <= roundtime_end + 1
                        && (script_running == NULL || *script_running)) {
		g_cond_wait (timer_cond, timer_mutex);
	}
	g_mutex_unlock (timer_mutex);
}

/* do NOT call from main thread */
void warlock_pause_wait (int t, gboolean *script_running)
{
        double pause_end;

        pause_end = warlock_get_time () + (double)t;

        debug ("waiting until: %f\n", pause_end);
        g_mutex_lock (timer_mutex);
        while (warlock_get_time () < pause_end && (script_running == NULL
                                || *script_running)) {
                g_cond_wait (timer_cond, timer_mutex);
        }
        g_mutex_unlock (timer_mutex);

        debug ("done waiting\n");
}

double warlock_get_time (void)
{
	return initial_game_time + g_timer_elapsed (timer, NULL);
}
