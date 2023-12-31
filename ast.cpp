#include "llvm/Config/config.h"

#include "llvm/Analysis/Passes.h"
#include "llvm/Analysis/Verifier.h"
#include "llvm/CodeGen/MachineCodeInfo.h"
#include "llvm/ExecutionEngine/ExecutionEngine.h"
#include "llvm/ExecutionEngine/JIT.h"
#if ((LLVM_VERSION_MAJOR > 3) || ((LLVM_VERSION_MAJOR == 3) && (LLVM_VERSION_MINOR >= 3)))
#include "llvm/IR/DataLayout.h"
#include "llvm/IR/DerivedTypes.h"
#include "llvm/IR/IRBuilder.h"
#include "llvm/IR/LLVMContext.h"
#include "llvm/IR/Module.h"
#else
#include "llvm/DataLayout.h"
#include "llvm/DerivedTypes.h"
#include "llvm/IRBuilder.h"
#include "llvm/LLVMContext.h"
#include "llvm/Module.h"
#endif
#include "llvm/PassManager.h"
#include "llvm/Support/TargetSelect.h"
#include "llvm/Support/Host.h"
#include "llvm/Support/TargetRegistry.h"
#include "llvm/Transforms/Scalar.h"
#include <cstdio>
#include <map>
#include <string>
#include <vector>
#include <iostream>
#include <sstream>
#include <llvm/Support/raw_ostream.h>
using namespace llvm;
using namespace std;

#include "typeinfo.h"
#include "ast.h"
#include "ast-internal.h"
using namespace nanjit;

/* Base types, from lowest to highest precedence */
static const TypeInfo::BaseTypeEnum BASE_TYPES[] = {
  TypeInfo::TYPE_USHORT,
  TypeInfo::TYPE_SHORT,
  TypeInfo::TYPE_UINT,
  TypeInfo::TYPE_INT,
  TypeInfo::TYPE_FLOAT,
  TypeInfo::TYPE_VOID
};

llvm::Type *typeinfo_get_llvm_type(nanjit::TypeInfo type)
{
  if (type.getBaseType() == TypeInfo::TYPE_VOID)
    throw SyntaxErrorException("Could not get LLVM type for " + type.toStr());

  Type *llvm_type = type.getLLVMType();

  if (!llvm_type)
    throw SyntaxErrorException("Could not get LLVM type for " + type.toStr());

  return llvm_type;
}

void cast_value(ScopeContext *scope, nanjit::TypeInfo to_type, nanjit::TypeInfo from_type, Value **value)
{
  IRBuilder<> *Builder = scope->Builder;

  if (to_type.getWidth() != from_type.getWidth())
    throw SyntaxErrorException("Could not convert " + from_type.toStr() + " to " + to_type.toStr() + ", types differ in width");

  TypeInfo::BaseTypeEnum to_type_base   = to_type.getBaseType();
  TypeInfo::BaseTypeEnum from_type_base = from_type.getBaseType();

  if (to_type == from_type)
    {
      return;
    }
  else if ((to_type_base == TypeInfo::TYPE_UINT && from_type_base == TypeInfo::TYPE_INT) ||
           (to_type_base == TypeInfo::TYPE_INT  && from_type_base == TypeInfo::TYPE_UINT))
    {
      return;
    }
  else if ((to_type_base == TypeInfo::TYPE_UINT && from_type_base == TypeInfo::TYPE_USHORT) ||
           (to_type_base == TypeInfo::TYPE_INT  && from_type_base == TypeInfo::TYPE_USHORT) ||
           (to_type_base == TypeInfo::TYPE_UINT && from_type_base == TypeInfo::TYPE_SHORT))
    {
      *value = Builder->CreateZExt(*value, typeinfo_get_llvm_type(to_type));
    }
  else if ((to_type_base == TypeInfo::TYPE_USHORT && from_type_base == TypeInfo::TYPE_UINT) ||
           (to_type_base == TypeInfo::TYPE_SHORT  && from_type_base == TypeInfo::TYPE_UINT) ||
           (to_type_base == TypeInfo::TYPE_USHORT && from_type_base == TypeInfo::TYPE_INT))
    {
      *value = Builder->CreateTrunc(*value, typeinfo_get_llvm_type(to_type));
    }
  else if ((to_type_base == TypeInfo::TYPE_INT && from_type_base == TypeInfo::TYPE_SHORT))
    {
      *value = Builder->CreateSExt(*value, typeinfo_get_llvm_type(to_type));
    }
  else if ((to_type_base == TypeInfo::TYPE_SHORT && from_type_base == TypeInfo::TYPE_INT))
    {
      *value = Builder->CreateTrunc(*value, typeinfo_get_llvm_type(to_type));
    }
  else if ((to_type_base == TypeInfo::TYPE_FLOAT && from_type_base == TypeInfo::TYPE_INT) ||
           (to_type_base == TypeInfo::TYPE_FLOAT && from_type_base == TypeInfo::TYPE_SHORT))
    {
      *value = Builder->CreateSIToFP(*value, typeinfo_get_llvm_type(to_type));
    }
  else if ((to_type_base == TypeInfo::TYPE_FLOAT && from_type_base == TypeInfo::TYPE_UINT) ||
           (to_type_base == TypeInfo::TYPE_FLOAT && from_type_base == TypeInfo::TYPE_USHORT))
    {
      *value = Builder->CreateUIToFP(*value, typeinfo_get_llvm_type(to_type));
    }
  else if ((to_type_base == TypeInfo::TYPE_INT   && from_type_base == TypeInfo::TYPE_FLOAT) ||
           (to_type_base == TypeInfo::TYPE_SHORT && from_type_base == TypeInfo::TYPE_FLOAT))
    {
      *value = Builder->CreateFPToSI(*value, typeinfo_get_llvm_type(to_type));
    }
  else if ((to_type_base == TypeInfo::TYPE_UINT   && from_type_base == TypeInfo::TYPE_FLOAT) ||
           (to_type_base == TypeInfo::TYPE_USHORT && from_type_base == TypeInfo::TYPE_FLOAT))
    {
      *value = Builder->CreateFPToUI(*value, typeinfo_get_llvm_type(to_type));
    }
  else
    throw SyntaxErrorException("Could not convert " + from_type.toStr() + " to " + to_type.toStr());
}

