%{
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

#include <string.h>
#include <ctype.h>

#include <gtk/gtk.h>
#include <glade/glade.h>

#include "highlight.h"
#include "debug.h"
#include "warlock.h"
#include "warlockstring.h"
#include "compass.h"
#include "hand.h"
#include "warlocktime.h"
#include "status.h"
#include "warlockview.h"
#include "script.h"
#include "simu_connection.h"

#ifdef CONFIG_SPIDERMONKEY
#include "jsscript.h"
#endif

/* external global variables */
extern SimuConnection *connection;
extern GladeXML *warlock_xml;
extern gboolean prompting;

/* local variables */
static gboolean highlighting = FALSE;
static gboolean familiar = FALSE;
static gboolean room_title = FALSE;
static gboolean death = FALSE;
static char command = 0;
static gboolean quit = FALSE;

/* local function definitions */
static char *lexer_string (void);
static void handle_command (char command, char *params);
static void arrival_string (void);
static void output_string (void);
static void eol_code (void);
static char *parse_hand (char *str);
static void unhandled_character (void);
%}

%option prefix="wizard" nounput noyywrap
%s COMMAND PARAMS

ANY             [[:alnum:][:blank:][:punct:]]
EOL		(\n|\r\n)
REST		[[:alnum:][:blank:][:punct:]]*(\n|\r\n)
%%
\034GS			BEGIN (COMMAND);
<COMMAND>[[:alpha:]]	{
	char *tmp;
	tmp = lexer_string ();
	g_assert (tmp != NULL && strlen (tmp) == 1);
	command = tmp[0];
	g_free (tmp);
	BEGIN (PARAMS);
}
<PARAMS>{REST}		{
	char *params;
	params = lexer_string ();
	handle_command (command, params);
	g_free (params);
	BEGIN (INITIAL);
}
<INITIAL>\216		{
	highlighting = TRUE;
}
<INITIAL>\240		{
	highlighting = FALSE;
}
<INITIAL>\007		warlock_bell ();
<INITIAL>(\ \*\ ){ANY}+((\ joins\ the\ adventure\.)|(\ returns\ home\ from\ a\ hard\ day\ of\ adventuring\.)|(\ has\ disconnected\.)){REST} arrival_string();
<INITIAL>{ANY}+		output_string ();
<INITIAL>{EOL}		eol_code ();
.			unhandled_character();
%%
static char *
lexer_string (void)
{
        return g_strndup (yytext, yyleng);
}

static void
unrecognized_command (int command)
{
        static GSList *list = NULL;
        GtkWidget *dialog;
        GtkWidget *parent;

        // Don't display a message if we already have
        if (g_slist_find (list, GINT_TO_POINTER (command)) != NULL) {
                return;
        }

        /* add the status to the list so we don't show it again */
        list = g_slist_append (list, GINT_TO_POINTER (command));

        /* display a warning */
        parent = glade_xml_get_widget (warlock_xml, "main_window");
        dialog = gtk_message_dialog_new (GTK_WINDOW (parent),
                        GTK_DIALOG_DESTROY_WITH_PARENT,
                        GTK_MESSAGE_ERROR, GTK_BUTTONS_CLOSE, 
                        "Got and unrecognized command character: %c\n"
                        "Please submit a bug report at http://sourceforge.net/tracker/?group_id=12587&atid=112587"
                        " or send an email to sproctor@gmail.com\n"
                        "Please let us know what you were doing when you got "
                        "this message and any other information that might "
                        "help us figure out what this comand character means.",
                        command);

        gtk_dialog_run (GTK_DIALOG (dialog));
        gtk_widget_destroy (dialog);
}

