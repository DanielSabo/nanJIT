%skeleton "lalr1.cc"
%require  "2.5"
%defines
%name-prefix "nanjit"
%define parser_class_name "BisonParser"
%locations

%code requires {
namespace nanjit {
  class Scanner;
}

#include "ast.h"
}

%code {
#include <cstdio>
#include <iostream>
#include <exception>
using namespace std;
}

// Define the bison union, this must be able to contain all types that can
// be output by Flex or returned by a Bison rule.
%union {
  char *sval;
  ExprAST *expr_ast;
  IdentifierExprAST *identifier_ast;
  ComparisonAST *comparison_ast;
  BlockAST *block_ast;
  FunctionAST *function_ast;
  FunctionArgAST *function_arg;
  FunctionArgListAST *function_arg_list;
  CallArgListAST *call_arg_list;
}

%code {
  #include "jitlexer.h"
  static int yylex(nanjit::BisonParser::semantic_type *yylval,
                   nanjit::BisonParser::location_type *location,
                   nanjit::Scanner &scanner);
}


// Define the "terminal symbol" tokens, also associate the token
// with a field in the union if it returns additional data.
%token <sval> INT
%token <sval> FLOAT
%token <sval> DOUBLE
%token <sval> IDENTIFIER
%token <sval> TYPENAME
%token DOT
%token PLUS
%token MINUS
%token ASTERISK
%token SLASH
%token EQUAL
%token PAIR_EQUAL
%token GREATER_THAN
%token GREATER_THAN_OR_EQUAL
%token LESS_THAN
%token LESS_THAN_OR_EQUAL
%token SEMICOLON
%token LPAREN
%token RPAREN
%token LCURL
%token RCURL
%token COMMA
%token RETURN
%token IF
%token ELSE

%type <function_ast> function
%type <function_arg> argument
%type <function_arg_list> arguments
%type <block_ast> block
%type <expr_ast> statement
%type <expr_ast> expression
%type <comparison_ast> comparison
%type <expr_ast> value
%type <identifier_ast> identifier
%type <identifier_ast> type_name
%type <call_arg_list> call_arguments

%destructor { free ($$); } INT FLOAT DOUBLE
%destructor { free ($$); } IDENTIFIER
%destructor { free ($$); } TYPENAME
%destructor { delete $$; } function argument arguments block
%destructor { delete $$; } comparison
%destructor { delete $$; } statement expression value
%destructor { delete $$; } identifier type_name call_arguments

%parse-param { ModuleAST **module }
%lex-param   { Scanner &scanner}
%parse-param { Scanner &scanner}

%error-verbose

%left EQUAL
%left PAIR_EQUAL GREATER_THAN GREATER_THAN_OR_EQUAL LESS_THAN LESS_THAN_OR_EQUAL
%left PLUS MINUS
%left ASTERISK SLASH

%%
module:
  function { if (!*module) { *module = new ModuleAST(); }; (*module)->prependFunction($1); }
  | function module { (*module)->prependFunction($1); }

function:
  type_name identifier LPAREN arguments RPAREN LCURL block RCURL { $$ = new FunctionAST($1, $2, $4, $7); }

arguments:
  argument { $$ = new FunctionArgListAST($1); }
  | argument COMMA arguments { $3->prependArg($1); $$ = $3; }

argument:
  type_name identifier { $$ = new FunctionArgAST($1, $2); }

block:
  statement { $$ = new BlockAST($1); }
  | statement block { $2->PrependExpr($1); $$ = $2; }

statement:
  expression SEMICOLON { $$ = $1; }
  | IF LPAREN comparison RPAREN LCURL block RCURL { $$ = new IfElseAST($3, $6, NULL); }
  | RETURN SEMICOLON  { $$ = new ReturnAST(); }
  | RETURN expression SEMICOLON { $$ = new ReturnAST($2); }

comparison:
  expression PAIR_EQUAL expression { $$ = new ComparisonAST(ComparisonAST::EqualTo, $1, $3); }
  | expression GREATER_THAN expression { $$ = new ComparisonAST(ComparisonAST::GreaterThan, $1, $3); }
  | expression GREATER_THAN_OR_EQUAL expression { $$ = new ComparisonAST(ComparisonAST::GreaterThanOrEqual, $1, $3); }
  | expression LESS_THAN expression { $$ = new ComparisonAST(ComparisonAST::LessThan, $1, $3); }
  | expression LESS_THAN_OR_EQUAL expression { $$ = new ComparisonAST(ComparisonAST::LessThanOrEqual, $1, $3); }

expression:
  value { $$ = $1; }
  | comparison { $$ = $1; }
  | LPAREN type_name RPAREN LPAREN expression COMMA expression COMMA expression COMMA expression RPAREN { $$ = new VectorConstructorAST($2, $5, $7, $9, $11); }
  | identifier EQUAL expression    { $$ = new AssignmentExprAST($1, $3); }
  | type_name identifier EQUAL expression { $$ = new AssignmentExprAST($1, $2, $4); }
  | expression PLUS expression     { $$ = new BinaryExprAST('+', $1, $3); }
  | expression MINUS expression    { $$ = new BinaryExprAST('-', $1, $3); }
  | expression ASTERISK expression { $$ = new BinaryExprAST('*', $1, $3); }
  | expression SLASH expression    { $$ = new BinaryExprAST('/', $1, $3); }
  | identifier DOT identifier { $$ = new ShuffleSelfAST($1, $3); }
  | LPAREN expression RPAREN { $$ = $2; }
  | identifier LPAREN RPAREN { $$ = new CallAST($1, new CallArgListAST()); }
  | identifier LPAREN call_arguments RPAREN { $$ = new CallAST($1, $3); }

call_arguments:
  expression { $$ = new CallArgListAST($1); }
  | expression COMMA call_arguments { $3->prependArg($1); $$ = $3; }

type_name:
  TYPENAME { $$ = new IdentifierExprAST($1); free($1); }

value: 
  INT { $$ = new IntExprAST($1); free($1); }
  | FLOAT { $$ = new FloatExprAST($1); free($1); }
  | DOUBLE { $$ = new DoubleExprAST($1); free($1); }
  | identifier { $$ = $1; }
  | MINUS value { $$ = new BinaryExprAST('-', new IntExprAST("0"), $2); }

identifier:
  IDENTIFIER { $$ = new IdentifierExprAST($1); free($1); }

%%
#include "parser.tab.hpp"

void nanjit::BisonParser::error(const nanjit::BisonParser::location_type &l,
                                const std::string &err_message)
{
  cout << "Parse error (" << l.begin.line << ":" << l.begin.column << "): " << err_message << endl;
}

#include "jitlexer.h"
static int yylex(nanjit::BisonParser::semantic_type *yylval,
                 nanjit::BisonParser::location_type *location,
                 nanjit::Scanner &scanner)
{
   return( scanner.yylex(yylval, location) );
}
