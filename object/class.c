#include "class.h"
#include "common.h"
#include "string.h"
#include "obj_range.h"
#include "core.h"
#include "vm.h"

DEFINE_BUFFER_METHOD(Method)

bool valueIsEqual(Value a, Value b) {
  if (a.type != b.type) {
     return false;
  }

  if (a.type == VT_NUM) {
     return a.num == b.num;
  }

  if (a.objHeader == b.objHeader) {
     return true;
  }

  if (a.objHeader->type != b.objHeader->type) {
     return false;
  }

  if (a.objHeader->type == OT_STRING) {
    ObjString* strA = VALUE_TO_OBJSTR(a);
    ObjString* strB = VALUE_TO_OBJSTR(b);
    return (strA->value.length == strB->value.length &&
    memcmp(strA->value.start, strB->value.start, strA->value.length) == 0);
  }

  if (a.objHeader->type == OT_RANGE) {
    ObjRange* rgA = VALUE_TO_OBJRANGE(a);
    ObjRange* rgB = VALUE_TO_OBJRANGE(b);
    return (rgA->from == rgB->from && rgA->to == rgB->to);
  }
  return false;
}

Class* newRawClass(VM* vm, const char* name, uint32_t fieldNum) {
  Class* class = ALLOCATE(vm, Class);

  initObjHeader(vm, &class->objHeader, OT_CLASS, NULL);
  class->name = newObjString(vm, name, strlen(name));
  class->fieldNum = fieldNum;
  class->superClass = NULL;
  MethodBufferInit(&class->methods);

  return class;
}

inline Class* getClassOfObj(VM* vm, Value object) {
    switch (object.type) {
      case VT_NULL:
        return vm->nullClass;
      case VT_FALSE:
      case VT_TRUE:
        return vm->boolClass;
      case VT_NUM:
        return vm->numClass;
      case VT_OBJ:
        return VALUE_TO_OBJ(object)->class;
      default:
        NOT_REACHED();
    }
    return NULL;
}
