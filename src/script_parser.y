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
static int scripterror (ScriptCommand **command, const char **label,
		int line_number, char *string);

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
%token SCRIPT_EOL SCRIPT_IF SCRIPT_THEN SCRIPT_OPEN_PAREN SCRIPT_CLOSE_PAREN
%token <binary_op> SCRIPT_BINARY_OP
%token <unary_op> SCRIPT_UNARY_OP
%token <compare_op> SCRIPT_COMPARE_OP
%token <test_op> SCRIPT_TEST_OP

%type <command> line command exp
%type <list> arg_list
%type <conditional> conditional
%type <data> script_data

%parse-param { ScriptCommand **command }
%parse-param { const char **label }
%parse-param { int line_number }
%%
parser:
	line				{
		$1->line_number = line_number;
		*command = $1;
		YYACCEPT; }
;
line:
	  SCRIPT_EOL			{
		$$ = g_new (ScriptCommand, 1);
		$$->conditionals = NULL;
		$$->command = NULL;
	}
        | exp SCRIPT_EOL		{ $$ = $1; }
        | SCRIPT_LABEL line		{
		*label = $1;
		/* make sure that $2 is an actual value (it is safe to assume
		 * currently, but if that changes, change this as well) */
		$$ = $2; }
;
exp:
          command			{
		$$ = $1; }
        | SCRIPT_IF_ exp {
		ScriptConditional *conditional;
		ScriptTestExpr *test;
		test = g_new (ScriptTestExpr, 1);
		test->op = SCRIPT_OP_EXISTS;
		test->arg = g_list_append (NULL, string_to_script_data ($1));
		conditional = g_new (ScriptConditional, 1);
		conditional->type = SCRIPT_TEST_EXPR;
		conditional->expr.test = test;
		$2->conditionals = g_list_append ($2->conditionals, conditional);
		$$ = $2; }
	| SCRIPT_IF conditional SCRIPT_THEN exp {
		$4->conditionals = g_list_append ($4->conditionals, $2);
		$$ = $4; }
;
command:
	arg_list			{
		$$ = g_new (ScriptCommand, 1);
		$$->command = $1;
		$$->line_number = line_number;
		$$->conditionals = NULL; }
;
arg_list:
	/* empty */			{ $$ = NULL; }
        | script_data arg_list		{
		$$ = g_list_prepend ($2, $1); }
;
script_data:
	  SCRIPT_STRING			{
	  	$$ = string_to_script_data ($1); }
	| SCRIPT_VARIABLE		{
		$$ = g_new (ScriptData, 1);
		$$->type = SCRIPT_TYPE_VARIABLE;
		$$->value.as_string = $1; }
;
conditional:
	  SCRIPT_OPEN_PAREN conditional SCRIPT_CLOSE_PAREN	{
	  	$$ = $2; }
	| conditional SCRIPT_BINARY_OP conditional	{
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
		expr = g_new (ScriptUnaryExpr, 1);
		expr->op = $1;
		expr->rhs = $2;
		$$ = g_new (ScriptConditional, 1);
		$$->type = SCRIPT_UNARY_EXPR;
		$$->expr.unary = expr; }
	| arg_list SCRIPT_COMPARE_OP arg_list	{
		ScriptCompareExpr *expr;
		expr = g_new (ScriptCompareExpr, 1);
		expr->op = $2;
		expr->lhs = $1;
		expr->rhs = $3;
		$$ = g_new (ScriptConditional, 1);
		$$->type = SCRIPT_COMPARE_EXPR;
		$$->expr.compare = expr; }
	| SCRIPT_TEST_OP arg_list	{
		ScriptTestExpr *expr;
		expr = g_new (ScriptTestExpr, 1);
		expr->op = $1;
		expr->arg = $2;
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
static int scripterror (ScriptCommand **command, const char **label,
		int line_number, char *string)
{
        debug ("script parser error: %s\n", string);
        return 1;
}
