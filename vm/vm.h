#ifndef _VM_VM_H
#define _VM_VM_H
#include "common.h"
#include "class.h"
#include "obj_map.h"
#include "obj_thread.h"
#include "parser.h"

typedef enum vmResult {
   VM_RESULT_SUCCESS,
   VM_RESULT_ERROR
} VMResult;

struct vm {
   Class* classOfClass;
   Class* objectClass;
   Class* stringClass;
   Class* mapClass;
   Class* rangeClass;
   Class* listClass;
   Class* nullClass;
   Class* boolClass;
   Class* numClass;
   Class* fnClass;
   Class* threadClass;
   uint32_t allocatedBytes;  //累计已分配的内存量
   ObjHeader* allObjects;  //所有已分配对象链表
   SymbolTable allMethodNames;    //(所有)类的方法名
   ObjMap* allModules;
   ObjThread* curThread;   //当前正在执行的线程
   Parser* curParser;  //当前词法分析器
};

void initVM(VM* vm);
VM* newVM(void);
#endif