Function *define_llvm_intrinisic(ScopeContext *scope, string name, TypeInfo type)
{
  if (!type.isFloatType())
    throw SyntaxErrorException("Intrinsic \"" + name + "\" type must be a float");

  std::stringstream full_name;
  if (type.getWidth() != 1)
    full_name << name << ".v" << type.getWidth() << "f32";
  else
    full_name << name << ".f32";

  Function *func = scope->Module->getFunction(full_name.str());

  if (!func)
  {
    Type *result_type = type.getLLVMType();
    vector<Type *> arg_types;

    arg_types.push_back(result_type);

    /* create the function type */
    FunctionType *func_type = FunctionType::get(result_type, arg_types, false);
    func = Function::Create(func_type, Function::ExternalLinkage, full_name.str(), scope->Module);
  }

  return func;
}

void ScopeContext::setVariable(std::string name, llvm::Value *value)
{
  /* Set an existing value in this contex's or a parent's scope */
  std::map<std::string, llvm::Value *>::iterator it;
  it = variables.find(name);
  
  if (it == variables.end())
    {
      if (parent)
        parent->setVariable(name, value);
      else
        throw SyntaxErrorException("Assignment to undefined variable \"" + name + "\"");
    }
  else
    {
      Builder->CreateStore(value, it->second);
    }
}

void ScopeContext::setVariable(TypeInfo type, std::string name, llvm::Value *value)
{
  /* When a type is given, create a new variable in this contex's scope */
  std::map<std::string, llvm::Value *>::iterator it;
  it = variables.find(name);
  
  if (it == variables.end())
    {
      Function *parent_function = Builder->GetInsertBlock()->getParent();
      IRBuilder<> temp_builder(&parent_function->getEntryBlock(), parent_function->getEntryBlock().begin());
      variables[name] = temp_builder.CreateAlloca(value->getType());
      types[name] = type;
      Builder->CreateStore(value, variables[name]);
    }
  else
    {
      throw SyntaxErrorException("Redefinition of variable \"" + name + "\"");
    }
}

Value *ScopeContext::getVariable(std::string name)
{
  std::map<std::string, llvm::Value *>::iterator it;
  
  it = variables.find(name);
  if (it != variables.end())
    return Builder->CreateLoad(it->second, name);
  else if (parent)
    return parent->getVariable(name);
    
  throw SyntaxErrorException ("Unknown variable: " + name);
}

nanjit::TypeInfo ScopeContext::getTypeInfo(std::string name)
{
  std::map<std::string, nanjit::TypeInfo>::iterator it;
  
  it = types.find(name);
  if (it != types.end())
    return it->second;
  else if (parent)
    return parent->getTypeInfo(name);
    
  throw SyntaxErrorException ("Unknown variable: " + name);
}

ScopeContext *ScopeContext::createChild()
{
  ScopeContext *scope = new ScopeContext();
  scope->Builder = Builder;
  scope->Module = Module;
  scope->return_type = return_type;
  scope->parent = this;
  return scope;
}

ASTNode::ASTNode()
{
  line_number = -1;
  column_number = -1;
}

void ASTNode::setLocation(int line, int col)
{
  line_number = line;
  /* FIXME: Bison considers the first column to be 0 */
  column_number = col + 1;
}

SyntaxErrorException ASTNode::genSyntaxError(std::string e)
{
  std::stringstream error_string;
  error_string << "(" << line_number << ":" << column_number << "): " << e;
  return SyntaxErrorException(error_string.str());
}

Value *ExprAST::codegen(ScopeContext *scope)
{
  throw SyntaxErrorException ("Codegen not implemented");
}

nanjit::TypeInfo ExprAST::getResultType(ScopeContext *scope)
{
  throw SyntaxErrorException("getResultType not implemented");
}

ostream& ExprAST::print(ostream& os)
{
  return os << "?" << this << "?";
}

DoubleExprAST::DoubleExprAST(std::string val)
{
  str_value = val;
}

Value *DoubleExprAST::codegen(ScopeContext *scope)
{
  float v = atof(str_value.c_str());
  cout << "Warning: Double value " << str_value << " will be truncated to float" << endl;
  return ConstantFP::get(getGlobalContext(), APFloat(v));
}

