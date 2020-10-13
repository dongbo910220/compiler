#include "vm.h"
#include <stdlib.h>
#include "core.h"

//初始化虚拟机
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

//确保stack有效
void ensureStack(VM* vm, ObjThread* objThread, uint32_t neededSlots) {
    if (objThread->stackCapacity >= neededSlots) {
      return;
    }

    uint32_t newStackCapacity = ceilToPowerOf2(neededSlots);
    ASSERT(newStackCapacity > objThread->stackCapacity, "newStackCapacity error!");

    //记录原栈底以用于下面判断扩容后的栈是否是原地扩容
    Value* oldStackBottom = objThread->stack;

    uint32_t slotSize = sizeof(Value);
    objThread->stack = (Value*)memManager(vm, objThread->stack,
 	 objThread->stackCapacity * slotSize, newStackCapacity * slotSize);
   objThread->stackCapacity = newStackCapacity;

   //为判断是否原地扩容
   long offset = objThread->stack - oldStackBottom;

   //说明os无法在原地满足内存需求, 重新分配了起始地址,下面要调整
   if (offset != 0) {
     //调整各堆栈框架的地址
     uint32_t idx = 0;
     while (idx < objThread->usedFrameNum) {
       objThread->frames[idx++].stackStart += offset;
     }

     //调整"open upValue"
     ObjUpvalue* upvalue = objThread->openUpvalues;
     while (upvalue != NULL) {
       upvalue->localVarPtr += offset;
       upvalue = upvalue->next;
     }

     //更新栈顶
     objThread->esp += offset;
   }
}


//for objClosure in objThread to build stack
inline static void createFrame(VM* vm, ObjThread* objThread,
    ObjClosure* objClosure, int argNum) {

      if (objThread->usedFrameNum + 1 > objThread->frameCapacity) { //扩容
        uint32_t newCapacity = objThread->frameCapacity * 2;
        uint32_t frameSize = sizeOf(Frame);
        objThread->frames = (Frame*)memManager(vm, objThread->frames,
  	    frameSize * objThread->frameCapacity, frameSize * newCapacity);
        objThread->frameCapacity = newCapacity;
      }

      //栈大小等于栈顶-栈底
      uint32_t stackSlots = (uint32_t)(objThread->esp - objThread->stack);
      //总共需要的栈大小
      uint32_t neededSlots = stackSlots + objClosure->fn->maxStackSlotUsedNum;

      ensureStack(vm, objThread, neededSlots);

      //准备上cpu
      prepareFrame(objThread, objClosure, objThread->esp - argNum);

}

//close upvalues (slot is higher than lastslots)
static void closeUpvalue(ObjThread* objThread, Value* lastSlot) {
    ObjUpvalue* upvalue = objThread->openUpvalues;
    while (upvalue != NULL && upvalue->localVarPtr >= lastSlot) {
      //localVarPtr改指向本结构中的closedUpvalue
      upvalue->closedUpvalue = *(upvalue->localVarPtr);
      upvalue->localVarPtr = &(upvalue->closedUpvalue);

      upvalue = upvalue->next;
    }
    objThread->openUpvalues = upvalue;
}


//创建线程已打开的upvalue链表，并将localVarPtr所属的upvalue以降序插入到该链表
static ObjUpvalue* createOpenUpvalue(VM* vm, ObjThread* objThread, Value* localVarPtr) {
    //如果openUpvalues链表为空就创建
      if (objThread->openUpvalues == NULL) {
        objThread->openUpvalues = newObjUpvalue(vm, localVarPtr);
        return objThread->openUpvalues;
      }

      //下面以upvalue.localVarPtr降序组织openUpvalues
      ObjUpvalue* preUpvalue = NULL;
      ObjUpvalue* upvalue = objThread->openUpvalues;

      //后面的代码保证了openUpvalues按照降顺组织,
      //下面向堆栈的底部遍历,直到找到合适的插入位置
      while (upvalue != NULL && upvalue->localVarPtr > localVarPtr) {
        preUpvalue = upvalue;
        upvalue = upvalue->next;
      }

      //如果之前已经插入了该upvalue则返回
      if (upvalue != NULL && upvalue->localVarPtr == localVarPtr) {
         return upvalue;
      }

      //openUpvalues中未找到该upvalue,
      //现在就创建新upvalue,按照降序插入到链表
      ObjUpvalue* newUpvalue = newObjUpvalue(vm, localVarPtr);

      //保证了openUpvalues首结点upvalue->localVarPtr的值是最高的
      if (preUpvalue == NULL) {
        //说明上面while的循环体未执行,新结点（形参localVarPtr）的值大于等于链表首结点
        //因此使链表结点指向它所在的新upvalue结点
        objThread->openUpvalues = newUpvalue;
      } else {
        //preUpvalue已处于正确的位置
        preUpvalue->next = newUpvalue;
      }
      newUpvalue->next = upvalue;

      return newUpvalue;//返回该结点
}

