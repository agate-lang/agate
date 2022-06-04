/* SPDX-License-Identifier: MIT */
/* Copyright (c) 2022 Julien Bernard */
%{

#include <stdlib.h>
#include <stdio.h>

#include "agate-parser.h"

int line = 1;

#define MAX_INTERPOLATION_NESTING 8

int parens[MAX_INTERPOLATION_NESTING];
int parens_count = 0;

%}

%option 8bit
%option never-interactive nounistd
%option warn
%option nodefault noyywrap nounput noinput

%x STRING

BIN   [0-1]
BINP  (0b)
OCT   [0-7]
OCTP  (0o)
DECNZ [1-9]
DEC   [0-9]
HEX   [a-fA-F0-9]
HEXP  (0x)
EXP   (e[+-]?{DEC}+)
ID    [a-zA-Z_]([a-zA-Z0-9_])*
ESC   (\\([0'"\\abfnrtv%]|x[a-fA-F0-9]{2}|u[a-fA-F0-9]{4}|U[a-fA-F0-9]{8}))

%%

"+"           { return '+'; }
"-"           { return '-'; }
"*"           { return '*'; }
"/"           { return '/'; }
"%"           { return '%'; }
"."           { return '.'; }
","           { return ','; }
"!"           { return '!'; }
"~"           { return '~'; }
"|"           { return '|'; }
"&"           { return '&'; }
"^"           { return '^'; }
"<"           { return '<'; }
">"           { return '>'; }
"="           { return '='; }
":"           { return ':'; }
"?"           { return '?'; }
"["           { return '['; }
"]"           { return ']'; }
"{"           { return '{'; }
"}"           { return '}'; }

"("           {
                if (parens_count > 0) { ++parens[parens_count - 1]; }
                return '(';
              }
")"           {
                if (parens_count > 0 && --parens[parens_count - 1] == 0) {
                  --parens_count;
                  BEGIN(STRING);
                } else {
                  return ')';
                }
              }

"as"          { return KW_AS; }
"assert"      { return KW_ASSERT; }
"break"       { return KW_BREAK; }
"class"       { return KW_CLASS; }
"construct"   { return KW_CONSTRUCT; }
"continue"    { return KW_CONTINUE; }
"def"         { return KW_DEF; }
"else"        { return KW_ELSE; }
"false"       { return KW_FALSE; }
"for"         { return KW_FOR; }
"foreign"     { return KW_FOREIGN; }
"if"          { return KW_IF; }
"import"      { return KW_IMPORT; }
"in"          { return KW_IN; }
"is"          { return KW_IS; }
"loop"        { return KW_LOOP; }
"nil"         { return KW_NIL; }
"once"        { return KW_ONCE; }
"return"      { return KW_RETURN; }
"static"      { return KW_STATIC; }
"super"       { return KW_SUPER; }
"this"        { return KW_THIS; }
"true"        { return KW_TRUE; }
"while"       { return KW_WHILE; }
"=="          { return OP_EQ; }
"..."         { return OP_EXCL_RANGE; }
">="          { return OP_GEQ; }
".."          { return OP_INCL_RANGE; }
"&&"          { return OP_LAND; }
"<="          { return OP_LEQ; }
"||"          { return OP_LOR; }
"<<"          { return OP_LSHIFT; }
"!="          { return OP_NE; }
">>"          { return OP_RSHIFT; }
"::"          { return OP_SCOPE; }


{ID}                    { return IDENTIFIER; }
"@"{ID}                 { return FIELD_INSTANCE; }
"@@"{ID}                { return FIELD_CLASS; }

"0"                     { return INT_LITERAL; }
{DECNZ}{DEC}*           { return INT_LITERAL; }
{HEXP}{HEX}+            { return INT_LITERAL; }
{BINP}{BIN}+            { return INT_LITERAL; }
{OCTP}{OCT}+            { return INT_LITERAL; }

{DECNZ}{DEC}*{EXP}              { return FLOAT_LITERAL; }
{DECNZ}{DEC}*"."{DEC}+({EXP})?  { return FLOAT_LITERAL; }
"0"{EXP}                        { return FLOAT_LITERAL; }
"0."{DEC}+({EXP})?              { return FLOAT_LITERAL; }

\"                      { BEGIN(STRING); }

<STRING>{
  "%("                  { parens[parens_count++] = 1; BEGIN(INITIAL); return INTERPOLATION_LITERAL; }
  ([^"\\\n%]|{ESC})     { }
  \"                    { BEGIN(INITIAL); return STRING_LITERAL; }
  \n                    { fprintf(stderr, "No newline permitted in string.\n"); exit(EXIT_FAILURE); }
  .                     { fprintf(stderr, "Invalid character in string: '%s'\n", yytext); exit(EXIT_FAILURE); }
}


"'"([^"'\\\n]{1,4}|{ESC})"'"   { return CHAR_LITERAL; }

"#"[^\n]*               { /* comment */ }

\n                      { ++line; return EOL; }
[\t ]*                  { /* nothing */ }
.                       { fprintf(stderr, "Unknown token: '%s'\n", yytext); exit(EXIT_FAILURE); }

%%