nanjit::TypeInfo DoubleExprAST::getResultType(ScopeContext *scope)
{
  return nanjit::TypeInfo(TypeInfo::TYPE_FLOAT);
}

ostream& DoubleExprAST::print(ostream& os)
{
  return os << str_value;
}

FloatExprAST::FloatExprAST(std::string val)
{
  str_value = val;
}

Value *FloatExprAST::codegen(ScopeContext *scope)
{
  float v = atof(str_value.c_str());
  return ConstantFP::get(getGlobalContext(), APFloat(v));
}

nanjit::TypeInfo FloatExprAST::getResultType(ScopeContext *scope)
{
  return nanjit::TypeInfo(TypeInfo::TYPE_FLOAT);
}

ostream& FloatExprAST::print(ostream& os)
{
  return os << str_value;
}

IntExprAST::IntExprAST(std::string val)
{
  str_value = val;
}

Value *IntExprAST::codegen(ScopeContext *scope)
{
  double dval = atof(str_value.c_str());
  if (dval <= numeric_limits<int32_t>::max())
    {
      if (dval < numeric_limits<int32_t>::min())
        cout << "Warning: Integer underflow, " << str_value << " can not be represented by a 32-bit value" << endl;

      int32_t ival = dval;
      return scope->Builder->getInt32(ival);
    }
  else
    {
      if (dval > numeric_limits<uint32_t>::max())
        cout << "Warning: Integer overflow, " << str_value << " can not be represented by a 32-bit value" << endl;

      uint32_t ival = dval;
      return scope->Builder->getInt32(ival);
    }
}

nanjit::TypeInfo IntExprAST::getResultType(ScopeContext *scope)
{
  double dval = atof(str_value.c_str());
  if (dval <= numeric_limits<int32_t>::max())
    return nanjit::TypeInfo(TypeInfo::TYPE_INT);
  return nanjit::TypeInfo(TypeInfo::TYPE_UINT);
}

ostream& IntExprAST::print(ostream& os)
{
  return os << str_value;
}

Value *IdentifierExprAST::codegen(ScopeContext *scope)
{
  return scope->getVariable(Name);
}

nanjit::TypeInfo IdentifierExprAST::getResultType(ScopeContext *scope)
{
  return nanjit::TypeInfo(scope->getTypeInfo(Name));
}

ostream& IdentifierExprAST::print(ostream& os)
{
  return os << "Identifier(" << Name << ")";
}

TypeInfo promote_types(const TypeInfo &lhs_type, const TypeInfo &rhs_type)
{
  /* From C99 TC3, Section 6.3.1.1 "Conversions", paragraph 2
   * The following may be used in an expression wherever an int or
   * unsigned int may be used:
   * - An object or expression with an integer type whose integer conversion
   *   rank is less than or equal to the rank of int and unsigned int.
   * — A bit-field of type _Bool, int, signed int, or unsigned int.
   * If an int can represent all values of the original type, the value is
   * converted to an int; otherwise, it is converted to an unsigned int.
   */

  TypeInfo promoted_type;

  unsigned int type_width = lhs_type.getWidth();
  if (lhs_type.isFloatType() || rhs_type.isFloatType())
    {
      promoted_type = TypeInfo(TypeInfo::TYPE_FLOAT, type_width);
    }
  else if (lhs_type.getBaseType() == TypeInfo::TYPE_UINT ||
           lhs_type.getBaseType() == TypeInfo::TYPE_UINT)
    {
      promoted_type = TypeInfo(TypeInfo::TYPE_UINT, type_width);
    }
  else
    {
      promoted_type = TypeInfo(TypeInfo::TYPE_INT, type_width);
    }

  return promoted_type;
}

Value *BinaryExprAST::codegen(ScopeContext *scope)
{
  IRBuilder<> *Builder = scope->Builder;
  Value *lhs = LHS->codegen(scope);
  Value *rhs = RHS->codegen(scope);

  TypeInfo lhs_type = LHS->getResultType(scope);
  TypeInfo rhs_type = RHS->getResultType(scope);
  TypeInfo type_for_operation = promote_types(lhs_type, rhs_type);

  cast_value(scope, type_for_operation, lhs_type, &lhs);
  cast_value(scope, type_for_operation, rhs_type, &rhs);

  if (lhs->getType() != rhs->getType())
    {
      std::string error;
      llvm::raw_string_ostream rso(error);
      rso << "Type mismatch for BinaryExprAST: ";
      lhs->getType()->print(rso);
      rso << " " << Op << " ";
      rhs->getType()->print(rso);
      throw SyntaxErrorException(rso.str());
    }
  
  if (type_for_operation.isFloatType())
    {
      if (Op == '+')
        return Builder->CreateFAdd(lhs, rhs);
      else if (Op == '-')
        return Builder->CreateFSub(lhs, rhs);
      else if (Op == '*')
        return Builder->CreateFMul(lhs, rhs);
      else if (Op == '/')
        return Builder->CreateFDiv(lhs, rhs);
      else
        throw SyntaxErrorException ("Invalid operator for BinaryExprAST");
    }
  else if (type_for_operation.isIntegerType())
    {
      if (Op == '+')
        return Builder->CreateAdd(lhs, rhs);
      else if (Op == '-')
        return Builder->CreateSub(lhs, rhs);
      else if (Op == '*')
        return Builder->CreateMul(lhs, rhs);
      else if (Op == '/')
        {
          if (type_for_operation.isUnsignedType())
            return Builder->CreateUDiv(lhs, rhs);
          else
            return Builder->CreateSDiv(lhs, rhs);
        }
      else
        throw SyntaxErrorException ("Invalid operator for BinaryExprAST");
    }
  else
    {
      throw SyntaxErrorException ("Invalid type for BinaryExprAST" + type_for_operation.toStr());
    }

}

