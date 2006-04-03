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

#ifndef _SGE_CONNECTION_H
#define _SGE_CONNECTION_H

/* types */
typedef enum {
        SGE_ACCOUNT,
        SGE_BAD_PASSWORD,
        SGE_INVALID_ACCOUNT,
        SGE_BAD_ACCOUNT,
        SGE_MENU,
        SGE_GAME,
        SGE_CHARACTERS,
        SGE_LOAD,
        SGE_NONE
} SgeState;

typedef void (*SgeFunc)(SgeState state, gpointer data);

typedef struct {
        gboolean logged_in;
        char *username;
        char *password;
        GSList *games;
        GSList *chars;
        SgeFunc func;
        SimuConnection *connection;
} SgeData;

SgeData *sge_init (const char *username, const char *password, SgeFunc func);
void sge_pick_game (SgeData *data, int n);
void sge_pick_character (SgeData *data, int n);

#endif
