#ifndef _OBJECT_THREAD_H
#define _OBJECT_THREAD_H
#include "obj_fn.h"

typedef struct objThread {
  ObjHeader objHeader;

  Value* stack;
  Value* esp;
  uint32_t stackCapacity;

  Frame* frames;
  uint32_t usedFrameNum;
  uint32_t frameCapacity;

  ObjUpvalue* openUpvalues;

  struct objThread* caller;

  Value errorObj;
} ObjThread;

void prepareFrame(ObjThread* objThread, ObjClosure* objClosure, Value* stackStart);
ObjThread* newObjThread(VM* vm, ObjClosure* objClosure);
void resetThread(ObjThread* objThread, ObjClosure*  objClosure);
#endif