//校验基类合法性
static void validateSuperClass(VM* vm, Value classNameValue,
      uint32_t fieldNum, Value superClassValue) {

        //首先确保superClass的类型得是class
        if (!VALUE_IS_CLASS(superClassValue)) {
          ObjString* classNameString = VALUE_TO_OBJSTR(classNameValue);
          RUN_ERROR("class \"%s\" `s superClass is not a valid class!",
    	    classNameString->value.start);
        }

        Class* superClass = VALUE_TO_CLASS(superClassValue);

        //基类不允许为内建类
         if (superClass == vm->stringClass ||
      	 superClass == vm->mapClass ||
      	 superClass == vm->rangeClass ||
      	 superClass == vm->listClass ||
      	 superClass == vm->nullClass ||
      	 superClass == vm->boolClass ||
      	 superClass == vm->numClass ||
      	 superClass == vm->fnClass ||
      	 superClass == vm->threadClass
      	 ) {
            RUN_ERROR("superClass mustn`t be a buildin class!");
         }

         //子类也要继承基类的域,
         //故子类自己的域+基类域的数量不可超过MAX_FIELD_NUM
         if (superClass->fieldNum + fieldNum > MAX_FIELD_NUM) {
            RUN_ERROR("number of field including super exceed %d!", MAX_FIELD_NUM);
         }
}

//修正部分指令的操作数
static void patchOperand(Class* class, ObjFn* fn) {
    int ip = 0;
    OpCode opCode;
    while (true) {
      opCode = (OpCode)fn->instrStream.datas[ip++];
      switch (opCode) {
        case OPCODE_LOAD_FIELD:
     	  case OPCODE_STORE_FIELD:
     	  case OPCODE_LOAD_THIS_FIELD:
     	  case OPCODE_STORE_THIS_FIELD:
       //修正子类的field数目  参数是1字节
          fn->instrStream.datas[ip++] += class->superClass->fieldNum;
          break;

          case OPCODE_SUPER0:
       	 case OPCODE_SUPER1:
       	 case OPCODE_SUPER2:
       	 case OPCODE_SUPER3:
       	 case OPCODE_SUPER4:
       	 case OPCODE_SUPER5:
       	 case OPCODE_SUPER6:
       	 case OPCODE_SUPER7:
       	 case OPCODE_SUPER8:
       	 case OPCODE_SUPER9:
       	 case OPCODE_SUPER10:
       	 case OPCODE_SUPER11:
       	 case OPCODE_SUPER12:
       	 case OPCODE_SUPER13:
       	 case OPCODE_SUPER14:
       	 case OPCODE_SUPER15:
       	 case OPCODE_SUPER16: {
           //指令流1: 2字节的method索引
           //指令流2: 2字节的基类常量索引

           ip += 2; //跳过2字节的method索引
           uint32_t superClassIdx =
                (fn->instrStream.datas[ip] << 8) | fn->instrStream.datas[ip + 1];

             //回填在函数emitCallBySignature中的占位VT_TO_VALUE(VT_NULL)
             fn->constants.datas[superClassIdx] = OBJ_TO_VALUE(class->superClass);

             ip += 2; //跳过2字节的基类索引

             break;
         }

         case OPCODE_CREATE_CLOSURE: {
           //指令流: 2字节待创建闭包的函数在常量表中的索引+函数所用的upvalue数 * 2

           //函数是存储到常量表中,获取待创建闭包的函数在常量表中的索引
           uint32_t fnIdx = (fn->instrStream.datas[ip] << 8) | fn->instrStream.datas[ip + 1];

           //递归进入该函数的指令流,继续为其中的super和field修正操作数
           patchOperand(class, VALUE_TO_OBJFN(fn->constants.datas[fnIdx]));

           //ip-1是操作码OPCODE_CREATE_CLOSURE,
     	    //闭包中的参数涉及到upvalue,调用getBytesOfOperands获得参数字节数
          ip += getBytesOfOperands(fn->instrStream.datas, fn->constants.datas, ip - 1);

          break;
         }

         case OPCODE_END:
          //用于从当前及递归嵌套闭包时返回
          return;

        default:
       	    //其它指令不需要回填因此就跳过
       	   ip += getBytesOfOperands(fn->instrStream.datas, fn->constants.datas, ip - 1);
       	   break;
      }
    }
}

