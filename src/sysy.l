%option noyywrap
%option nounput
%option noinput

%{

#include <cstdlib>
#include <string>
#include "sysy.tab.hpp"

using namespace std;

%}

WhiteSpace    [ \t\n\r]*
LineComment   "//".*
BlockComments "/*"([^*]|(\*+[^*/]))*\*+\/

RelOp         <=|>=|<|>
EqOp          ==|!=
LAndOp        &&
LOrOp         \|\|

Identifier    [a-zA-Z_][a-zA-Z0-9_]*

Decimal       [1-9][0-9]*
Octal         0[0-7]*
Hexadecimal   0[xX][0-9a-fA-F]+

%%

{WhiteSpace}    {  }
{LineComment}   {  }
{BlockComments} {  }

"int"           { return INT; }
"const"         { return CONST; }
"return"        { return RETURN; }
"if"            { return IF; }
"else"          { return ELSE; }
"while"         { return WHILE; }
"break"         { return BREAK; }
"continue"      { return CONTINUE; }
"void"          { return VOID; }

{RelOp}         { yylval.str_val = new string(yytext); return RELOP; }
{EqOp}          { yylval.str_val = new string(yytext); return EQOP; }
{LAndOp}        { return LANDOP; }
{LOrOp}         { return LOROP; }

{Identifier}    { yylval.str_val = new string(yytext); return IDENT; }

{Decimal}       { yylval.int_val = strtol(yytext, nullptr, 0); return INT_CONST; }
{Octal}         { yylval.int_val = strtol(yytext, nullptr, 0); return INT_CONST; }
{Hexadecimal}   { yylval.int_val = strtol(yytext, nullptr, 0); return INT_CONST; }

.               { return yytext[0]; }

%%