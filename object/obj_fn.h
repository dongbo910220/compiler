#ifndef _OBJECT_FN_H
#define _OBJECT_FN_H
#include "utils.h"
#include "meta_obj.h"

typedef struct {
    char* fnName;
    IntBuffer lineNo;
} FnDebug;

typedef struct {
  ObjHeader objHeader;
  ByteBuffer instrStream;  //函数编译后的指令流
  ValueBuffer constants;   // 函数中的常量表

  ObjModule* module;    //本函数所属的模块
  uint32_t maxStackSlotUsedNum;
  uint32_t upvalueNum;	  //本函数所涵盖的upvalue数量
  uint8_t argNum;   //函数期望的参数个数
#if DEBUG
    FnDebug* debug;
#endif
} ObjFn;

typedef struct upvalue {
    ObjHeader objHeader;

    Value* localValPtr;
    Value closedUpvalue;
    struct upvalue* next;
} ObjUpvalue;

typedef struct {
    ObjHeader objHeader;
    ObjFn* fn;  //闭包中所要引用的函数

    ObjUpvalue* upvalues[0];
} ObjClosure;	  //闭包对象

typedef struct {
  uint8_t* ip;
  ObjClosure* closure;
  Value* stackStart;
} Frame;  //调用框架

#define INITIAL_FRAME_NUM 4

ObjUpvalue* newObjUpvalue(VM* vm, Value* localVarPtr);
ObjClosure* newObjClosure(VM* vm, ObjFn* objFn);
ObjFn* newObjFn(VM* vm, ObjModule* objModule, uint32_t maxStackSlotUsedNum);
#endif
