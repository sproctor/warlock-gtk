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

#ifndef YACC_PREFIX
#define YACC_PREFIX	yy
#endif

#define PASTE1(x, y)	x ## y
#define PASTE(x, y)	PASTE1(x, y)
#define YACC_MANGLE(x)	PASTE(YACC_PREFIX, x)

#define	yymaxdepth 	YACC_MANGLE(maxdepth)
#define	yyparse		YACC_MANGLE(parse)
#define	yylex		PASTE(YACC_PREFIX, lex)
#define	yyerror		PASTE(YACC_PREFIX, error)
#define	yylval		PASTE(YACC_PREFIX, lval)
#define	yychar		PASTE(YACC_PREFIX, char)
#define	yydebug		PASTE(YACC_PREFIX, debug)
#define	yypact		PASTE(YACC_PREFIX, pact)
#define	yyr1		PASTE(YACC_PREFIX, r1)
#define	yyr2		PASTE(YACC_PREFIX, r2)
#define	yydef		PASTE(YACC_PREFIX, def)
#define	yychk		PASTE(YACC_PREFIX, chk)
#define	yypgo		PASTE(YACC_PREFIX, pgo)
#define	yyact		PASTE(YACC_PREFIX, act)
#define	yyexca		PASTE(YACC_PREFIX, exca)
#define yyerrflag 	PASTE(YACC_PREFIX, errflag)
#define yynerrs		PASTE(YACC_PREFIX, nerrs)
#define	yyps		PASTE(YACC_PREFIX, ps)
#define	yypv		PASTE(YACC_PREFIX, pv)
#define	yys		PASTE(YACC_PREFIX, s)
#define	yy_yys		PASTE(YACC_PREFIX, _yys)
#define	yystate		PASTE(YACC_PREFIX, state)
#define	yytmp		PASTE(YACC_PREFIX, tmp)
#define	yyv		PASTE(YACC_PREFIX, v)
#define	yy_yyv		PASTE(YACC_PREFIX, _yyv)
#define	yyval		PASTE(YACC_PREFIX, val)
#define	yylloc		PASTE(YACC_PREFIX, lloc)
#define yyreds		PASTE(YACC_PREFIX, reds)
#define yytoks		PASTE(YACC_PREFIX, toks)
#define yylhs		PASTE(YACC_PREFIX, yylhs)
#define yylen		PASTE(YACC_PREFIX, yylen)
#define yydefred 	PASTE(YACC_PREFIX, yydefred)
#define yydgoto		PASTE(YACC_PREFIX, yydgoto)
#define yysindex 	PASTE(YACC_PREFIX, sindex)
#define yyrindex 	PASTE(YACC_PREFIX, rindex)
#define yygindex 	PASTE(YACC_PREFIX, gindex)
#define yytable	 	PASTE(YACC_PREFIX, table)
#define yycheck	 	PASTE(YACC_PREFIX, check)
#define yyname   	PASTE(YACC_PREFIX, name)
#define yyrule   	PASTE(YACC_PREFIX, rule)
