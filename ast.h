#ifndef __AST_HPP__
#define __AST_HPP__
#ifndef __cplusplus
typedef struct _BlockAST BlockAST;
typedef struct _ExprAST ExprAST;
typedef struct _IdentifierExprAST IdentifierExprAST;
typedef struct _FunctionAST FunctionAST;
typedef struct _FunctionArgAST FunctionArgAST;
typedef struct _FunctionArgListAST FunctionArgListAST;
typedef struct _CallArgListAST CallArgListAST;
typedef struct _ComparisonAST ComparisonAST;
#else
#include <list>
#include <vector>
#include <map>
#include <memory>
#include <string>
#include <ostream>

namespace llvm {
  class Value;
  class Module;
};

namespace nanjit {
  class TypeInfo;
}

class ScopeContext;

class SyntaxErrorException : public std::exception
{
  std::string error_string;
public:
  SyntaxErrorException (std::string e) : error_string(e.c_str()) {}
  SyntaxErrorException (const char *e) : error_string(e) {}
  ~SyntaxErrorException() throw() {}
  const char* what() const throw() { return error_string.c_str(); }
};

class ASTNode {
private:
  int line_number;
  int column_number;
public:
  ASTNode();
  void setLocation(int line, int col);
  SyntaxErrorException genSyntaxError(std::string e);
};


class ExprAST : public ASTNode {
public:
  virtual ~ExprAST() {}
  virtual llvm::Value *codegen(ScopeContext *scope);
  virtual nanjit::TypeInfo getResultType(ScopeContext *scope);
  virtual std::ostream& print(std::ostream& os);
};

class DoubleExprAST : public ExprAST {
  std::string str_value;
public:
  DoubleExprAST(std::string val);
  virtual llvm::Value *codegen(ScopeContext *scope);
  virtual nanjit::TypeInfo getResultType(ScopeContext *scope);
  virtual std::ostream& print(std::ostream& os);
};

class FloatExprAST : public ExprAST {
  std::string str_value;
public:
  FloatExprAST(std::string val);
  virtual llvm::Value *codegen(ScopeContext *scope);
  virtual nanjit::TypeInfo getResultType(ScopeContext *scope);
  virtual std::ostream& print(std::ostream& os);
};

class IntExprAST : public ExprAST {
  std::string str_value;
public:
  IntExprAST(std::string val);
  virtual llvm::Value *codegen(ScopeContext *scope);
  virtual nanjit::TypeInfo getResultType(ScopeContext *scope);
  virtual std::ostream& print(std::ostream& os);
};

class IdentifierExprAST : public ExprAST {
  std::string Name;
public:
  IdentifierExprAST(const std::string &name) : Name(name) {};
  std::string getName() { return Name; };
  virtual llvm::Value *codegen(ScopeContext *scope);
  virtual nanjit::TypeInfo getResultType(ScopeContext *scope);
  virtual std::ostream& print(std::ostream& os);
};

class BinaryExprAST : public ExprAST {
  char Op;
  std::auto_ptr<ExprAST> LHS, RHS;
public:
  BinaryExprAST(char op, ExprAST *lhs, ExprAST *rhs) 
    : Op(op), LHS(lhs), RHS(rhs) {}
  virtual llvm::Value *codegen(ScopeContext *scope);
  virtual nanjit::TypeInfo getResultType(ScopeContext *scope);
  virtual std::ostream& print(std::ostream& os);
};

class AssignmentExprAST : public ExprAST {
  std::auto_ptr<IdentifierExprAST> LHSType;
  std::auto_ptr<IdentifierExprAST> LHS;
  std::auto_ptr<ExprAST> RHS;
public:
  AssignmentExprAST(IdentifierExprAST *lhs, ExprAST *rhs)
    : LHS(lhs), RHS(rhs) {}
  AssignmentExprAST(IdentifierExprAST *lhstype, IdentifierExprAST *lhs, ExprAST *rhs)
    : LHSType(lhstype), LHS(lhs), RHS(rhs) {}
  virtual llvm::Value *codegen(ScopeContext *scope);
  virtual std::ostream& print(std::ostream& os);
};

class ShuffleSelfAST : public ExprAST {
  std::auto_ptr<IdentifierExprAST> Target;
  std::auto_ptr<IdentifierExprAST> Access;
public:
  ShuffleSelfAST(IdentifierExprAST *target, IdentifierExprAST *access)
    : Target(target), Access(access) {}
  virtual llvm::Value *codegen(ScopeContext *scope);
  virtual nanjit::TypeInfo getResultType(ScopeContext *scope);
  virtual std::ostream& print(std::ostream& os);
};

class CallArgListAST {
  std::vector<ExprAST *> Args;
public:
  CallArgListAST();
  CallArgListAST(ExprAST *arg);
  void prependArg(ExprAST *arg);
  std::vector<ExprAST *> &getArgsList() { return Args; };
  virtual std::ostream& print(std::ostream& os);

