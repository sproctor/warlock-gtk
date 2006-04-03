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

#include <glib.h>

#define YACC_PREFIX script
#include "yacc_mangle.h"
#include "debug.h"
#include "script.h"

extern int scriptlex (void);

/* local functions */
static int scripterror (char *string);

/* external variables */
extern GList *commands;
extern int curr_line_number;

/* local variables */
%}

%union {
	char *string;
	GList *list;
	ScriptCommand *command;
}

%token <string> SCRIPT_LABEL SCRIPT_STRING SCRIPT_VARIABLE SCRIPT_IF_
%token SCRIPT_EOL

%type <list> arg_list
%type <command> command
%%
parser:
	line				{ YYACCEPT; }
;
line:
        exp SCRIPT_EOL			{ }
        | SCRIPT_LABEL line		{ script_save_label ($1); }
;
exp:	/* empty */			{
		ScriptCommand *command;
		command = g_new (ScriptCommand, 1);
		command->line_number = curr_line_number;
		command->command = NULL;
		command->depends_on = NULL;
		commands = g_list_append (commands, command); }
        | command			{
		$1->depends_on = NULL;
		commands = g_list_append (commands, $1); }
        | SCRIPT_IF_ command {
		$2->depends_on = g_ascii_strdown ($1, -1);
		commands = g_list_append (commands, $2); }
;
command:
	arg_list			{
		ScriptCommand *command;
		command = g_new (ScriptCommand, 1);
		command->command = $1;
		command->line_number = curr_line_number;
		$$ = command; }
;
arg_list:
	/* empty */			{ $$ = NULL; }
        | SCRIPT_STRING arg_list	{
		ScriptData *data = g_new (ScriptData, 1);
		data->type = SCRIPT_TYPE_STRING;
		data->value.as_string = $1;
		$$ = g_list_prepend ($2, data); }
        | SCRIPT_VARIABLE arg_list      {
		ScriptData *data;
                data = g_new (ScriptData, 1);
		data->type = SCRIPT_TYPE_VARIABLE;
		data->value.as_string = $1;
		$$ = g_list_prepend ($2, data); }
;
%%

/*
 * yyerror() is invoked when the lexer or the parser encounter
 * something they don't recognize [haven't been programmed to 
 * recognize]
 *
 */
static int scripterror (char *string)
{
        debug ("script parser error: %s\n", string);
        return 1;
}
