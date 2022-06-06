// SPDX-License-Identifier: MIT
// Copyright (c) 2022 Julien Bernard
%{

#include <stdio.h>
#include <stdlib.h>

#include "agate-lexer.h"

extern int line;

static int yyerror(const char *message) {
  printf("line %d: %s\n", line, message);
  return 0;
}

%}

%token EOM  0            "end of module file"
%token EOL               "end of line"

%token CHAR_LITERAL      "char literal"
%token INT_LITERAL       "integer literal"
%token FLOAT_LITERAL     "float literal"
%token STRING_LITERAL    "string literal"
%token INTERPOLATION_LITERAL "interpolation literal"
%token IDENTIFIER        "identifier"
%token FIELD_INSTANCE    "instance field"
%token FIELD_CLASS       "class field"

/* keywords */
%token          KW_AS             "as"
%token          KW_ASSERT         "assert"
%token          KW_BREAK          "break"
%token          KW_CLASS          "class"
%token          KW_CONSTRUCT      "construct"
%token          KW_CONTINUE       "continue"
%token          KW_DEF            "def"
%token          KW_ELSE           "else"
%token          KW_FALSE          "false"
%token          KW_FOR            "for"
%token          KW_FOREIGN        "foreign"
%token          KW_IF             "if"
%token          KW_IMPORT         "import"
%token          KW_IN             "in"
%token          KW_IS             "is"
%token          KW_LOOP           "loop"
%token          KW_NIL            "nil"
%token          KW_ONCE           "once"
%token          KW_RETURN         "return"
%token          KW_STATIC         "static"
%token          KW_SUPER          "super"
%token          KW_THIS           "this"
%token          KW_TRUE           "true"
%token          KW_WHILE          "while"
/* operators */
%token          OP_EQ             "=="
%token          OP_EXCL_RANGE     "..."
%token          OP_GEQ            ">="
%token          OP_INCL_RANGE     ".."
%token          OP_LAND           "&&"
%token          OP_LEQ            "<="
%token          OP_LOR            "||"
%token          OP_LSHIFT         "<<"
%token          OP_NE             "!="
%token          OP_RSHIFT         ">>"

%start unit

%define parse.error verbose

%debug
%defines
%verbose

%%

unit:
    maybe_eol declarations
  ;

declarations:
    /* empty */
  | declarations decl eol
  ;

decl:
    import
  | decl_function
  | decl_variable
  | decl_class
  | statement
  ;

import:
    KW_IMPORT STRING_LITERAL
  | KW_IMPORT STRING_LITERAL KW_FOR identifiers
  ;

identifiers:
    id
  | identifiers ',' id
  ;

id:
    IDENTIFIER
  | IDENTIFIER KW_AS IDENTIFIER
  ;

block:
    '{' eol declarations '}'
  | '{' expr '}'
  | '{' '}'
  ;

decl_function:
    KW_DEF IDENTIFIER '(' parameters ')' block
  ;

parameters:
    /* empty */
  | IDENTIFIER
  | parameters ',' IDENTIFIER
  ;

subscript_parameters:
    IDENTIFIER
  | parameters ',' IDENTIFIER
  ;

decl_variable:
    KW_DEF IDENTIFIER
  | KW_DEF IDENTIFIER '=' expr
  ;

decl_class:
    KW_CLASS IDENTIFIER class_body
  | KW_FOREIGN KW_CLASS IDENTIFIER class_body
  | KW_CLASS IDENTIFIER KW_IS expr_primary class_body
  | KW_FOREIGN KW_CLASS IDENTIFIER KW_IS expr_primary class_body
  ;

class_body:
    '{' eol methods '}'
  | '{' '}'
  ;

methods:
    /* empty */
  | methods method eol
  ;

method:
    KW_CONSTRUCT IDENTIFIER '(' parameters ')' block
  | decorator method_signature block
  | KW_FOREIGN decorator method_signature
  ;

method_signature:
    IDENTIFIER '(' parameters ')'
  | IDENTIFIER
  | IDENTIFIER '=' '(' IDENTIFIER ')'
  | binary_operator '(' IDENTIFIER ')'
  | unary_operator
  | '[' subscript_parameters ']'
  | '[' subscript_parameters ']' '=' '(' IDENTIFIER ')'
  ;

decorator:
    /* empty */
  | KW_STATIC
  ;

binary_operator:
    '&'
  | '/'
  | OP_EQ
  | OP_EXCL_RANGE
  | '>'
  | OP_GEQ
  | OP_INCL_RANGE
  | KW_IS
  | '<'
  | OP_LEQ
  | '-'
  | '%'
  | OP_NE
  | '|'
  | '+'
  | OP_LSHIFT
  | OP_RSHIFT
  | '*'
  | '^'
  ;

unary_operator:
    '+'
  | '-'
  | '!'
  | '~'
  ;

