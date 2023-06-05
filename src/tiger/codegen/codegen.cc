#include "tiger/codegen/codegen.h"

#include <cassert>
#include <sstream>

extern frame::RegManager *reg_manager;

namespace {

constexpr int maxlen = 1024;

//需要利用fs将fp全部替换成sp

} // namespace

namespace cg {

void CodeGen::Codegen() {
  auto *list = new assem::InstrList();
  for (tree::Stm *stm : traces_->GetStmList()->GetList()) {
    stm->Munch(*list, frame_->GetLabel()+"_framesize");
  }
  assem_instr_ = std::make_unique<AssemInstr>(frame::ProcEntryExit2(list));
}

void AssemInstr::Print(FILE *out, temp::Map *map) const {
  for (auto instr : instr_list_->GetList())
    instr->Print(out, map);
}
} // namespace cg

namespace tree {

void SeqStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  assert(0);
}

void LabelStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  instr_list.Append(new assem::LabelInstr(temp::LabelFactory::LabelString(label_),label_));
}

void JumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  instr_list.Append(new assem::OperInstr(
    "jmp "+temp::LabelFactory::LabelString(exp_->name_),
    new temp::TempList(), new temp::TempList(),
    new assem::Targets(jumps_)
  ));
}

void CjumpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *left_reg = left_->Munch(instr_list, fs);
  temp::Temp *right_reg = right_->Munch(instr_list, fs);
  instr_list.Append(new assem::OperInstr(
    "cmpq `s1,`s0",
    new temp::TempList(),
    new temp::TempList({left_reg, right_reg}),
    nullptr
  ));

  switch (op_)
  {
  case EQ_OP:
    instr_list.Append(new assem::OperInstr(
      "je "+temp::LabelFactory::LabelString(true_label_),
      new temp::TempList(), new temp::TempList(),
      new assem::Targets(new std::vector<temp::Label *>({true_label_, false_label_}))
    ));
    return;
    break;

  case NE_OP:
    instr_list.Append(new assem::OperInstr(
      "jne "+temp::LabelFactory::LabelString(true_label_),
      new temp::TempList(), new temp::TempList(),
      new assem::Targets(new std::vector<temp::Label *>({true_label_, false_label_}))
    ));
    return;
    break;

  case LT_OP:
    instr_list.Append(new assem::OperInstr(
      "jl "+temp::LabelFactory::LabelString(true_label_),
      new temp::TempList(), new temp::TempList(),
      new assem::Targets(new std::vector<temp::Label *>({true_label_, false_label_}))
    ));
    return;
    break;

  case GT_OP:
    instr_list.Append(new assem::OperInstr(
      "jg "+temp::LabelFactory::LabelString(true_label_),
      new temp::TempList(), new temp::TempList(),
      new assem::Targets(new std::vector<temp::Label *>({true_label_, false_label_}))
    ));
    return;
    break;

  case LE_OP:
    instr_list.Append(new assem::OperInstr(
      "jle "+temp::LabelFactory::LabelString(true_label_),
      new temp::TempList(), new temp::TempList(),
      new assem::Targets(new std::vector<temp::Label *>({true_label_, false_label_}))
    ));
    return;
    break;

  case GE_OP:
    instr_list.Append(new assem::OperInstr(
      "jge "+temp::LabelFactory::LabelString(true_label_),
      new temp::TempList(), new temp::TempList(),
      new assem::Targets(new std::vector<temp::Label *>({true_label_, false_label_}))
    ));
    return;
    break;
  
  default:
    break;
  }
}

void MoveStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  /* 当move的两侧都是MemExp时，必须优先匹配destination为MemExp的所有情况 */

  if (typeid(*dst_) == typeid(MemExp) && typeid(*src_) == typeid(ConstExp)) {
    MemExp *dst_mem = static_cast<MemExp *>(dst_);
    if (typeid(*dst_mem->exp_) == typeid(BinopExp)) {
      BinopExp *dst_binop = static_cast<BinopExp *>(dst_mem->exp_);
      if (dst_binop->op_ == tree::PLUS_OP && typeid(*dst_binop->right_) == typeid(ConstExp)) { 
        if (typeid(*dst_binop->left_) == typeid(TempExp) && ((TempExp *)dst_binop->left_)->temp_ == reg_manager->FramePointer()) {
          //move(mem(+(fp,const)),const)
          instr_list.Append(new assem::OperInstr(
            "movq $"+std::to_string(((ConstExp *)src_)->consti_)+","+std::to_string(((ConstExp *)dst_binop->right_)->consti_)+"+"+std::string(fs.data())+"(`s0)",
            new temp::TempList(),
            new temp::TempList({reg_manager->StackPointer()}), nullptr
          ));
          return;
        }
        //move(mem(+(r,const)),const)
        temp::Temp *dst_binop_reg = dst_binop->left_->Munch(instr_list, fs);
        instr_list.Append(new assem::OperInstr(
          "movq $"+std::to_string(((ConstExp *)src_)->consti_)+","+std::to_string(((ConstExp *)dst_binop->right_)->consti_)+"(`s0)",
          new temp::TempList(),
          new temp::TempList({dst_binop_reg}), nullptr
        ));
        return;
      }
      if (dst_binop->op_ == tree::PLUS_OP && typeid(*dst_binop->left_) == typeid(ConstExp)) {
        if (typeid(*dst_binop->right_) == typeid(TempExp) && ((TempExp *)dst_binop->right_)->temp_ == reg_manager->FramePointer()) {
          //move(mem(+(const,fp)),const)
          instr_list.Append(new assem::OperInstr(
            "movq $"+std::to_string(((ConstExp *)src_)->consti_)+","+std::to_string(((ConstExp *)dst_binop->left_)->consti_)+"+"+std::string(fs.data())+"(`s0)",
            new temp::TempList(),
            new temp::TempList({reg_manager->StackPointer()}), nullptr
          ));
          return;
        }
        //move(mem(+(const,r)),const)
        temp::Temp *dst_binop_reg = dst_binop->right_->Munch(instr_list, fs);
        instr_list.Append(new assem::OperInstr(
          "movq $"+std::to_string(((ConstExp *)src_)->consti_)+","+std::to_string(((ConstExp *)dst_binop->left_)->consti_)+"(`s0)",
          new temp::TempList(),
          new temp::TempList({dst_binop_reg}), nullptr
        ));
        
        return;
      }
    }
    if (typeid(*dst_mem->exp_) == typeid(ConstExp)) {
      //move(mem(const),const)
      instr_list.Append(new assem::OperInstr(
        "movq $"+std::to_string(((ConstExp *)src_)->consti_)+","+std::to_string(((ConstExp *)dst_mem->exp_)->consti_),
        new temp::TempList(), new temp::TempList(), nullptr
      ));
      return;
    }
  }
  if (typeid(*dst_) == typeid(MemExp)) {
    MemExp *dst_mem = static_cast<MemExp *>(dst_);
    if (typeid(*dst_mem->exp_) == typeid(BinopExp)) {
      BinopExp *dst_binop = static_cast<BinopExp *>(dst_mem->exp_);
      if (dst_binop->op_ == tree::PLUS_OP && typeid(*dst_binop->right_) == typeid(ConstExp)) {
        temp::Temp *src_reg = src_->Munch(instr_list, fs);
        if (typeid(*dst_binop->left_) == typeid(TempExp) && ((TempExp *)dst_binop->left_)->temp_ == reg_manager->FramePointer()) {
          //move(mem(+(fp,const)),r)
          instr_list.Append(new assem::OperInstr(
            "movq `s0,"+std::to_string(((ConstExp *)dst_binop->right_)->consti_)+"+"+std::string(fs.data())+"(`s1)",
            new temp::TempList(),
            new temp::TempList({src_reg, reg_manager->StackPointer()}),
            nullptr
          ));
          return;
        }
        //move(mem(+(r,const)),r)
        temp::Temp *dst_binop_reg = dst_binop->left_->Munch(instr_list, fs);
          instr_list.Append(new assem::OperInstr(
            "movq `s0,"+std::to_string(((ConstExp *)dst_binop->right_)->consti_)+"(`s1)",
            new temp::TempList(),
            new temp::TempList({src_reg, dst_binop_reg}),
            nullptr
          ));
        
        return;
      }
      if (dst_binop->op_ == tree::PLUS_OP && typeid(*dst_binop->left_) == typeid(ConstExp)) {
        temp::Temp *src_reg = src_->Munch(instr_list, fs);
        if (typeid(*dst_binop->right_) == typeid(TempExp) && ((TempExp *)dst_binop->right_)->temp_ == reg_manager->FramePointer()) {
          //move(mem(+(const,fp)),r)
          instr_list.Append(new assem::OperInstr(
            "movq `s0,"+std::to_string(((ConstExp *)dst_binop->left_)->consti_)+"+"+std::string(fs.data())+"(`s1)",
            new temp::TempList(),
            new temp::TempList({src_reg, reg_manager->StackPointer()}),
            nullptr
          ));
          return;
        }
        //move(mem(+(const,r)),r)
        temp::Temp *dst_binop_reg = dst_binop->right_->Munch(instr_list, fs);
          instr_list.Append(new assem::OperInstr(
            "movq `s0,"+std::to_string(((ConstExp *)dst_binop->left_)->consti_)+"(`s1)",
            new temp::TempList(),
            new temp::TempList({src_reg, dst_binop_reg}),
            nullptr
          ));  
        
        return;
      }
    }
    if (typeid(*dst_mem->exp_) == typeid(ConstExp)) {
      //move(mem(const),r)
      temp::Temp *src_reg = src_->Munch(instr_list, fs);
      instr_list.Append(new assem::OperInstr(
        "movq `s0,"+std::to_string(((ConstExp *)dst_mem->exp_)->consti_),
        new temp::TempList(),
        new temp::TempList({src_reg}),
        nullptr
      ));
      return;
    }
    if (typeid(*src_) == typeid(ConstExp)) {
      //move(mem(r),const)
      temp::Temp *dst_mem_reg = dst_mem->exp_->Munch(instr_list, fs);
      instr_list.Append(new assem::OperInstr(
        "movq $"+std::to_string(((ConstExp *)src_)->consti_)+",0(`s0)",
        new temp::TempList(),
        new temp::TempList({dst_mem_reg}), nullptr
      ));
      return;
    }
  }
  if (typeid(*dst_) == typeid(MemExp)) {
    MemExp *dst_mem = static_cast<MemExp *>(dst_);
    //move(mem(r),r)
    temp::Temp *src_reg = src_->Munch(instr_list, fs);
    temp::Temp *dst_mem_reg = dst_mem->exp_->Munch(instr_list, fs);
    instr_list.Append(new assem::OperInstr(
      "movq `s0,0(`s1)",
      new temp::TempList(),
      new temp::TempList({src_reg, dst_mem_reg}),
      nullptr
    ));
    return;
  }
  if (typeid(*src_) == typeid(MemExp)) {
    MemExp *src_mem = static_cast<MemExp *>(src_);
    if (typeid(*src_mem->exp_) == typeid(BinopExp)) {
      BinopExp *src_binop = static_cast<BinopExp *>(src_mem->exp_);
      temp::Temp *dst_reg = dst_->Munch(instr_list, fs);
      if (src_binop->op_ == tree::PLUS_OP && typeid(*src_binop->right_) == typeid(ConstExp)) {
        //move(r,mem(+(fp,const)))
        if (typeid(*src_binop->left_) == typeid(TempExp) && ((TempExp *)src_binop->left_)->temp_ == reg_manager->FramePointer()) {
          instr_list.Append(new assem::OperInstr(
            "movq "+std::to_string(((ConstExp *)src_binop->right_)->consti_)+"+"+std::string(fs.data())+"(`s0),`d0",
            new temp::TempList({dst_reg}),
            new temp::TempList({reg_manager->StackPointer()}),
            nullptr
          ));  
          return;
        }
        //move(r,mem(+(r,const)))
        temp::Temp *src_binop_reg = src_binop->left_->Munch(instr_list, fs);
        instr_list.Append(new assem::OperInstr(
          "movq "+std::to_string(((ConstExp *)src_binop->right_)->consti_)+"(`s0),`d0",
          new temp::TempList({dst_reg}),
          new temp::TempList({src_binop_reg}),
          nullptr
        ));
        
        return;
      }
      if (src_binop->op_ == tree::PLUS_OP && typeid(*src_binop->left_) == typeid(ConstExp)) {
        temp::Temp *dst_reg = dst_->Munch(instr_list, fs);
        //move(r,mem(+(const,fp)))
        if (typeid(*src_binop->right_) == typeid(TempExp) && ((TempExp *)src_binop->right_)->temp_ == reg_manager->FramePointer()) {
          instr_list.Append(new assem::OperInstr(
            "movq "+std::to_string(((ConstExp *)src_binop->left_)->consti_)+"+"+std::string(fs.data())+"(`s0),`d0",
            new temp::TempList({dst_reg}),
            new temp::TempList({reg_manager->StackPointer()}),
            nullptr
          ));
          return;
        }
        //move(r,mem(+(const,r)))
        temp::Temp *src_binop_reg = src_binop->right_->Munch(instr_list, fs);
        instr_list.Append(new assem::OperInstr(
          "movq "+std::to_string(((ConstExp *)src_binop->left_)->consti_)+"(`s0),`d0",
          new temp::TempList({dst_reg}),
          new temp::TempList({src_binop_reg}),
          nullptr
        ));
        return;
      }
    }
  }
  
  if (typeid(*src_) == typeid(MemExp)) {
    MemExp *src_mem = static_cast<MemExp *>(src_);
    if (typeid(*src_mem->exp_) == typeid(ConstExp)) {
      //move(r,mem(const))
      temp::Temp *dst_reg = dst_->Munch(instr_list, fs);
      instr_list.Append(new assem::OperInstr(
        "movq "+std::to_string(((ConstExp *)src_mem->exp_)->consti_)+",`d0",
        new temp::TempList({dst_reg}),
        new temp::TempList(), nullptr
      ));
      return;
    }

    //move(r,mem(r))
    temp::Temp *dst_reg = dst_->Munch(instr_list, fs);
    temp::Temp *src_mem_reg = src_mem->exp_->Munch(instr_list, fs);
    instr_list.Append(new assem::OperInstr(
      "movq 0(`s0),`d0",
      new temp::TempList({dst_reg}),
      new temp::TempList({src_mem_reg}),
      nullptr
    ));
    return;
  }
  
  if (typeid(*src_) == typeid(ConstExp)) {
    //move(r,const)
    temp::Temp *dst_reg = dst_->Munch(instr_list, fs);
    instr_list.Append(new assem::OperInstr(
      "movq $"+std::to_string(((ConstExp *)src_)->consti_)+",`d0",
      new temp::TempList({dst_reg}),
      new temp::TempList(), nullptr
    ));
    return;
  }

  //move(r,r)
  temp::Temp *dst_reg = dst_->Munch(instr_list, fs);
  temp::Temp *src_reg = src_->Munch(instr_list, fs);
  instr_list.Append(new assem::MoveInstr(
    "movq `s0,`d0", 
    new temp::TempList({dst_reg}), 
    new temp::TempList({src_reg})
  ));
  return;
}