nanjit::TypeInfo BinaryExprAST::getResultType(ScopeContext *scope)
{
  return promote_types(LHS->getResultType(scope), RHS->getResultType(scope));
}

ostream& BinaryExprAST::print(ostream& os)
{
  os << "BinaryExprAST(" << Op << " ";
  LHS->print(os);
  os << " ";
  RHS->print(os);
  os << ")";
  return os;
}

Value *AssignmentExprAST::codegen(ScopeContext *scope)
{
  IRBuilder<> *Builder = scope->Builder;

  std::string name = LHS->getName();
  TypeInfo lhs_type(TypeInfo::TYPE_VOID);
  TypeInfo rhs_type = RHS->getResultType(scope);

  if (LHSType.get())
    lhs_type = TypeInfo(LHSType->getName());
  else
    lhs_type = scope->getTypeInfo(name);

  Value *result = RHS->codegen(scope);
  cast_value(scope, lhs_type, rhs_type, &result);

  if (LHSType.get())
    scope->setVariable(lhs_type, name, result);
  else
    scope->setVariable(name, result);

  return result;
}

ostream& AssignmentExprAST::print(ostream& os)
{
  os << "AssignmentExprAST(= ";
  LHS->print(os);
  os << " ";
  RHS->print(os);
  os << ")";
  return os;
}

Value *ShuffleSelfAST::codegen(ScopeContext *scope)
{
  IRBuilder<> *Builder = scope->Builder;
  Value *lhs = Target->codegen(scope);
  VectorType *lhs_type = dyn_cast<VectorType>(lhs->getType());

  if (!lhs_type)
    throw genSyntaxError("LHS of ShuffleSelfAST is not a vector");
  
  std::string access = Access->getName();
  if (access[0] == 's' && access.size() == 5)
    {
      int a, b, c, d;
      int max_index = lhs_type->getNumElements() - 1;
      if (4 != sscanf(access.c_str(), "s%1d%1d%1d%1d", &a, &b, &c, &d))
        throw genSyntaxError("ShuffleSelfAST index invalid");
      if (a > max_index || b > max_index || c > max_index || d > max_index)
        throw genSyntaxError("ShuffleSelfAST index out of range");
      
      Value *undef = UndefValue::get(lhs->getType());
      Value *mask  = UndefValue::get(VectorType::get(Builder->getInt32Ty(), 4));
      mask = Builder->CreateInsertElement(mask, Builder->getInt32(a), Builder->getInt32(0));
      mask = Builder->CreateInsertElement(mask, Builder->getInt32(b), Builder->getInt32(1));
      mask = Builder->CreateInsertElement(mask, Builder->getInt32(c), Builder->getInt32(2));
      mask = Builder->CreateInsertElement(mask, Builder->getInt32(d), Builder->getInt32(3));
      return Builder->CreateShuffleVector(lhs, undef, mask);
      throw genSyntaxError("ShuffleSelfAST");
    }
  else if (access[0] == 's' && access.size() == 2)
    {
      int a;
      int max_index = lhs_type->getNumElements() - 1;
      if (1 != sscanf(access.c_str(), "s%1d", &a))
        throw genSyntaxError("ShuffleSelfAST index invalid");
      if (a > max_index)
        throw genSyntaxError("ShuffleSelfAST index out of range");
      return Builder->CreateExtractElement(lhs, Builder->getInt32(a));
    }
  throw genSyntaxError(std::string("Invalid indexes for ShuffleSelfAST: ") + access);
}

nanjit::TypeInfo ShuffleSelfAST::getResultType(ScopeContext *scope)
{
  /* FIXME: Validate */

  TypeInfo in_type = Target->getResultType(scope);
  int width = Access->getName().size() - 1;

  return nanjit::TypeInfo(in_type.getBaseType(), width);
}

ostream& ShuffleSelfAST::print(ostream& os)
{
  os << "Shuffle(";
  Target->print(os);
  os << " ";
  Access->print(os);
  os << ")";
  return os;
}

CallArgListAST::CallArgListAST()
{
}

CallArgListAST::CallArgListAST(ExprAST *expr)
{
  Args.insert(Args.begin(), expr);
}

void CallArgListAST::prependArg(ExprAST *expr)
{
  Args.insert(Args.begin(), expr);
}

CallArgListAST::~CallArgListAST()
{
  for (std::vector<ExprAST *>::iterator it=Args.begin(); it!=Args.end(); ++it)
    {
      delete (*it);
    }
}