statement:
    stmt_once
  | stmt_loop
  | stmt_assert
  | stmt_if
  | stmt_for
  | stmt_while
  | KW_BREAK
  | KW_CONTINUE
  | stmt_return
  | expr
  ;

stmt_once:
    KW_ONCE block
  ;

stmt_loop:
    KW_LOOP block
  ;

stmt_assert:
    KW_ASSERT '(' expr ',' expr ')'
  ;

stmt_if:
    KW_IF '(' expr ')' block
  | KW_IF '(' expr ')' block KW_ELSE block
  | KW_IF '(' expr ')' block KW_ELSE stmt_if
  ;

stmt_for:
    KW_FOR '(' IDENTIFIER KW_IN expr ')' block
  ;

stmt_while:
    KW_WHILE '(' expr ')' block
  ;

stmt_return:
    KW_RETURN
  | KW_RETURN expr
  ;

expr:
    expr_assignment
  ;

expr_assignment:
    expr_postfix '=' expr
  | expr_conditional
  ;

expr_conditional:
    expr_logical_or '?' expr ':' expr_conditional
  | expr_logical_or
  ;

expr_logical_or:
    expr_logical_or OP_LOR expr_logical_and
  | expr_logical_and
  ;

expr_logical_and:
    expr_logical_and OP_LAND expr_equality
  | expr_equality
  ;

expr_equality:
    expr_equality OP_EQ expr_is
  | expr_equality OP_NE expr_is
  | expr_is
  ;

expr_is:
    expr_is KW_IS expr_comparison
  | expr_comparison
  ;

expr_comparison:
    expr_comparison '<' expr_bitwise_or
  | expr_comparison '>' expr_bitwise_or
  | expr_comparison OP_LEQ expr_bitwise_or
  | expr_comparison OP_GEQ expr_bitwise_or
  | expr_bitwise_or
  ;

expr_bitwise_or:
    expr_bitwise_or '|' expr_bitwise_xor
  | expr_bitwise_xor
  ;

expr_bitwise_xor:
    expr_bitwise_xor '^' expr_bitwise_and
  | expr_bitwise_and
  ;

expr_bitwise_and:
    expr_bitwise_and '&' expr_shift
  | expr_shift
  ;

expr_shift:
    expr_shift OP_LSHIFT expr_range
  | expr_shift OP_RSHIFT expr_range
  | expr_range
  ;

expr_range:
    expr_range OP_INCL_RANGE expr_addition
  | expr_range OP_EXCL_RANGE expr_addition
  | expr_addition
  ;

expr_addition:
    expr_addition '+' expr_multiplication
  | expr_addition '-' expr_multiplication
  | expr_multiplication
  ;

expr_multiplication:
    expr_multiplication '*' expr_unary
  | expr_multiplication '/' expr_unary
  | expr_multiplication '%' expr_unary
  | expr_unary
  ;

expr_unary:
    expr_postfix
  | '+' expr_unary
  | '-' expr_unary
  | '!' expr_unary
  | '~' expr_unary
  ;

expr_postfix:
    expr_primary
  | expr_postfix '[' arguments ']'
  | expr_postfix call_arguments
  | expr_postfix '.' IDENTIFIER
  | expr_postfix '.' IDENTIFIER lambda
  ;

call_arguments:
    '(' ')'
  | '(' ')' lambda
  | '(' arguments ')'
  | '(' arguments ')' lambda
  ;

lambda:
    '{' eol declarations '}'
  | '{' expr '}'
  | '{' '|' lambda_parameters '|' eol declarations '}'
  | '{' '|' lambda_parameters '|' expr '}'
  | '{' '}'
  ;

lambda_parameters:
    IDENTIFIER
  | lambda_parameters ',' IDENTIFIER
  ;

arguments:
    expr
  | arguments ',' expr
  ;

expr_primary:
    IDENTIFIER
  | FIELD_INSTANCE
  | FIELD_CLASS
  | '.' IDENTIFIER
  | KW_NIL
  | KW_TRUE
  | KW_FALSE
  | KW_THIS
  | KW_SUPER
  | CHAR_LITERAL
  | INT_LITERAL
  | FLOAT_LITERAL
  | string
  | '(' expr ')'
  | compound
  ;

string:
    STRING_LITERAL
  | INTERPOLATION_LITERAL expr string
  ;

compound:
    '[' maybe_eol ']'
  | '[' elements maybe_eol ']'
  | '[' elements ',' maybe_eol ']'
  | '{' maybe_eol '}'
  | '{' members maybe_eol '}'
  | '{' members ',' maybe_eol '}'
  ;

elements:
    maybe_eol expr
  | elements ',' maybe_eol  expr
  ;

members:
    maybe_eol member
  | members ',' maybe_eol member
  ;

member:
    expr_unary ':' maybe_eol expr
  ;

eol:
    EOL
  | eol EOL
  ;

maybe_eol:
    /* empty */
  | eol
  ;

%%

