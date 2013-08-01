#ifndef __JITPARSER_H__
#define __JITPARSER_H__
#include <istream>
#include "ast.h"

namespace nanjit {
  ModuleAST *parse(std::istream &source_code_stream);
}

#endif /* __JITPARSER_H__ */
