//
// Created by wzl on 2021/10/12.
//

#ifndef TIGER_COMPILER_X64FRAME_H
#define TIGER_COMPILER_X64FRAME_H

#include "tiger/frame/frame.h"

namespace frame {

enum regName {
  RAX,
  RBX,
  RCX,
  RDX,
  RSI,
  RDI,
  RBP,
  RSP,
  R8,
  R9,
  R10,
  R11,
  R12,
  R13,
  R14,
  R15
};

class X64RegManager : public RegManager {

private:

public:
  X64RegManager() {
    reg_num = 16;

    std::string reg_name[16] = {"%rax","%rbx","%rcx","%rdx","%rsi","%rdi","%rbp","%rsp","%r8","%r9","%r10","%r11","%r12","%r13","%r14","%r15"};

    for (int i = 0; i < 16; ++i) {
      temp::Temp *t = temp::TempFactory::NewTemp();
      regs_.push_back(t);
      temp_map_->Enter(t, new std::string(reg_name[i]));
    }

  }

  int WordSize() {return 8;}

  temp::Temp *FramePointer() {return regs_[RBP];}

  temp::Temp *StackPointer() {return regs_[RSP];}

  temp::Temp *ReturnValue() {return regs_[RAX];}

  temp::TempList *Registers() {
    temp::TempList *list = new temp::TempList;
    for (temp::Temp *reg : regs_) {
      list->Append(reg);
    }
    return list;
  }

  temp::TempList *ArgRegs() {
    return new temp::TempList({regs_[RDI], regs_[RSI], regs_[RDX], regs_[RCX], regs_[R8], regs_[R9]});
  }

  temp::TempList *CallerSaves() {
    return new temp::TempList({regs_[R10], regs_[R11]});
  }

  temp::TempList *CalleeSaves() {
    return new temp::TempList({regs_[RBX], regs_[R12], regs_[R13], regs_[R14], regs_[R15]});
  }

  temp::TempList *ReturnSink() {
    temp::TempList *list = CalleeSaves();
    list->Append(ReturnValue());
    list->Append(StackPointer());
    //fp是吗？
    return list;
  }

};


} // namespace frame
#endif // TIGER_COMPILER_X64FRAME_H
