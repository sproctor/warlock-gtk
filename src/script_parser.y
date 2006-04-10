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
static int scripterror (ScriptCommand **command, int line_number, char *string);

/* local variables */
%}

%union {
	char			*string;
	GList			*list;
	ScriptData		*data;
	ScriptCommand		*command;
	ScriptConditional	*conditional;
	ScriptBinaryOp		 binary_op;
	ScriptUnaryOp		 unary_op;
	ScriptCompareOp		 compare_op;
	ScriptTestOp		 test_op;
}

%token <string> SCRIPT_LABEL SCRIPT_STRING SCRIPT_VARIABLE SCRIPT_IF_
%token SCRIPT_EOL SCRIPT_IF SCRIPT_THEN
%token <binary_op> SCRIPT_BINARY_OP
%token <unary_op> SCRIPT_UNARY_OP
%token <compare_op> SCRIPT_COMPARE_OP
%token <test_op> SCRIPT_TEST_OP

%type <command> line command exp
%type <list> arg_list
%type <conditional> conditional
%type <data> script_data

%parse-param { ScriptCommand **command }
%parse-param { int line_number }
%%
parser:
	line				{ *command = $1; YYACCEPT; }
;
line:
          exp SCRIPT_EOL		{ $$ = $1; }
        | SCRIPT_LABEL line		{ $$ = $2; script_save_label ($1); }
;
exp:	/* empty */			{ $$ = NULL; }
        | command			{
		$1->conditional = NULL;
		$$ = $1; }
        | SCRIPT_IF_ command {
		ScriptConditional *conditional;
		ScriptTestExpr *test;
		ScriptData *data;
		data = g_new (ScriptData, 1);
		data->type = SCRIPT_TYPE_STRING;
		data->value.as_string = $1;
		test = g_new (ScriptTestExpr, 1);
		test->op = SCRIPT_OP_EXISTS;
		test->rhs = data;
		conditional = g_new (ScriptConditional, 1);
		conditional->type = SCRIPT_TEST_EXPR;
		conditional->expr.test = test;
		$2->conditional = conditional;
		$$ = $2; }
	| SCRIPT_IF conditional SCRIPT_THEN command {
		$4->conditional = $2;
		$$ = $4; }
;
command:
	arg_list			{
		ScriptCommand *command;
		command = g_new (ScriptCommand, 1);
		command->command = $1;
		command->line_number = line_number;
		$$ = command; }
;
arg_list:
	/* empty */			{ $$ = NULL; }
        | script_data arg_list		{
		$$ = g_list_prepend ($2, $1); }
;
script_data:
	  SCRIPT_STRING			{
	  	$$ = g_new (ScriptData, 1);
		$$->type = SCRIPT_TYPE_STRING;
		$$->value.as_string = $1; }
	| SCRIPT_VARIABLE		{
		$$ = g_new (ScriptData, 1);
		$$->type = SCRIPT_TYPE_VARIABLE;
		$$->value.as_string = $1; }
;
conditional:
	  conditional SCRIPT_BINARY_OP conditional	{
	  	ScriptBinaryExpr *expr;
		expr = g_new (ScriptBinaryExpr, 1);
		expr->op = $2;
		expr->lhs = $1;
		expr->rhs = $3;
	  	$$ = g_new (ScriptConditional, 1);
		$$->type = SCRIPT_BINARY_EXPR;
		$$->expr.binary = expr; }
	| SCRIPT_UNARY_OP conditional	{
		ScriptUnaryExpr *expr;
		g_new (ScriptUnaryExpr, 1);
		expr->op = $1;
		expr->rhs = $2;
		$$ = g_new (ScriptConditional, 1);
		$$->type = SCRIPT_UNARY_EXPR;
		$$->expr.unary = expr; }
	| script_data SCRIPT_COMPARE_OP script_data	{
		ScriptCompareExpr *expr;
		expr = g_new (ScriptCompareExpr, 1);
		expr->op = $2;
		expr->lhs = $1;
		expr->rhs = $3;
		$$ = g_new (ScriptConditional, 1);
		$$->type = SCRIPT_COMPARE_EXPR;
		$$->expr.compare = expr; }
	| SCRIPT_TEST_OP script_data	{
		ScriptTestExpr *expr;
		expr = g_new (ScriptTestExpr, 1);
		expr->op = $1;
		expr->rhs = $2;
		$$ = g_new (ScriptConditional, 1);
		$$->type = SCRIPT_TEST_EXPR;
		$$->expr.test = expr; }
;
%%

/*
 * yyerror() is invoked when the lexer or the parser encounter
 * something they don't recognize [haven't been programmed to 
 * recognize]
 *
 */
static int scripterror (ScriptCommand **command, int line_number, char *string)
{
        debug ("script parser error: %s\n", string);
        return 1;
}