void ExpStm::Munch(assem::InstrList &instr_list, std::string_view fs) {
  //可以通过判断，只对callExp执行munch吗？（减少不需要的语句）
  //后续会消除不需要的语句吗？
  exp_->Munch(instr_list, fs);
}

temp::Temp *BinopExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *r = temp::TempFactory::NewTemp();
  switch (op_)
  {
  case PLUS_OP: 
    if (typeid(*left_) == typeid(MemExp)) {
      MemExp *left_mem = static_cast<MemExp *>(left_);
      if (typeid(*left_mem->exp_) == typeid(BinopExp)) {
        BinopExp *left_binop = static_cast<BinopExp *>(left_mem->exp_);
        if (left_binop->op_ == PLUS_OP && typeid(*left_binop->right_) == typeid(ConstExp)) {
          temp::Temp *right_reg = right_->Munch(instr_list, fs);
          instr_list.Append(new assem::MoveInstr(
              "movq `s0,`d0",
              new temp::TempList(r),
              new temp::TempList(right_reg)
            ));
          if (typeid(*left_binop->left_) == typeid(TempExp) && ((TempExp *)left_binop->left_)->temp_ == reg_manager->FramePointer()) {
            //+(mem(+(fp,const)),r)
            instr_list.Append(new assem::OperInstr(
              "addq "+std::to_string(((ConstExp *)left_binop->right_)->consti_)+"+"+std::string(fs.data())+"(`s1),`d0",
              new temp::TempList({r}),
              new temp::TempList({r, reg_manager->StackPointer()}),
              nullptr
            ));
            return r;
          } else {
            //+(mem(+(r,const)),r)
            temp::Temp *left_binop_reg = left_binop->left_->Munch(instr_list, fs);
            instr_list.Append(new assem::OperInstr(
              "addq "+std::to_string(((ConstExp *)left_binop->right_)->consti_)+"(`s1),`d0",
              new temp::TempList({r}),
              new temp::TempList({r, left_binop_reg}),
              nullptr
            ));
            return r;
          }
          
        }
      }
    }
    if (typeid(*right_) == typeid(MemExp)) {
      MemExp *right_mem = static_cast<MemExp *>(right_);
      if (typeid(*right_mem->exp_) == typeid(BinopExp)) {
        BinopExp *right_binop = static_cast<BinopExp *>(right_mem->exp_);
        if (right_binop->op_ == PLUS_OP && typeid(*right_binop->right_) == typeid(ConstExp)) {
          temp::Temp *left_reg = left_->Munch(instr_list, fs);
          instr_list.Append(new assem::MoveInstr(
            "movq `s0,`d0",
            new temp::TempList(r),
            new temp::TempList(left_reg)
          ));
          if (typeid(*right_binop->left_) == typeid(TempExp) && ((TempExp *)right_binop->left_)->temp_ == reg_manager->FramePointer()) {
            //+(r,mem(+(fp,const)))
            instr_list.Append(new assem::OperInstr(
              "addq "+std::to_string(((ConstExp *)right_binop->right_)->consti_)+"+"+std::string(fs.data())+"(`s1),`d0",
              new temp::TempList({r}),
              new temp::TempList({r, reg_manager->StackPointer()}),
              nullptr
            ));
            return r;
          } else {
            //+(r,mem(+(r,const)))
            temp::Temp *right_binop_reg = right_binop->left_->Munch(instr_list, fs);
            instr_list.Append(new assem::OperInstr(
              "addq "+std::to_string(((ConstExp *)right_binop->right_)->consti_)+"(`s1),`d0",
              new temp::TempList({r}),
              new temp::TempList({r, right_binop_reg}),
              nullptr
            ));
            return r;
          }
          
        }
      }
    }
    if (typeid(*left_) == typeid(ConstExp)) {
      //+(const,r)
      temp::Temp *reg = right_->Munch(instr_list, fs);
      instr_list.Append(new assem::MoveInstr(
        "movq `s0,`d0",
        new temp::TempList(r),
        new temp::TempList(reg)
      ));
      instr_list.Append(new assem::OperInstr(
        "addq $"+std::to_string(((ConstExp *)left_)->consti_)+",`d0",
        new temp::TempList(r),
        new temp::TempList(r), 
        nullptr
      ));
      return r;
    }
    if (typeid(*right_) == typeid(ConstExp)) {
      //+(r,const)
      temp::Temp *reg = left_->Munch(instr_list, fs);
      instr_list.Append(new assem::MoveInstr(
        "movq `s0,`d0",
        new temp::TempList(r),
        new temp::TempList(reg)
      ));
      instr_list.Append(new assem::OperInstr(
        "addq $"+std::to_string(((ConstExp *)right_)->consti_)+",`d0",
        new temp::TempList(r),
        new temp::TempList(r), 
        nullptr
      ));
      return r;
    }
    {
      //+(r,r)
      temp::Temp *reg1 = left_->Munch(instr_list, fs);
      temp::Temp *reg2 = right_->Munch(instr_list, fs);
      instr_list.Append(new assem::MoveInstr(
        "movq `s0,`d0",
        new temp::TempList(r),
        new temp::TempList(reg1)
      ));
      instr_list.Append(new assem::OperInstr(
        "addq `s1,`d0",
        new temp::TempList({r}),
        new temp::TempList({r, reg2}),
        nullptr
      ));
      return r;
    }
    break;

  case MINUS_OP:
    if (typeid(*right_) == typeid(ConstExp)) {
      //-(r,const)
      temp::Temp *reg = left_->Munch(instr_list, fs);
      instr_list.Append(new assem::MoveInstr(
        "movq `s0,`d0",
        new temp::TempList(r),
        new temp::TempList(reg)
      ));
      instr_list.Append(new assem::OperInstr(
        "subq $"+std::to_string(((ConstExp *)right_)->consti_)+",`d0",
        new temp::TempList(r),
        new temp::TempList(r), 
        nullptr
      ));
      return r;
    }
    {
      //-(r,r)
      temp::Temp *reg1 = left_->Munch(instr_list, fs);
      temp::Temp *reg2 = right_->Munch(instr_list, fs);
      instr_list.Append(new assem::MoveInstr(
        "movq `s0,`d0",
        new temp::TempList(r),
        new temp::TempList(reg1)
      ));
      instr_list.Append(new assem::OperInstr(
        "subq `s1,`d0",
        new temp::TempList({r}),
        new temp::TempList({r, reg2}),
        nullptr
      ));
      return r;
    }
    break;

  case MUL_OP:
    {
      //*(r,r)
      temp::Temp *reg1 = left_->Munch(instr_list, fs);
      temp::Temp *reg2 = right_->Munch(instr_list, fs);
      instr_list.Append(new assem::MoveInstr(
        "movq `s0,`d0",
        new temp::TempList({reg_manager->GetRegister(frame::RAX)}),
        new temp::TempList({reg1})
      ));
      instr_list.Append(new assem::OperInstr(
        "imulq `s0",
        new temp::TempList({reg_manager->GetRegister(frame::RAX), reg_manager->GetRegister(frame::RDX)}),
        new temp::TempList({reg2, reg_manager->GetRegister(frame::RAX)}),
        nullptr
      ));
      //加上这一句有利于寄存器分配，不需要的移动会消掉，需要的移动会保留
      //如不加这一句，有一些寄存器会被默认分配为%rax，不能灵活处理
      instr_list.Append(new assem::MoveInstr(
        "movq `s0,`d0",
        new temp::TempList({r}),
        new temp::TempList({reg_manager->GetRegister(frame::RAX)})
      ));
      temp::Temp *tmp = temp::TempFactory::NewTemp();
      instr_list.Append(new assem::MoveInstr(
        "movq `s0,`d0",
        new temp::TempList(tmp),
        new temp::TempList(reg_manager->GetRegister(frame::RDX))
      ));
      return r;
    }
    break;

  case DIV_OP:
    {
      // /(r,r)
      temp::Temp *reg1 = left_->Munch(instr_list, fs);
      temp::Temp *reg2 = right_->Munch(instr_list, fs);
      instr_list.Append(new assem::MoveInstr(
        "movq `s0,`d0",
        new temp::TempList({reg_manager->GetRegister(frame::RAX)}),
        new temp::TempList({reg1})
      ));
      instr_list.Append(new assem::OperInstr(
        "movq $0,`d0",
        new temp::TempList({reg_manager->GetRegister(frame::RDX)}),
        new temp::TempList(), nullptr
      ));
      instr_list.Append(new assem::OperInstr(
        "idivq `s0",
        new temp::TempList({reg_manager->GetRegister(frame::RAX), reg_manager->GetRegister(frame::RDX)}),
        new temp::TempList({reg2, reg_manager->GetRegister(frame::RAX), reg_manager->GetRegister(frame::RDX)}),
        nullptr
      ));
      //加上这一句有利于寄存器分配，不需要的移动会消掉，需要的移动会保留
      //如不加这一句，有一些寄存器会被默认分配为%rax，不能灵活处理
      instr_list.Append(new assem::MoveInstr(
        "movq `s0,`d0",
        new temp::TempList({r}),
        new temp::TempList({reg_manager->GetRegister(frame::RAX)})
      ));
      temp::Temp *tmp = temp::TempFactory::NewTemp();
      instr_list.Append(new assem::MoveInstr(
        "movq `s0,`d0",
        new temp::TempList(tmp),
        new temp::TempList(reg_manager->GetRegister(frame::RDX))
      ));
      return r;
    }
    break;
  
  default:
    break;
  }
}

