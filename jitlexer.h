#ifndef __JITSCANNER_HPP__
#define __JITSCANNER_HPP__

/* FlexLexer.h doesn't use a normal header guard */
#ifndef yyFlexLexerOnce
#include <FlexLexer.h>
#endif

#include "parser.tab.hpp"

namespace nanjit {
  class Scanner : public yyFlexLexer {
    nanjit::BisonParser::semantic_type *yylval;
    nanjit::BisonParser::location_type *yylloc;

  public:
    Scanner(std::istream *in) : yyFlexLexer(in), yylval(NULL), yylloc(NULL) {};

    /* Replace yylex with a version bison can use */
    int yylex(nanjit::BisonParser::semantic_type *lval,
              nanjit::BisonParser::location_type *loc)
    {
      yylval = lval;
      yylloc = loc;
      return yylex();
      yylval = NULL;
      yylloc = NULL;
    }

  private:
    int yylex();
  };
}

#endif /* __JITSCANNER_HPP__ */