ostream& CallArgListAST::print(ostream& os)
{
  os << "(";
  bool first = true;
  for (std::vector<ExprAST *>::iterator it=Args.begin(); it!=Args.end(); ++it)
    {
      if (!first)
        os << ", ";
      else
        first = false;

      (*it)->print(os);
    }
  os << ")";
  return os;
}

Value *VectorConstructorAST::codegen(ScopeContext *scope)
{
  IRBuilder<> *Builder = scope->Builder;

  TypeInfo vector_type = TypeInfo(Type->getName());
  std::vector<ExprAST *> &args = ArgList->getArgsList();

  int width = vector_type.getWidth();

  if (width < 2)
    throw genSyntaxError(std::string("Invalid vector type: ") + Type->getName());

  if (args.size() != width)
    throw genSyntaxError(std::string("Invalid arguments to vector constructor for ") + Type->getName());

  Value *out_value = UndefValue::get(typeinfo_get_llvm_type(vector_type));
  TypeInfo element_type = TypeInfo(vector_type.getBaseType());

  for (int element_index = 0; element_index < width; ++element_index)
    {
      ExprAST *elmement_ast = args[element_index];
      Value *elmement_value = elmement_ast->codegen(scope);
      cast_value(scope, element_type, elmement_ast->getResultType(scope), &elmement_value);
      out_value = Builder->CreateInsertElement(out_value, elmement_value, Builder->getInt32(element_index));
    }

  return out_value;
}

nanjit::TypeInfo VectorConstructorAST::getResultType(ScopeContext *scope)
{
  TypeInfo type = TypeInfo(Type->getName());

  if (type.getWidth() < 2)
    throw genSyntaxError(std::string("Invalid vector type: ") + Type->getName());

  return type;
}

ostream& VectorConstructorAST::print(ostream& os)
{
  os << "VectorConstructorAST(";
  Type->print(os);
  os << " ";
  ArgList->print(os);
  os << ")";
  return os;
}

Value *CallAST::codegen(ScopeContext *scope)
{
  std::vector<ExprAST *> &args = ArgList->getArgsList();

  if (std::string("shuffle2") == Target->getName())
    {
      const int num_args = 3;
      if (args.size() != num_args)
        {
          std::stringstream error_string;
          error_string << "Called \"" << Target->getName() << "\" with ";
          error_string << args.size() << " arguments, expected " << num_args;
        throw SyntaxErrorException(error_string.str());
        }
      IRBuilder<> *Builder = scope->Builder;
      ExprAST *vecA = args[0];
      ExprAST *vecB = args[1];
      std::auto_ptr<Value> mask(args[2]->codegen(scope));

      /* FIXME: Validate mask size and type */
      return Builder->CreateShuffleVector(vecA->codegen(scope), vecB->codegen(scope), mask.release());
    }
  else if (std::string("select") == Target->getName())
    {
      const int num_args = 3;
      if (args.size() != num_args)
        {
          std::stringstream error_string;
          error_string << "Called \"" << Target->getName() << "\" with ";
          error_string << args.size() << " arguments, expected " << num_args;
        throw SyntaxErrorException(error_string.str());
        }
      IRBuilder<> *Builder = scope->Builder;
      ExprAST *vecA = args[0];
      ExprAST *vecB = args[1];
      ExprAST *cmp  = args[2];
      /* FIXME: Validate arguments */
      return Builder->CreateSelect(cmp->codegen(scope), vecA->codegen(scope), vecB->codegen(scope));
    }
  else if (std::string("clamp") == Target->getName())
    {
      const int num_args = 3;
      if (args.size() != num_args)
        {
          std::stringstream error_string;
          error_string << "Called \"" << Target->getName() << "\" with ";
          error_string << args.size() << " arguments, expected " << num_args;
        throw SyntaxErrorException(error_string.str());
        }

      IRBuilder<> *Builder = scope->Builder;

      Value *value = args[0]->codegen(scope);
      Value *min   = args[1]->codegen(scope);
      Value *max   = args[2]->codegen(scope);

      TypeInfo value_type = args[0]->getResultType(scope);
      TypeInfo min_type   = args[1]->getResultType(scope);
      TypeInfo max_type   = args[2]->getResultType(scope);

      if ((value_type.getWidth() != min_type.getWidth()) ||
          (min_type.getWidth()   != max_type.getWidth()))
        {
          std::string error;
          llvm::raw_string_ostream rso(error);
          rso << "Type mismatch for " << Target->getName() << " : ";
          rso << value_type.toStr();
          rso << " vs ";
          rso << min_type.toStr();
          rso << " vs ";
          rso << max_type.toStr();
          throw SyntaxErrorException(rso.str());
        }

      TypeInfo target_type = TypeInfo(TypeInfo::TYPE_FLOAT, value_type.getWidth());
      cast_value(scope, target_type, value_type, &value);
      cast_value(scope, target_type, min_type, &min);
      cast_value(scope, target_type, max_type, &max);

      Value *mask;
      mask = Builder->CreateFCmpUGE(value, min);
      value = Builder->CreateSelect(mask, value, min);
      mask = Builder->CreateFCmpULE(value, max);
      value = Builder->CreateSelect(mask, value, max);
      return value;
    }
  else if ((std::string("min") == Target->getName()) || (std::string("max") == Target->getName()))
    {
      const int num_args = 2;
      if (args.size() != num_args)
        {
          std::stringstream error_string;
          error_string << "Called \"" << Target->getName() << "\" with ";
          error_string << args.size() << " arguments, expected " << num_args;
        throw SyntaxErrorException(error_string.str());
        }

      IRBuilder<> *Builder = scope->Builder;

      Value *value_a = args[0]->codegen(scope);
      Value *value_b = args[1]->codegen(scope);

      TypeInfo a_type = args[0]->getResultType(scope);
      TypeInfo b_type = args[1]->getResultType(scope);

      if (a_type.getWidth() != b_type.getWidth())
        {
          std::string error;
          llvm::raw_string_ostream rso(error);
          rso << "Type mismatch for " << Target->getName() << " : ";
          rso << a_type.toStr();
          rso << " vs ";
          rso << b_type.toStr();
          throw SyntaxErrorException(rso.str());
        }

      TypeInfo target_type = TypeInfo(TypeInfo::TYPE_FLOAT, a_type.getWidth());
      cast_value(scope, target_type, a_type, &value_a);
      cast_value(scope, target_type, b_type, &value_b);

      Value *mask = NULL;
      if (std::string("min") == Target->getName())
        mask = Builder->CreateFCmpULE(value_a, value_b);
      else
        mask = Builder->CreateFCmpUGE(value_a, value_b);
      Value *value = Builder->CreateSelect(mask, value_a, value_b);
      return value;
    }
  else if (std::string("sqrt") == Target->getName())
    {
      const int num_args = 1;
      if (args.size() != num_args)
        {
          std::stringstream error_string;
          error_string << "Called \"" << Target->getName() << "\" with ";
          error_string << args.size() << " arguments, expected " << num_args;
        throw SyntaxErrorException(error_string.str());
        }

      IRBuilder<> *Builder = scope->Builder;

      Value *arg_value   = args[0]->codegen(scope);
      TypeInfo arg_type  = args[0]->getResultType(scope);
      TypeInfo call_type = TypeInfo(TypeInfo::TYPE_FLOAT, arg_type.getWidth());
      cast_value(scope, call_type, arg_type, &arg_value);

      vector<Value *>call_parameters;
      call_parameters.push_back(arg_value);

      Function *target_func = define_llvm_intrinisic(scope, "llvm.sqrt", call_type);

      Value *call_result = Builder->CreateCall(target_func, call_parameters);

      return call_result;
    }

  throw SyntaxErrorException("Call \"" + Target->getName() + "\" not implemented");
}

