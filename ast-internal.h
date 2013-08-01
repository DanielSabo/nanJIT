class ScopeContext {
  std::map<std::string, llvm::Value *> Variables;
  std::map<std::string, nanjit::TypeInfo> Types;
  nanjit::TypeInfo ReturnType;
public:
  IRBuilder<> *Builder;

  ScopeContext() : ReturnType("void") {};

  void setVariable(std::string name, llvm::Value *value);
  void setVariable(nanjit::TypeInfo, std::string name, llvm::Value *value);
  llvm::Value *getVariable(std::string name);
  nanjit::TypeInfo getTypeInfo(std::string name);

  void setReturnType(nanjit::TypeInfo rt) { ReturnType = rt; };
  nanjit::TypeInfo getReturnType() { return ReturnType; };
};

llvm::Type *typeinfo_get_llvm_type(nanjit::TypeInfo type);
void cast_value(ScopeContext *scope, nanjit::TypeInfo to_type, nanjit::TypeInfo from_type, Value **value);