%{
#include <string>
#include <iostream>

#include "jitlexer.h"

typedef nanjit::BisonParser::token token;

/* FIXME: The end column will be wrong if the token spans a line */
#define YY_USER_ACTION {\
  yylloc->step();\
  yylloc->columns(strlen(yytext));\
  yylloc->begin.line = yylineno;\
  yylloc->end.line = yylineno;\
}

/* Override he default yylex name with our namespaced version */
#undef YY_DECL
#define YY_DECL int nanjit::Scanner::yylex()
%}

%option yyclass="Scanner"
%option yylineno
%option noyywrap
%option c++

%x IN_COMMENT
%%
[ \t]                ;
\n                   { yylloc->end.line += 1; yylloc->end.column = 0; };
";"                  { return token::SEMICOLON; };
"."                  { return token::DOT; };
"+"                  { return token::PLUS; };
"-"                  { return token::MINUS; };
"*"                  { return token::ASTERISK; };
"/"                  { return token::SLASH; };
"="                  { return token::EQUAL; };
"=="                 { return token::PAIR_EQUAL; };
">"                  { return token::GREATER_THAN; };
">="                 { return token::GREATER_THAN_OR_EQUAL; };
"<"                  { return token::LESS_THAN; };
"<="                 { return token::LESS_THAN_OR_EQUAL; };
"("                  { return token::LPAREN; };
")"                  { return token::RPAREN; };
"{"                  { return token::LCURL; };
"}"                  { return token::RCURL; };
","                  { return token::COMMA; };
"return"             { return token::RETURN; };
"if"                 { return token::IF; };
"else"               { return token::ELSE; };
"float"[234]?        { yylval->sval = strdup(yytext); return token::TYPENAME; };
"int"[234]?          { yylval->sval = strdup(yytext); return token::TYPENAME; };
"uint"[234]?         { yylval->sval = strdup(yytext); return token::TYPENAME; };
"short"[234]?        { yylval->sval = strdup(yytext); return token::TYPENAME; };
"ushort"[234]?       { yylval->sval = strdup(yytext); return token::TYPENAME; };
"bool"[234]?         { yylval->sval = strdup(yytext); return token::TYPENAME; };
[A-Za-z_][A-Za-z0-9_]* { yylval->sval = strdup(yytext); return token::IDENTIFIER; };
[0-9]+"."?[0-9]*[fF] { yylval->sval = strdup(yytext); return token::FLOAT; };
[0-9]+"."[0-9]*      { yylval->sval = strdup(yytext); return token::DOUBLE; };
[0-9]+               { yylval->sval = strdup(yytext); return token::INT; };

"//".*               ;

  /* Taken from the Flex FAQ to match C style comments */
<INITIAL>{
"/*"              BEGIN(IN_COMMENT);
}
<IN_COMMENT>{
"*/"      BEGIN(INITIAL);
[^*\n]+   // eat comment in chunks
"*"       // eat the lone star
\n        ;
}
%%