temp::Temp *MemExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *r = temp::TempFactory::NewTemp();
  if (typeid(*exp_) == typeid(BinopExp)) {
    BinopExp *binop = static_cast<BinopExp *>(exp_);
    if (binop->op_ == PLUS_OP && typeid(*binop->right_) == typeid(ConstExp)) {
      //mem(+(r,const))
      temp::Temp *reg = binop->left_->Munch(instr_list, fs);
      if (reg == reg_manager->FramePointer()) {
        instr_list.Append(new assem::OperInstr(
          "movq "+std::to_string(((ConstExp *)binop->right_)->consti_)+"+"+std::string(fs.data())+"(`s0),`d0",
          new temp::TempList({r}),
          new temp::TempList({reg_manager->StackPointer()}),
          nullptr
        ));
      } else {
          instr_list.Append(new assem::OperInstr(
          "movq "+std::to_string(((ConstExp *)binop->right_)->consti_)+"(`s0),`d0",
          new temp::TempList({r}),
          new temp::TempList({reg}),
          nullptr
        ));
      }
      
      return r;
    }
    if (binop->op_ == PLUS_OP && typeid(*binop->left_) == typeid(ConstExp)) {
      //mem(+(const,r))
      temp::Temp *reg = binop->right_->Munch(instr_list, fs);
      if (reg == reg_manager->FramePointer()) {
        instr_list.Append(new assem::OperInstr(
          "movq "+std::to_string(((ConstExp *)binop->left_)->consti_)+"+"+std::string(fs.data())+"(`s0),`d0",
          new temp::TempList({r}),
          new temp::TempList({reg_manager->StackPointer()}),
          nullptr
        ));
      } else {
        instr_list.Append(new assem::OperInstr(
          "movq "+std::to_string(((ConstExp *)binop->left_)->consti_)+"(`s0),`d0",
          new temp::TempList({r}),
          new temp::TempList({reg}),
          nullptr
        ));
      }
      return r;
    }
  }
  if (typeid(*exp_) == typeid(ConstExp)) {
    //mem(const)
    instr_list.Append(new assem::OperInstr(
      "movq "+std::to_string(((ConstExp *)exp_)->consti_)+",`d0",
      new temp::TempList({r}),
      new temp::TempList(), nullptr
    ));
    return r;
  }
  //mem(r)
  temp::Temp *reg = exp_->Munch(instr_list, fs);
  instr_list.Append(new assem::OperInstr(
    "movq 0(`s0),`d0",
    new temp::TempList({r}),
    new temp::TempList({reg}),
    nullptr
  ));
  return r;
}

