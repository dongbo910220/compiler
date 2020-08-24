#include "obj_fn.h"
#include "class.h"
#include "vm.h"
#include <string.h>

ObjModule* newObjModule(VM* vm, const char* modName) {
    ObjModule* objModule = ALLOCATE(vm, ObjModule);
    if (objModule == NULL) {
       MEM_ERROR("allocate ObjModule failed!");
    }

    initObjHeader(vm, &objModule->objHeader, OT_MODULE, NULL);

    StringBufferInit(&objModule->moduleVarName);
    ValueBufferInit(&objModule->moduleVarValue);

    objModule->name = NULL;
    if (modName != NULL) {
      objModule->name = newObjString(vm, modName, strlen(modName));
    }

    return objModule;
}

ObjInstance* newObjInstance(VM* vm, Class* class) {
  ObjInstance* objInstance = ALLOCATE_EXTRA(
    vm, ObjInstance, sizeof(Value) * class->fieldNum);

    initObjHeader(vm, &objInstance->objHeader, OT_INSTANCE, class);

    uint32_t idx = 0;
    while (idx < class->fieldNum) {
       objInstance->fields[idx++] = VT_TO_VALUE(VT_NULL);
    }
    return objInstance;
}
