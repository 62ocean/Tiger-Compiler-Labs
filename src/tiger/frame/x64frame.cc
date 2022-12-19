#include "tiger/frame/x64frame.h"

extern frame::RegManager *reg_manager;

namespace frame {

class InFrameAccess : public Access {
public:
  int offset;

  explicit InFrameAccess(int offset) : offset(offset) {}
  
  tree::Exp *ToExp(tree::Exp *framePtr) {
    return new tree::MemExp(
      new tree::BinopExp(tree::PLUS_OP, framePtr, new tree::ConstExp(offset)));
  }
};


class InRegAccess : public Access {
public:
  temp::Temp *reg;

  explicit InRegAccess(temp::Temp *reg) : reg(reg) {}
  
  tree::Exp *ToExp(tree::Exp *framePtr) {
    return new tree::TempExp(reg);
  }
};

class X64Frame : public Frame {
  
public:
  Access *AllocLocal(bool escape) {
    if (escape) {
      return new InFrameAccess(-(local_num++) * reg_manager->WordSize());
    } else {
      temp::Temp *r = temp::TempFactory::NewTemp();
      return new InRegAccess(r);
    }
  }


};

tree::Exp *ExternalCall(std::string s, tree::ExpList *args)
{
  return new tree::CallExp(
    new tree::NameExp(temp::LabelFactory::NamedLabel(s)), args
  );
}

Frame *Frame::NewFrame(temp::Label *name, std::list<bool> formals) {
  Frame *frame = new X64Frame;
  frame->name = name;

  //初始化了frame的formals
  frame->formals = new std::list<Access *>;
  // int num_inFrame = 0;
  //这里的shift view实际上是错的，具体要和传参配合起来写
  // for (bool escape : formals) {
  //   if (escape) {
  //     frame->formals->push_back(
  //       new InFrameAccess((++num_inFrame) * reg_manager->WordSize())
  //     );
  //   } else {
  //     frame->formals->push_back(
  //       new InRegAccess(temp::TempFactory::NewTemp())
  //     );
  //   }
  // }
  //没有考虑return address，怎么处理？

  //shift view实质上是生成了一些新的可以接触到参数的方式，具体把参数放到里面的工作是在FunDec中做的
  int arg_num = 0;
  for (bool escape : formals) {
    if (arg_num < 6) {
      Access *arg = frame->AllocLocal(escape);
      frame->formals->push_back(arg);
    } else {
      frame->formals->push_back(
        new InFrameAccess((arg_num - 5) * reg_manager->WordSize())
      );
    }
    ++arg_num;
  }

  return frame;
}

} // namespace frame