temp::Temp *TempExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  if (temp_ == reg_manager->FramePointer()) {
    temp::Temp *r = temp::TempFactory::NewTemp();
    instr_list.Append(new assem::OperInstr(
      "leaq "+std::string(fs.data())+"(`s0),`d0",
      new temp::TempList(r),
      new temp::TempList(reg_manager->StackPointer()),
      nullptr
    ));
    return r;
  }
  return temp_;
}

temp::Temp *EseqExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  assert(0);
}

temp::Temp *NameExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *r = temp::TempFactory::NewTemp();
  instr_list.Append(new assem::OperInstr(
    "leaq "+temp::LabelFactory::LabelString(name_)+"(%rip),`d0",
    new temp::TempList({r}),
    new temp::TempList(), nullptr
  ));
  return r;
}

temp::Temp *ConstExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  temp::Temp *r = temp::TempFactory::NewTemp();
  instr_list.Append(new assem::OperInstr(
    "movq $"+std::to_string(consti_)+",`d0",
    new temp::TempList({r}),
    new temp::TempList(), nullptr
  ));
  return r;
}

temp::Temp *CallExp::Munch(assem::InstrList &instr_list, std::string_view fs) {
  //translate生成的所有CallExp的exp_都是NameExp
  NameExp *fun = static_cast<NameExp *>(fun_);
  temp::TempList *list = args_->MunchArgs(instr_list, fs);

  temp::TempList *calldefs = reg_manager->CallerSaves();
  // calldefs->Append(reg_manager->ReturnValue());

  instr_list.Append(new assem::OperInstr(
    "callq "+temp::LabelFactory::LabelString(fun->name_),
    calldefs, list, 
    nullptr
  ));
  temp::Temp *r = temp::TempFactory::NewTemp();
  instr_list.Append(new assem::MoveInstr(
    "movq `s0,`d0",
    new temp::TempList(r),
    new temp::TempList({reg_manager->ReturnValue()})
  ));
  return r;
}

temp::TempList *ExpList::MunchArgs(assem::InstrList &instr_list, std::string_view fs) {
  int i = 0;
  temp::TempList *arg_temps = reg_manager->ArgRegs();
  temp::TempList *list = new temp::TempList;
  for (Exp *exp : exp_list_) {
    temp::Temp *r = exp->Munch(instr_list, fs);
    if (i < 6) {
      instr_list.Append(new assem::MoveInstr(
        "movq `s0,`d0",
        new temp::TempList({arg_temps->NthTemp(i)}),
        new temp::TempList({r})
      ));
      list->Append(arg_temps->NthTemp(i));
    } else {
      instr_list.Append(new assem::OperInstr(
        "movq `s0,"+std::to_string((i-6)*reg_manager->WordSize())+"(`d0)",
        new temp::TempList({reg_manager->StackPointer()}),
        new temp::TempList({r}),
        nullptr
      ));
    }
    ++i;
  }
  return list;
}

} // namespace tree
