#include <vm.h>
#include <stdlib.h>
#include <utils.h>

void initVM(VM* vm) {
   vm->allocatedBytes = 0;
   vm->allObjects = NULL;
   vm->curParser = NULL;
   StringBufferInit(&vm->allMethodNames);
   vm->allModules = newObjMap(vm);
   vm->curParser = NULL;
}


VM* newVM() {
   VM* vm = (VM*)malloc(sizeof(VM));
   if (vm == NULL) {
      MEM_ERROR("allocate VM failed!");
   }
   initVM(vm);
   buildCore(vm);
   return vm;
}
