#ifndef _COMPILER_COMPILER_H
#define _COMPILER_COMPILER_H
#include "obj_fn.h"

#define MAX_LOCAL_VAR_NUM 128
#define MAX_UPVALUE_NUM 128
#define MAX_ID_LEN 128   //变量名最大长度

#define MAX_METHOD_NAME_LEN MAX_ID_LEN
#define MAX_ARG_NUM 16

#define MAX_SIGN_LEN MAX_METHOD_NAME_LEN + MAX_ARG_NUM * 2 + 1

#define MAX_FIELD_NUM 128

typedef struct {
  bool isEnclosingLocalVar;

  uint32_t index;
} Upvalue;

typedef struct {
  const char* name;
  uint32_t length;
  int scopeDepth;

  bool isUpvalue;
} LocalVar;

typedef enum {
  SIGN_CONSTRUCT,  //构造函数
  SIGN_METHOD,  //普通方法
  SIGN_GETTER, //getter方法
  SIGN_SETTER, //setter方法
  SIGN_SUBSCRIPT, //getter形式的下标
  SIGN_SUBSCRIPT_SETTER   //setter形式的下标
} SignatureType;

typedef struct {
  SignatureType type;
  const char* name;
  uint32_t length;
  uint32_t argNum;
} Signature;

typedef struct loop {
  int condStartIndex;
  int bodyStartIndexl
  int scopeDepth;
  int exitIndex;
  struct loop* enclosingLoop;
} Loop;

typedef struct {
  ObjString* name;
  SymbolTable fields;
  bool inStatic;
  IntBuffer instantMethods;
  IntBuffer staticMethods;
  Signature* signature;
} ClassBookKeep;

typedef struct compileUnit CompileUnit;

int defineModuleVar(VM* vm, ObjModule* objModule, const char* name, uint32_t length, Value value);

#endif
