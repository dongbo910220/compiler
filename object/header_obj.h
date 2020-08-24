#ifndef _OBJECT_HEADER_H
#define _OBJECT_HEADER_H
#include "utils.h"
typedef enum {
   OT_CLASS,   //此项是class类型,以下都是object类型
   OT_LIST,
   OT_MAP,
   OT_MODULE,
   OT_RANGE,
   OT_STRING,
   OT_UPVALUE,
   OT_FUNCTION,
   OT_CLOSURE,
   OT_INSTANCE,
   OT_THREAD
} ObjType;  //对象类型

typedef struct objHeader {
  ObjType type;
  bool  isDark;
  Class* class;
  struct objHeader* next;
} ObjHeader;

type enum {
  VT_UNDEFINED,
  VT_NULL,
  VT_FALSE,
  VT_TRUE,
  VT_NUM,
  VT_OBJ,
} ValueType;

typedef struct {
  ValueType type;
  union {
    double num;
    ObjHeader* objHeader;
  };
} Value;

DECLARE_BUFFER_TYPE(Value)

void initObjHeader(VM* vm, ObjHeader* objHeader, ObjType objType, Class* class);
#endif