static void
handle_command (char command, char *params)
{
        int time;
	int len;

	/* TODO the following few lines should be moved into the lexer
	 *      eventually
	 */
	// change the trailing newline to a null
	len = strlen (params);
	if (len >= 1 && params[len - 1] == '\n') {
		params[len - 1] = '\0';
		if (len >= 2 && params[len - 2] == '\r') {
			params[len - 2] = '\0';
		}
	}

	switch (command) {
		case 'A': // initial time setting?
			break;
		case 'B': // character name, and a number (all 0's)
                        simu_connection_send (connection, "GOOD\n");
			break;
		case 'C': // server wants you to disconnect
                        debug ("got a message from the server telling us to "
                                        "quit\n");
                        quit = TRUE;
			break;
		case 'D': // seen at login - function unknown
			break;
		case 'F': // maybe to display pictures?
			break;
		case 'H': // room description to follow
			break;
		case 'I': // items/NPC's/characters to follow
			break;
                case 'J': // something to do with poison
                        break;
                case 'K': // like GSJ but for disease
                        break;
		case 'L': // start of a highlight
			highlighting = TRUE;
			break;
		case 'M': // end of a highlight
			highlighting = FALSE;
			break;
		case 'P': // player's status - see docs for details
			status_set (params);
			break;
		case 'Q': // gives us when the roundtime will end
                        time = atoi (g_strchomp (params));
                        if (time != 0) {
                                new_roundtime (time);
                        }
			break;
		case 'V': // seen at login - function unknown
			break;
		case 'W': // sent when wounds change
			break;
		case 'X': // the number of hit points you have
			break;
		case 'Y': // the number of spirit points you have
			break;
		case 'Z': // the number of mana points you have
			break;
                case 'a': //wounds/severity
                        break;
		case 'b': // scars
			break;
		case 'c': // familiar start
			familiar = TRUE;
			break;
		case 'd': // familiar end
			familiar = FALSE;
			break;
		case 'e': // familiar start (again?)
			familiar= TRUE;
			break;
		case 'f': // familiar end (again?)
			familiar = FALSE;
			break;
		case 'g': // current stance - see docs for details
			break;
		case 'j': // directions you can go - see docs for details
			compass_set_directions (params);
			break;
		case 'l': // left hand contents
			hand_set_contents (LEFT_HAND, parse_hand (params));
			break;
		case 'm': // right hand contents
			hand_set_contents (RIGHT_HAND, parse_hand (params));
			break;
		case 'n': // gives us the spell being prepared (not DR)
			break;
		case 'o': // marks the start of the room description
			  // done waiting in move.
			  // TODO setup hooks for these so we don't have this
			  //      kind of code in this file
			script_moved ();
#ifdef CONFIG_SPIDERMONKEY
			js_script_moved ();
#endif
			room_title = TRUE;
			break;
		case 'p': // marks then end of the room description
			room_title = FALSE;
			break;
		case 'q': // gives us the current game time and signifies that
			  // we should give a status prompt
                        time = atoi (g_strchomp (params));
                        if (time != 0) {
                                warlock_set_time (time);
                        }
			do_prompt ();
			break;
                case 'r': // I have no idea what this command does
                        break;
		case 't': // signifies contents of the hands have changed
			break;
		case 'w':
			if (strcmp (g_strchomp (params), "00003") == 0) {
				death = TRUE;
			} else if (strcmp (g_strchomp (params), "00004") == 0) {
				death = FALSE;
			} else {
                                debug ("unsupported GSw command: %s\n",
                                                g_strchomp (params));
                        }
			break;
		default:
			debug ("unsupported command: %c\n", command);
                        unrecognized_command (command);
                        break;
	}
}

// removes duplicate spaces in a string
static void
compress_whitespace (char *str)
{
	char *ptr;

        for(ptr = str; str[0] != '\0'; str++) {
                if (!isspace (str[0]) || !isspace (str[1])) {
                        ptr[0] = str[0];
                        ptr++;
                }
        }
        ptr[0] = '\0';
}

// follows the simu protocol for what a hand contains
// check docs/SIMU-PROTOCOL for more info
static char *
parse_hand (char *str)
{
	char temp[16];
        char *new_string;
        int i;

        debug ("hand string length: %ui\n", (unsigned int) strlen (str));
        g_assert (strlen (str) == 45);

        new_string = g_new (char, 48);
        new_string[0] = '\0';
        for (i = 0; i < 3; i++) {
                g_strlcpy (temp, str + i * 15, 16);
                g_strlcat (new_string, g_strchomp (temp), 48);
                g_strlcat (new_string, " ", 48);
        }

        compress_whitespace (new_string);

        return new_string;
}

static void
arrival_string (void)
{
	char *string;
	WString *w_string;

	string = g_strndup (yytext, yyleng);
	w_string = w_string_new (g_strchomp (string));
	warlock_view_append ("arrival", w_string);
	w_string_free (w_string, TRUE);
	warlock_view_end_line ("arrival");
}

static void
output_string (void)
{
	WString *string = w_string_new_len (yytext, yyleng);

	if (highlighting) {
		w_string_add_tag (string, MONSTER_TAG, 0, -1);
	}

	if (room_title) {
		w_string_add_tag (string, TITLE_TAG, 0, -1);
	}

	if (familiar) {
		warlock_view_append ("familiar", string);
	} else if (death) {
		warlock_view_append ("death", string);
	} else {
		warlock_view_append (NULL, string);
	}

	w_string_free (string, TRUE);
}

static void
eol_code (void)
{
	if (familiar) {
		warlock_view_end_line ("familiar");
	} else if (death) {
		warlock_view_end_line ("death");
	} else {
		warlock_view_end_line (NULL);
	}
}

int
wizardparse (void)
{
	quit = FALSE;

	for (;;) {
                int val;

                val = wizardlex ();
		if (val == 0) {
			return 0;
		}

		if (quit) {
			return 1;
		}
	}	
}

static void
unhandled_character (void)
{
	WString *string = w_string_new_len (yytext, yyleng);
	debug ("Unhandled character %s\n", string->string->str);
}
