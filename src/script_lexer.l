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

#include <glib.h>

#include "script.h"
#include "debug.h"
#define YACC_PREFIX script
#include "yacc_mangle.h"
#include "script_parser.h"

#define lexer_string()		g_strndup (yytext, yyleng)

extern gboolean script_running;

%}

%option prefix="script" nounput case-insensitive stack noyy_top_state noyywrap
%x VAR COMMENT STRING IF

%%
<*>(\n|\r\n)			{
	while (YY_START != INITIAL) {
		yy_pop_state ();
	}
	debug ("script eol\n");
	return SCRIPT_EOL;
}
<INITIAL>if_			{
        yy_push_state (IF);
        debug ("script if_\n");
}
<INITIAL>[^[:alpha:][:blank:]]		{
	debug ("comment line\n");
	yy_push_state (COMMENT);
}
<COMMENT>.			{}
<INITIAL>[^:[:blank:]]+:	{
        yylval.string = g_strndup (yytext, yyleng - 1);
	debug ("got label: %s\n", yylval.string);
        return SCRIPT_LABEL;
}
<STRING>%			{
	yy_push_state (VAR);
}
<VAR>[_[:alnum:]]+		{
        yy_pop_state ();
        yylval.string = lexer_string ();
        debug ("script variable: %s\n", yylval.string);
        return SCRIPT_VARIABLE;
}
<IF>[_[:alnum:]]+		{
        yy_pop_state ();
        yylval.string = lexer_string ();
        debug ("script if: %s\n", yylval.string);
        return SCRIPT_IF_;
}
<VAR>.				{
	yy_pop_state ();
	yymore ();
	yylval.string = lexer_string ();
	debug ("script string: %s\n", yylval.string);
	return SCRIPT_STRING;
}
<INITIAL>[[:alpha:]]		{
	yy_push_state (STRING);
	yymore ();
}
<STRING>[^%\r\n]+		{
        yylval.string = lexer_string ();
        debug ("script string: %s\n", yylval.string);
        return SCRIPT_STRING;
}
<INITIAL>[[:blank:]]+		{}
.				debug ("Unhandled char: %s\n", lexer_string ());
%%
