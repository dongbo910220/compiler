#include "meta_obj.h"
#include "class.h"
#include "vm.h"

ObjFn* newObjFn(VM* vm, ObjModule* objModule, uint32_t slotNum) {
    ObjFn* objFn = ALLOCATE(vm, ObjFn);
    if (objFn == NULL) {
       MEM_ERROR("allocate ObjFn failed!");
    }
    initObjHeader(vm, &objFn->objHeader, OT_FUNCTION, vm->fnClass);
    ByteBufferInit(&objFn->instrStream);
    ValueBufferInit(&objFn->constants);

    objFn->module = objModule;
    objFn->maxStackSlotUsedNum = slotNum;
    objFn->upvalueNum = objFn->argNum = 0;
#ifdef DEBUG
    objFn->debug = ALLOCATE(vm, FnDebug);
    objFn->debug->fnName = NULL;
    IntBufferInit(&objFn->debug->lineNo);
#endif
    return objFn;
}

ObjClosure* newObjClosure(VM* vm, ObjFn* objFn) {
  ObjClosure* objClosure = ALLOCATE_EXTRA(vm,
  ObjClosure, sizeof(ObjUpvalue*) * objFn->upvalueNum);
  initObjHeader(vm, &objClosure->objHeader, OT_CLOSURE, vm->fnClass);
  objClosure->fn = objFn;

  uint32_t idx = 0;
  while (idx < objFn->upvalueNum) {
    objClosure->upvalues[idx] = NULL;
    idx++;
  }
  return objClosure;
}

ObjUpvalue* newObjUpvalue(VM* vm, Value* localVarPtr) {
    ObjUpvalue* objUpvalue = ALLOCATE(vm, ObjUpvalue);
    initObjHeader(vm, &objUpvalue->objHeader, OT_UPVALUE, NULL);
    objUpvalue->localVarPtr = localVarPtr;
    objUpvalue->closedUpvalue = VT_TO_VALUE(VT_NULL);
    objUpvalue->next = NULL;
    return objUpvalue;
}