  virtual ~CallArgListAST();
};

class VectorConstructorAST : public ExprAST {
  std::auto_ptr<IdentifierExprAST> Type;
  std::auto_ptr<CallArgListAST> ArgList;
public:
  VectorConstructorAST(IdentifierExprAST *type, CallArgListAST *arg_list)
    : Type(type), ArgList(arg_list) {}
  virtual llvm::Value *codegen(ScopeContext *scope);
  virtual nanjit::TypeInfo getResultType(ScopeContext *scope);
  virtual std::ostream& print(std::ostream& os);
};

class CallAST : public ExprAST {
  std::auto_ptr<IdentifierExprAST> Target;
  std::auto_ptr<CallArgListAST> ArgList;
public:
  CallAST(IdentifierExprAST *target, CallArgListAST* args)
    : Target(target), ArgList(args) {}
  virtual llvm::Value *codegen(ScopeContext *scope);
  virtual nanjit::TypeInfo getResultType(ScopeContext *scope);
  virtual std::ostream& print(std::ostream& os);
};

class BlockAST {
  std::list<ExprAST *> Expressions;
public:
  BlockAST(ExprAST *expr);
  void PrependExpr(ExprAST *expr);
  virtual llvm::Value *codegen(ScopeContext *scope);
  virtual std::ostream& print(std::ostream& os);

  virtual ~BlockAST();
};

class ComparisonAST : public ExprAST {
public:
  enum CompareOp {
    EqualTo,
    NotEqualTo,
    GreaterThan,
    GreaterThanOrEqual,
    LessThan,
    LessThanOrEqual
  };
private:
  CompareOp Op;
  std::auto_ptr<ExprAST> LHS, RHS;
public:
  ComparisonAST(CompareOp op, ExprAST *lhs, ExprAST *rhs)
    : Op(op), LHS(lhs), RHS(rhs) {}
  virtual llvm::Value *codegen(ScopeContext *scope);
  virtual nanjit::TypeInfo getResultType(ScopeContext *scope);
  virtual std::ostream& print(std::ostream& os);
};

class IfElseAST : public ExprAST /* FIXME: Not really an expr */ {
  std::auto_ptr<ComparisonAST> Comparison;
  std::auto_ptr<BlockAST> IfBlock;
  std::auto_ptr<BlockAST> ElseBlock;
public:
  IfElseAST(ComparisonAST *comp,
            BlockAST *ifblock,
            BlockAST *elseblock) : Comparison(comp), IfBlock(ifblock), ElseBlock(elseblock) {}
  virtual llvm::Value *codegen(ScopeContext *scope);
  virtual std::ostream& print(std::ostream& os);
};

class FunctionArgAST {
  std::auto_ptr<IdentifierExprAST> Type;
  std::auto_ptr<IdentifierExprAST> Name;
public:
  FunctionArgAST(IdentifierExprAST *type, IdentifierExprAST *name) : Type(type), Name(name) {};
  std::string getType();
  std::string getName();
};

class FunctionArgListAST {
  std::list<FunctionArgAST *> Args;
public:
  FunctionArgListAST(FunctionArgAST *arg);
  void prependArg(FunctionArgAST *arg);
  std::list<FunctionArgAST *> &getArgsList();
  virtual std::ostream& print(std::ostream& os);
  virtual ~FunctionArgListAST();
};

class FunctionAST : public ASTNode {
  std::auto_ptr<IdentifierExprAST> Name;
  std::auto_ptr<IdentifierExprAST> ReturnType;
  std::auto_ptr<FunctionArgListAST> Args;
  std::auto_ptr<BlockAST> Block;
public:
  FunctionAST(IdentifierExprAST *return_type, IdentifierExprAST *name, FunctionArgListAST *args, BlockAST *b) : ReturnType(return_type), Name(name), Args(args), Block(b) {}

  std::string getName();

  virtual llvm::Value *codegen(llvm::Module *module = NULL);
  virtual std::ostream& print(std::ostream& os);

  virtual ~FunctionAST();
};

class ReturnAST : public ExprAST /* FIXME: Not really an expr */ {
  std::auto_ptr<ExprAST> Result;
public:
  ReturnAST(ExprAST *expr = NULL) : Result(expr) {};
  virtual llvm::Value *codegen(ScopeContext *scope);

  virtual std::ostream& print(std::ostream& os);
};

class ModuleAST {
  std::list<FunctionAST *> functions;
public:
  ModuleAST();

  void prependFunction(FunctionAST *func);

  virtual llvm::Module *codegen(llvm::Module *module);
  virtual std::ostream& print(std::ostream& os);

  virtual ~ModuleAST();
};

#endif /* __cplusplus */
#endif /* __AST_HPP__ */