nanjit::TypeInfo CallAST::getResultType(ScopeContext *scope)
{
  if (std::string("shuffle2") == Target->getName())
    {
      return ArgList->getArgsList()[0]->getResultType(scope);
    }
  else if (std::string("select") == Target->getName())
    {
      return ArgList->getArgsList()[0]->getResultType(scope);
    }
  else if (std::string("clamp") == Target->getName())
    {
      TypeInfo arg_type = ArgList->getArgsList()[0]->getResultType(scope);
      return TypeInfo(TypeInfo::TYPE_FLOAT, arg_type.getWidth());
    }
  else if ((std::string("min") == Target->getName()) ||
           (std::string("max") == Target->getName()))
    {
      TypeInfo arg_type = ArgList->getArgsList()[0]->getResultType(scope);
      return TypeInfo(TypeInfo::TYPE_FLOAT, arg_type.getWidth());
    }
  else if (std::string("sqrt") == Target->getName())
    {
      TypeInfo arg_type = ArgList->getArgsList()[0]->getResultType(scope);
      return TypeInfo(TypeInfo::TYPE_FLOAT, arg_type.getWidth());
    }
  throw SyntaxErrorException("Call \"" + Target->getName() + "\" result type not implemented");
}

ostream& CallAST::print(ostream& os)
{
  os << "CallAST( ";
  Target->print(os);
  os << " ";
  ArgList->print(os);
  os << " )";
  return os;
}

BlockAST::BlockAST(ExprAST *expr)
{
  Expressions.push_front(expr);
}

void BlockAST::PrependExpr(ExprAST *expr)
{
  Expressions.push_front(expr);
}

BlockAST::~BlockAST()
{
  for (std::list<ExprAST *>::iterator it=Expressions.begin(); it!=Expressions.end(); ++it)
    {
      delete (*it);
    }
}

Value *BlockAST::codegen(ScopeContext *scope)
{
  Value *last = NULL;

  for (std::list<ExprAST *>::iterator it = Expressions.begin();
       it != Expressions.end();
       ++it)
    {
      last = (*it)->codegen(scope);
    }
  
  return last;
}

ostream& BlockAST::print(ostream& os)
{
  for (std::list<ExprAST *>::iterator it=Expressions.begin(); it!=Expressions.end(); ++it)
    {
      os << "  ";
      (*it)->print(os);
      os << endl;
    }
  return os;
}

