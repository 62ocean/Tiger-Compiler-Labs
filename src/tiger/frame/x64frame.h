//
// Created by wzl on 2021/10/12.
//

#ifndef TIGER_COMPILER_X64FRAME_H
#define TIGER_COMPILER_X64FRAME_H

#include "tiger/frame/frame.h"

namespace frame {
class X64RegManager : public RegManager {

private:
  temp::Temp *fp;
  temp::Temp *sp;
  temp::Temp *ret;

public:
  X64RegManager() {
    fp = temp::TempFactory::NewTemp();
    sp = temp::TempFactory::NewTemp();
    ret = temp::TempFactory::NewTemp();
  }

  int WordSize() {return 8;}

  temp::Temp *FramePointer() {return fp;}

  temp::Temp *StackPointer() {return sp;}

  temp::Temp *ReturnValue() {return ret;}

  temp::TempList *Registers() {}

  temp::TempList *ArgRegs() {}

  temp::TempList *CallerSaves() {}

  temp::TempList *CalleeSaves() {}

  temp::TempList *ReturnSink() {}

};


} // namespace frame
#endif // TIGER_COMPILER_X64FRAME_H
