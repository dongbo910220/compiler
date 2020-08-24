#ifndef _OBJECT_METAOBJ_H
#define _OBJECT_METAOBJ_H
#include "obj_string.h"

typedef struct {
   ObjHeader objHeader;
   SymbolTable moduleVarName;
   ValueBuffer moduleVarValue;
   ObjString* name;
} ObjModule;

typedef struct {
  ObjHeader objHeader;
  Value fields[0];
} ObjInstance;

ObjModule* newObjModule(VM* vm, const char* modName);
ObjInstance* newObjInstance(VM* vm, Class* class);
#endif
