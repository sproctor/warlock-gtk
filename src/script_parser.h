/* A Bison parser, made by GNU Bison 2.1.  */

/* Skeleton parser for Yacc-like parsing with Bison,
   Copyright (C) 1984, 1989, 1990, 2000, 2001, 2002, 2003, 2004, 2005 Free Software Foundation, Inc.

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU General Public License as published by
   the Free Software Foundation; either version 2, or (at your option)
   any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU General Public License for more details.

   You should have received a copy of the GNU General Public License
   along with this program; if not, write to the Free Software
   Foundation, Inc., 51 Franklin Street, Fifth Floor,
   Boston, MA 02110-1301, USA.  */

/* As a special exception, when this file is copied by Bison into a
   Bison output file, you may use that output file without restriction.
   This special exception was added by the Free Software Foundation
   in version 1.24 of Bison.  */

/* Tokens.  */
#ifndef YYTOKENTYPE
# define YYTOKENTYPE
   /* Put the tokens into the symbol table, so that GDB and other debuggers
      know about them.  */
   enum yytokentype {
     SCRIPT_LABEL = 258,
     SCRIPT_STRING = 259,
     SCRIPT_VARIABLE = 260,
     SCRIPT_IF_ = 261,
     SCRIPT_EOL = 262,
     SCRIPT_IF = 263,
     SCRIPT_THEN = 264,
     SCRIPT_BINARY_OP = 265,
     SCRIPT_UNARY_OP = 266,
     SCRIPT_COMPARE_OP = 267,
     SCRIPT_TEST_OP = 268
   };
#endif
/* Tokens.  */
#define SCRIPT_LABEL 258
#define SCRIPT_STRING 259
#define SCRIPT_VARIABLE 260
#define SCRIPT_IF_ 261
#define SCRIPT_EOL 262
#define SCRIPT_IF 263
#define SCRIPT_THEN 264
#define SCRIPT_BINARY_OP 265
#define SCRIPT_UNARY_OP 266
#define SCRIPT_COMPARE_OP 267
#define SCRIPT_TEST_OP 268




#if ! defined (YYSTYPE) && ! defined (YYSTYPE_IS_DECLARED)
#line 41 "script_parser.y"
typedef union YYSTYPE {
	char			*string;
	GList			*list;
	ScriptData		*data;
	ScriptCommand		*command;
	ScriptConditional	*conditional;
	ScriptBinaryOp		 binary_op;
	ScriptUnaryOp		 unary_op;
	ScriptCompareOp		 compare_op;
	ScriptTestOp		 test_op;
} YYSTYPE;
/* Line 1447 of yacc.c.  */
#line 76 "script_parser.h"
# define yystype YYSTYPE /* obsolescent; will be withdrawn */
# define YYSTYPE_IS_DECLARED 1
# define YYSTYPE_IS_TRIVIAL 1
#endif

extern YYSTYPE yylval;



