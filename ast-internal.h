class ScopeContext {
  std::map<std::string, llvm::Value *> variables;
  std::map<std::string, nanjit::TypeInfo> types;
  nanjit::TypeInfo return_type;
  ScopeContext *parent;
public:
  llvm::IRBuilder<> *Builder;
  llvm::Module *Module;

  ScopeContext() : return_type("void"), parent(NULL) {};

  void setVariable(std::string name, llvm::Value *value);
  void setVariable(nanjit::TypeInfo, std::string name, llvm::Value *value);
  llvm::Value *getVariable(std::string name);
  nanjit::TypeInfo getTypeInfo(std::string name);

  void setReturnType(nanjit::TypeInfo rt) { return_type = rt; };
  nanjit::TypeInfo getReturnType() { return return_type; };

  ScopeContext *createChild();
};

llvm::Type *typeinfo_get_llvm_type(nanjit::TypeInfo type);
void cast_value(ScopeContext *scope, nanjit::TypeInfo to_type, nanjit::TypeInfo from_type, Value **value);
Function *define_llvm_intrinisic(ScopeContext *scope, string name, nanjit::TypeInfo type);