Value *ComparisonAST::codegen(ScopeContext *scope)
{
  IRBuilder<> *Builder = scope->Builder;

  if ((LHS->getResultType(scope).getBaseType() != TypeInfo::TYPE_FLOAT) ||
      (RHS->getResultType(scope).getBaseType() != TypeInfo::TYPE_FLOAT))
  {
    throw SyntaxErrorException ("Invalid type for ComparisonAST");
  }

  switch (Op)
    {
      case EqualTo:
        return Builder->CreateFCmpUEQ(LHS->codegen(scope), RHS->codegen(scope));
      case NotEqualTo:
        return Builder->CreateFCmpUNE(LHS->codegen(scope), RHS->codegen(scope));
      case GreaterThan:
        return Builder->CreateFCmpUGT(LHS->codegen(scope), RHS->codegen(scope));
      case GreaterThanOrEqual:
        return Builder->CreateFCmpUGE(LHS->codegen(scope), RHS->codegen(scope));
      case LessThan:
        return Builder->CreateFCmpULT(LHS->codegen(scope), RHS->codegen(scope));
      case LessThanOrEqual:
        return Builder->CreateFCmpULE(LHS->codegen(scope), RHS->codegen(scope));
    }

  throw SyntaxErrorException("Compare op not implemented");
}

nanjit::TypeInfo ComparisonAST::getResultType(ScopeContext *scope)
{
  nanjit::TypeInfo lhs_type = LHS->getResultType(scope);
  return nanjit::TypeInfo(TypeInfo::TYPE_BOOL, lhs_type.getWidth());
}

ostream& ComparisonAST::print(ostream& os)
{
  os << "ComparisonAST( ";
  LHS->print(os);
  switch (Op)
    {
      case EqualTo:
        os << " == ";
        break;
      case NotEqualTo:
        os << " != ";
        break;
      case GreaterThan:
        os << " > ";
        break;
      case GreaterThanOrEqual:
        os << " >= ";
        break;
      case LessThan:
        os << " < ";
        break;
      case LessThanOrEqual:
        os << " <= ";
        break;
    }
  RHS->print(os);
  os << " )";
  return os;
}

Value *IfElseAST::codegen(ScopeContext *scope)
{
  IRBuilder<> *builder = scope->Builder;
  Function *parent_function = builder->GetInsertBlock()->getParent();

  Value *comparison = Comparison->codegen(scope);

  if (ElseBlock.get())
    {
      BasicBlock *if_block = BasicBlock::Create(builder->getContext(), "if", parent_function);
      BasicBlock *else_block = BasicBlock::Create(builder->getContext(), "else", parent_function);
      BasicBlock *merge_block = BasicBlock::Create(builder->getContext(), "ifcont");
      BasicBlock *active_block = NULL;

      builder->CreateCondBr(comparison, if_block, else_block);

      builder->SetInsertPoint(if_block);

      auto_ptr<ScopeContext> child_scope(scope->createChild());
      IfBlock->codegen(child_scope.get());

      /* Use GetInsertBlock() here because IfBlock->codegen may change the active block */
      active_block = builder->GetInsertBlock();
      if (active_block->empty() ||
          !(isa<ReturnInst>(builder->GetInsertBlock()->back())))
      {
        /* Create a merge jump if the block doesn't cause a return */
        builder->CreateBr(merge_block);
      }

      builder->SetInsertPoint(else_block);

      child_scope.reset(scope->createChild());
      ElseBlock->codegen(child_scope.get());

      /* Use GetInsertBlock() here because ElseBlock->codegen may change the active block */
      active_block = builder->GetInsertBlock();
      if (active_block->empty() ||
          !(isa<ReturnInst>(builder->GetInsertBlock()->back())))
      {
        /* Create a merge jump if the block doesn't cause a return */
        builder->CreateBr(merge_block);
      }

      parent_function->getBasicBlockList().push_back(merge_block);
      builder->SetInsertPoint(merge_block);
    }
  else
    {
      BasicBlock *if_block = BasicBlock::Create(builder->getContext(), "if", parent_function);
      BasicBlock *merge_block = BasicBlock::Create(builder->getContext(), "ifcont");
      builder->CreateCondBr(comparison, if_block, merge_block);

      builder->SetInsertPoint(if_block);

      auto_ptr<ScopeContext> child_scope(scope->createChild());
      IfBlock->codegen(child_scope.get());

      /* Use GetInsertBlock() here because IfBlock->codegen may change the active block */
      BasicBlock *active_block = builder->GetInsertBlock();
      if (active_block->empty() ||
          !(isa<ReturnInst>(builder->GetInsertBlock()->back())))
      {
        /* Create a merge jump if the block doesn't cause a return */
        builder->CreateBr(merge_block);
      }

      parent_function->getBasicBlockList().push_back(merge_block);
      builder->SetInsertPoint(merge_block);
    }

  return NULL;
}

ostream& IfElseAST::print(ostream& os)
{
  os << "If( ";
  Comparison->print(os);
  os << " ) {" << endl;
  IfBlock->print(os);
  if (!ElseBlock.get())
    {
      os << "  }";
    }
  else
    {
      os << "  } Else { ";
      ElseBlock->print(os);
      os << "  }";
    }
  return os;
}

std::string FunctionArgAST::getType()
{
  return Type->getName();
}

std::string FunctionArgAST::getName()
{
  return Name->getName();
}

FunctionArgListAST::FunctionArgListAST(FunctionArgAST *expr)
{
  Args.push_front(expr);
}

