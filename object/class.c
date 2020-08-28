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
