#include "obj_list.h"

ObjList* newObjList(VM* vm, uint32_t elementNum) {
  Value* elementArray = NULL;

  if (elementNum > 0) {
    elementArray = ALLOCATE_ARRAY(vm, Value, elementNum);
  }

  ObjList* objList = ALLOCATE(vm, ObjList);

  objList->elements.datas = elementArray;

  objList->elements.capacity = objList->elements.count = elementNum;
  initObjHeader(vm, &objList->objHeader, OT_LIST, vm->listClass);
  return objList;
}

void insertElement(VM* vm,  ObjList* objList, uint32_t index, Value value) {
  if (index > objList->elements.count - 1) {
     RUN_ERROR("index out bounded!");
  }

  ValueBufferAdd(vm, &objList->elements, VT_TO_VALUE(VT_NULL));

  uint32_t idx = objList->elements.count - 1;
  while (idx > index) {
    objList->elements.datas[idx] = objList->elements.datas[idx-1];
    idx--;
  }

  objList->elements.datas[index] = value;
}

static void shrinkList(VM* vm, ObjList* objList, uint32_t newCapacity) {
  uint32_t oldSize = objList->elements.capacity * sizeof(Value);
  uint32_t newSize = newCapacity * sizeof(Value);
  memManager(vm, objList->elements.datas, oldSize, newSize);
  objList->elements.capacity = newCapacity;
}

Value removeElement(VM* vm, ObjList* objList, uint32_t index) {
  Value valueRemoved = objList->elements.datas[index];
  uint32_t idx = index;
  while (idx < objList->elements.count) {
     objList->elements.datas[idx] = objList->elements.datas[idx + 1];
     idx++;
  }

  uint32_t _capacity = objList->elements.capacity / CAPACITY_GROW_FACTOR;
  if (_capacity > objList->elements.count) {
     shrinkList(vm, objList, _capacity);
  }

  objList->elements.count--;
  return valueRemoved;
}  