std::list<FunctionArgAST *> &FunctionArgListAST::getArgsList()
{
  return Args;
}

void FunctionArgListAST::prependArg(FunctionArgAST *expr)
{
  Args.push_front(expr);
}

ostream& FunctionArgListAST::print(ostream& os)
{
  os << "(";
  bool first = true;
  for (std::list<FunctionArgAST *>::iterator it=Args.begin(); it!=Args.end(); ++it)
    {
      if (!first)
        os << ", ";
      else
        first = false;

      os << (*it)->getType() << " " << (*it)->getName();
    }
  os << ")";
  return os;
}

FunctionArgListAST::~FunctionArgListAST()
{
  for (std::list<FunctionArgAST *>::iterator it=Args.begin(); it!=Args.end(); ++it)
    {
      delete (*it);
    }
}

std::string FunctionAST::getName()
{
  return Name->getName();
}

Value *FunctionAST::codegen(llvm::Module *module)
{
  IRBuilder<> Builder(getGlobalContext());
  ScopeContext scope = ScopeContext();
  scope.Builder = &Builder;
  scope.Module = module;

  scope.setReturnType(ReturnType->getName());

  /* generate the arguments (all pointers to float4 for now) */
  std::list<FunctionArgAST *> &args = Args->getArgsList();
  std::vector<Type*> ArgTypes(args.size());

  Type *result_type = typeinfo_get_llvm_type(ReturnType->getName());
  
  {
    std::list<FunctionArgAST *>::iterator args_iter;
    std::vector<Type*>::iterator types_iter;
    
    for (args_iter = args.begin(), types_iter = ArgTypes.begin();
         args_iter != args.end();
         ++args_iter, ++types_iter)
      {
        *types_iter = typeinfo_get_llvm_type((*args_iter)->getType());
      }
  }

  /* create the function type */
  FunctionType *func_type = FunctionType::get(result_type, ArgTypes, false);
  
  Function *func = Function::Create(func_type, Function::ExternalLinkage, Name->getName(), module);
  
  {
    std::list<FunctionArgAST *>::iterator args_iter;
    Function::arg_iterator func_arg_iter;
    
    for (args_iter = args.begin(), func_arg_iter = func->arg_begin();
         args_iter != args.end();
         ++args_iter, ++func_arg_iter)
      {
        func_arg_iter->setName((*args_iter)->getName());
      }
  }

  BasicBlock *func_body_block = BasicBlock::Create(getGlobalContext(), "entry", func);
  Builder.SetInsertPoint(func_body_block);
  
  /* load the arguments into the scope */
  {
    std::list<FunctionArgAST *>::iterator args_iter;
    Function::arg_iterator func_arg_iter;
    
    for (args_iter = args.begin(), func_arg_iter = func->arg_begin();
         args_iter != args.end();
         ++args_iter, ++func_arg_iter)
      {
        std::string arg_name = (*args_iter)->getName();
        std::string arg_type = (*args_iter)->getType();

        scope.setVariable(arg_type, arg_name, func_arg_iter);
      }
  }
  
  Block->codegen(&scope);
  
  /* return zero if the block was missing a return */
  if (!(isa<ReturnInst>(func->back().back())))
    Builder.CreateRet(ConstantAggregateZero::get(result_type));
  return func;
}

ostream& FunctionAST::print(ostream& os)
{
  os << "Function ";
  Args->print(os);
  os << " --> " << ReturnType->getName();
  os << " {" << endl;
  Block->print(os);
  os << "}" << endl;
  return os;
}

FunctionAST::~FunctionAST()
{
  return;
}


Value *ReturnAST::codegen(ScopeContext *scope)
{
  IRBuilder<> *Builder = scope->Builder;

  if (!Result.get())
    {
      throw SyntaxErrorException("Function must return a value");
    }

  Value *result = Result.get()->codegen(scope);
  cast_value(scope, scope->getReturnType(), Result->getResultType(scope), &result);

  return Builder->CreateRet(result);
}

ostream& ReturnAST::print(ostream& os)
{
  if (Result.get())
    {
      os << "Return( ";
      Result->print(os);
      os << " )";
    }
  else
    {
      os << "Return()";
    }
  return os;
}

ModuleAST::ModuleAST()
{
  return;
}

void ModuleAST::prependFunction(FunctionAST *func)
{
  functions.push_front(func);
}

llvm::Module *ModuleAST::codegen(llvm::Module *module = NULL)
{
  if (!module)
    module = new Module("Unnamed Module", getGlobalContext());

  for (std::list<FunctionAST *>::iterator it = functions.begin(); it != functions.end(); ++it)
    {
      if (module->getFunction((*it)->getName()))
        throw SyntaxErrorException("Redefinition of function \"" + (*it)->getName() + "\"");
      (*it)->codegen(module);
    }

  return module;
}

std::ostream& ModuleAST::print(std::ostream& os)
{
  for (std::list<FunctionAST *>::iterator it = functions.begin(); it != functions.end(); ++it)
    {
      (*it)->print(os);
      os << endl;
    }
  return os;
}

ModuleAST::~ModuleAST()
{
  for (std::list<FunctionAST *>::iterator it = functions.begin(); it != functions.end(); ++it)
    {
      delete (*it);
    }
}
