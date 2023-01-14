#include "tiger/frame/x64frame.h"

extern frame::RegManager *reg_manager;

namespace frame {

// class InFrameAccess : public Access {
// public:
//   int offset;

//   explicit InFrameAccess(int offset) : offset(offset) {}
  
//   tree::Exp *ToExp(tree::Exp *framePtr) {
//     return new tree::MemExp(
//       new tree::BinopExp(tree::PLUS_OP, framePtr, new tree::ConstExp(offset)));
//   }
// };


// class InRegAccess : public Access {
// public:
//   temp::Temp *reg;

//   explicit InRegAccess(temp::Temp *reg) : reg(reg) {}
  
//   tree::Exp *ToExp(tree::Exp *framePtr) {
//     return new tree::TempExp(reg);
//   }
// };

class X64Frame : public Frame {
  
public:

  std::string GetLabel() {
    return name_->Name();
  }

  Access *AllocLocal(bool escape) {
    if (escape) {
      return new InFrameAccess(-(++local_num) * reg_manager->WordSize());
    } else {
      temp::Temp *r = temp::TempFactory::NewTemp();
      return new InRegAccess(r);
    }
  }



};

Frame *Frame::NewMainFrame(temp::Label *name) {
  Frame *frame = new X64Frame;
  frame->name_ = name;
  return frame;
}

Frame *Frame::NewFrame(temp::Label *name, std::list<bool> formals) {
  Frame *frame = new X64Frame;
  frame->name_ = name;
  frame->return_addr = new InFrameAccess(0);

  //初始化frame的formals
  frame->formals = new std::list<Access *>;

  //shift view实质上是生成了一些新的可以接触到参数的方式
  temp::TempList *arg_regs = reg_manager->ArgRegs();
  int arg_num = 0;
  for (bool escape : formals) {
    if (arg_num < 6) {
      Access *arg = frame->AllocLocal(escape);
      frame->formals->push_back(arg);

      //生成shift view的代码段
      if (frame->init_args == nullptr) {
        frame->init_args = new tree::MoveStm(
          arg->ToExp(new tree::TempExp(reg_manager->FramePointer())), 
          new tree::TempExp(arg_regs->NthTemp(arg_num))
        );
      } else {
        frame->init_args = new tree::SeqStm(
          frame->init_args,
          new tree::MoveStm(
            arg->ToExp(new tree::TempExp(reg_manager->FramePointer())), 
            new tree::TempExp(arg_regs->NthTemp(arg_num))
          )
        );
      }

    } else {
      frame->formals->push_back(
        new InFrameAccess((arg_num - 5) * reg_manager->WordSize())
      );
    }
    ++arg_num;
  }

  return frame;
}

tree::Exp *ExternalCall(std::string s, tree::ExpList *args)
{
  return new tree::CallExp(
    new tree::NameExp(temp::LabelFactory::NamedLabel(s)), args
  );
}

tree::Stm *ProcEntryExit1(Frame *frame, tree::Stm *stm)
{
  // save and restore callee-saved registers
  temp::TempList *callee_regs = reg_manager->CalleeSaves();

  for (temp::Temp *reg : callee_regs->GetList()) {
    Access *tmp = frame->AllocLocal(false);

    stm = new tree::SeqStm(
      new tree::MoveStm(
        tmp->ToExp(new tree::TempExp(reg_manager->FramePointer())),
        new tree::TempExp(reg)
      ),
      stm
    );

    stm = new tree::SeqStm(
      stm,
      new tree::MoveStm(
        new tree::TempExp(reg),
        tmp->ToExp(new tree::TempExp(reg_manager->FramePointer()))
      )
    );
  }

  //对参数进行shift view
  if (frame->init_args) stm = new tree::SeqStm(frame->init_args, stm);

  return stm;
}

assem::InstrList *ProcEntryExit2(assem::InstrList *body)
{
  body->Append(new assem::OperInstr("", new temp::TempList(), reg_manager->ReturnSink(), nullptr));
  return body;
}

assem::Proc *ProcEntryExit3(frame::Frame *frame, assem::InstrList *body)
{
  int frame_size = (frame->local_num + frame->max_call_args) * reg_manager->WordSize();

  body->Insert(body->GetList().begin(), new assem::OperInstr(
    "subq $"+std::to_string(frame_size)+",`d0",
    new temp::TempList({reg_manager->StackPointer()}),
    new temp::TempList(), nullptr
  ));
  body->Append(new assem::OperInstr(
    "addq $"+std::to_string(frame_size)+",`d0",
    new temp::TempList({reg_manager->StackPointer()}),
    new temp::TempList(), nullptr
  ));
  body->Append(new assem::OperInstr(
    "retq",
    new temp::TempList({reg_manager->StackPointer()}),
    new temp::TempList({reg_manager->StackPointer()}), 
    nullptr
  ));

  char buf[100];
  sprintf(buf, ".set %s_framesize, %d\n", temp::LabelFactory::LabelString(frame->name_).data(), frame_size);
  sprintf(buf, "%s%s:\n", buf, temp::LabelFactory::LabelString(frame->name_).data());
  return new assem::Proc(std::string(buf), body, "\n");
}

} // namespace frame
