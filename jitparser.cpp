#include "jitparser.hpp"

#include <istream>
using namespace std;

namespace nanjit {
  class Scanner;
}

#include "jitlexer.h"
#include "parser.tab.hpp"

ModuleAST *nanjit::parse(istream &source_code_stream)
{
  ModuleAST *ast = NULL;
  nanjit::Scanner scanner(&source_code_stream);
  nanjit::BisonParser parser(&ast, scanner);
  parser.parse();
  return ast;
};