//绑定方法和修正操作数
static void bindMethodAndPatch(VM* vm, OpCode opCode,
      uint32_t methodIndex, Class* class, Value methodValue) {

        //如果是静态方法,就将类指向meta类(使接收者为meta类)
        if (opCode == OPCODE_STATIC_METHOD) {
          class = class->objHeader.class;
        }

        Method method;
        method.type = MT_SCRIPT; //脚本中定义的方法
        method.obj = VALUE_TO_OBJCLOSURE(methodValue);

        //修正操作数
        patchOperand(class, method.obj->fn);

        //修正过后，绑定method到class
        bindMethod(vm, class, methodIndex, method);
}

//执行指令
VMResult executeInstruction(VM* vm, register ObjThread* curThread) {
    vm->curThread = curThread;
    register Frame* curFrame;
    register Value* stackStart;
    register uint8_t* ip;
    register ObjFn*  fn;
    Opcode opCode;

    //定义操作运行时栈的宏
    //esp是栈中下一个可写入数据的slot
    #define PUSH(value) (*curThread->esp++ = value)  //压栈
    #define POP()        (*(--curThread->esp))   //出栈
    #define DROP()       (curThread->esp--)
    #define PEEK()        (*(curThread->esp - 1)) // 获得栈顶的数据
    #define PEEK2()       (*(curThread->esp - 2))  // 获得次栈顶的数据

    //下面是读取指令流:objfn.instrStream.datas
    #define READ_BYTE()   (*ip++)   //从指令流中读取一字节
    //读取指令流中的2字节
    #define READ_SHORT()  (ip += 2, (uint16_t)((ip[-2] << 8) | ip[-1]))

    //当前指令单元执行的进度就是在指令流中的指针,即ip,将其保存起来
    #define STORE_CUR_FRAME() curFrame->ip = ip   // 备份ip以能回到当前

    //加载最新的frame
    #define LOAD_CUR_FRAME()      \
        /* frames是数组,索引从0起,故usedFrameNum-1 */   \
        curFrame = &curThread->frames[curThread->usedFrameNum - 1];   \
        stackStart = curFrame->stackStart;  \
        ip = curFrame->ip;  \
        fn = curFrame->closure->fn;

    #define DECODE loopStart; \
        opCode = READ_BYTE();   \
        switch (opCode)

    #define CASE(shortOpCode) case OPCODE_##shortOpCode
    #define LOOP() goto loopStart

    LOAD_CUR_FRAME();
    DECODE {
      //若OPCODE依赖于指令环境(栈和指令流),会在各OPCODE下说明

      CASE(LOAD_LOCAL_VAR):
      //指令流: 1字节的局部变量索引

      PUSH(stackStart[READ_BYTE()]);
      LOOP();

      CASE(LOAD_THIS_FIELD): {
        //指令流: 1字节的field索引

        uint8_t fieldIdx = READ_BYTE();

        //stackStart[0]是实例对象this
        ASSERT(VALUE_IS_OBJINSTANCE(stackStart[0]), "method receiver should be objInstance.");
        ObjInstance* objInstance = VALUE_TO_OBJINSTANCE(stackStart[0]);

        ASSERT(fieldIdx < objInstance->objHeader.class->fieldNum, "out of bounds field!");
        PUSH(objInstance->fields[fieldIdx]);
        LOOP();

      }

        CASE(POP):
        DPOP();
        LOOP();

        CASE(PUSH_NULL):
        PUSH(VT_TO_VALUE(VT_NULL));
        LOOP();

       CASE(PUSH_FALSE):
       	 PUSH(VT_TO_VALUE(VT_FALSE));
       	 LOOP();

        CASE(PUSH_TRUE):
         PUSH(VT_TO_VALUE(VT_TRUE));
         LOOP();

         CASE(STORE_LOCAL_VAR):
         //栈顶: 局部变量值
      	 //指令流: 1字节的局部变量索引

         stackStart[READ_BYTE()] = PEEK();
         LOOP();

         CASE(LOAD_CONSTANT):
         //指令流: 2字节的常量索引

        //加载常量就是把常量表中的数据入栈
        PUSH(fn->constants.datas[READ_SHORT()]);
        LOOP();

        {
          int argNum, index;
          Value* args;
          Class* class;
          Method* method;

            CASE(CALL0):
            CASE(CALL1):
            CASE(CALL2):
            CASE(CALL3):
            CASE(CALL4):
            CASE(CALL5):
            CASE(CALL6):
            CASE(CALL7):
            CASE(CALL8):
            CASE(CALL9):
            CASE(CALL10):
            CASE(CALL11):
            CASE(CALL12):
            CASE(CALL13):
            CASE(CALL14):
            CASE(CALL15):
            CASE(CALL16):
            //指令流1: 2字节的method索引
         	 //因为还有个隐式的receiver(就是下面的args[0]), 所以参数个数+1.
           argNum = opCode - OPCODE_CALL0 + 1;

           //读取2字节的数据(CALL指令的操作数),index是方法名的索引
           index = READ_SHORT();

           //为参数指针数组args赋值
           args = curThread->esp - argNum;

           //获得方法所在的类
           class = getClassOfObj(vm, args[0]);
           goto invokeMethod;

               CASE(SUPER0):
               CASE(SUPER1):
               CASE(SUPER2):
               CASE(SUPER3):
               CASE(SUPER4):
               CASE(SUPER5):
               CASE(SUPER6):
               CASE(SUPER7):
               CASE(SUPER8):
               CASE(SUPER9):
               CASE(SUPER10):
               CASE(SUPER11):
               CASE(SUPER12):
               CASE(SUPER13):
               CASE(SUPER14):
               CASE(SUPER15):
               CASE(SUPER16):
            //指令流1: 2字节的method索引
            //指令流2: 2字节的基类常量索引

            //因为还有个隐式的receiver(就是下面的args[0]), 所以参数个数+1.
            argNum = opCode - OPCODE_SUPER0 + 1;
            index = READ_SHORT();
            args = curThread->esp - argNum;

            //在函数bindMethodAndPatch中实现的基类的绑定
            class = VALUE_TO_CLASS(fn->constants.datas[READ_SHORT()]);

              invokeMethod:
            if ((uint32_t)index > class->methods.count ||
            (method = &class->methods.datas[index])->type == MT_NONE) {
              RUN_ERROR("method \"%s\" not found!", vm->allMethodNames.datas[index].str);
            }

  switch (method->type) {
      case MT_PRIMITIVE:

        //如果返回值为true,则vm进行空间回收的工作
        if (method->primFn(vm, args)) {
          //args[0]是返回值, argNum-1是保留args[0],
    		  //args[0]的空间最终由返回值的接收者即函数的主调方回收
          curThread->esp -= argNum - 1;
        } else {
          //如果返回false则说明有两种情况:
    		  //   1 出错(比如原生函数primThreadAbort使线程报错或无错退出),
    		  //   2 或者切换了线程,此时vm->curThread已经被切换为新的线程
    		  //保存线程的上下文环境,运行新线程之后还能回到当前老线程指令流的正确位置
          STORE_CUR_FRAME();

          if (!VALUE_IS_NULL(curThread->errorObj)) {
            if (VALUE_IS_OBJSTR(curThread->errorObj)) {
              ObjString* err = VALUE_TO_OBJSTR(curThread->errorObj);
              printf("%s", err->value.start);
            }
            //出错后将返回值置为null,避免主调方获取到错误的结果
            PEEK() = VT_TO_VALUE(VT_NULL);
          }

          //如果没有待执行的线程,说明执行完毕
    		  if (vm->curThread == NULL) {
    		     return VM_RESULT_SUCCESS;
    		  }

          //vm->curThread已经由返回false的函数置为下一个线程
          //切换到下一个线程的上下文
          curThread = vm->curThread;
          LOAD_CUR_FRAME();
        }
        break;

        case MT_SCRIPT:
            STORE_CUR_FRAME();
            createFrame(vm, curThread, (ObjClosure*)method->obj, argNum);
            LOAD_CUR_FRAME();
            break;

        case MT_FN_CALL:
            ASSERT(VALUE_IS_OBJCLOSURE(args[0]), "instance must be a closure!");
            ObjFn* objFn = VALUE_TO_OBJCLOSURE(args[0])->fn;
            //-1是去掉实例this
            if (argNum - 1 < objFn->argNum) {
   		  RUN_ERROR("arguments less");
   	       }

           STORE_CUR_FRAME();
           createFrame(vm, curThread, VALUE_TO_OBJCLOSURE(args[0]), argNum);
           LOAD_CUR_FRAME();  //加载最新的frame
           break;

           default:
              NOT_REACHED();
            }

            LOOP();
        }


    }



}
