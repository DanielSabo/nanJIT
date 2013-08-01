#ifndef __TYPEINFO_HPP__
#define __TYPEINFO_HPP__

#include <string>

namespace llvm
{
  class Type;
};

namespace nanjit
{
  class TypeInfo
  {
  public:
    typedef enum {
      TYPE_VOID,
      TYPE_FLOAT,
      TYPE_INT,
      TYPE_UINT,
      TYPE_SHORT,
      TYPE_USHORT,
      TYPE_CHAR,
      TYPE_UCHAR,
      TYPE_BOOL
    } BaseTypeEnum;
  private:
    BaseTypeEnum base_type;
    unsigned int width;
  public:
    TypeInfo(std::string);
    TypeInfo(BaseTypeEnum bt = TYPE_VOID, unsigned int w = 1);

    BaseTypeEnum getBaseType() const;
    unsigned int getWidth() const;
    virtual llvm::Type *getLLVMType() const;
    virtual std::string toStr() const;

    virtual bool isFloatType() const;
    virtual bool isIntegerType() const;
    virtual bool isUnsignedType() const;

    bool operator == (const TypeInfo &rhs) const;
    bool operator != (const TypeInfo &rhs) const;
  };
}

#endif /* __TYPEINFO_HPP__ */