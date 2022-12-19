#include "tiger/translate/translate.h"

#include <tiger/absyn/absyn.h>

#include "tiger/env/env.h"
#include "tiger/errormsg/errormsg.h"
#include "tiger/frame/x64frame.h"
#include "tiger/frame/temp.h"
#include "tiger/frame/frame.h"

extern frame::Frags *frags;
extern frame::RegManager *reg_manager;


namespace tr {

Access *Access::AllocLocal(Level *level, bool escape) {
  frame::Access *access = level->frame_->AllocLocal(escape);
  return new Access(level, access);
}

Level *Level::NewLevel(Level *parent, temp::Label *name, std::list<bool> formals) {
  formals.push_front(true);
  frame::Frame *frame = frame::Frame::NewFrame(name, formals);
  return new Level(frame, parent);
}

class Cx {
public:
  PatchList trues_;
  PatchList falses_;
  tree::Stm *stm_;

  Cx(PatchList trues, PatchList falses, tree::Stm *stm)
      : trues_(trues), falses_(falses), stm_(stm) {}
  Cx() {}
};

class Exp {
public:
  [[nodiscard]] virtual tree::Exp *UnEx() = 0;
  [[nodiscard]] virtual tree::Stm *UnNx() = 0;
  [[nodiscard]] virtual Cx UnCx(err::ErrorMsg *errormsg) = 0;
};

class ExpAndTy {
public:
  tr::Exp *exp_;
  type::Ty *ty_;

  ExpAndTy(tr::Exp *exp, type::Ty *ty) : exp_(exp), ty_(ty) {}
};

class ExExp : public Exp {
public:
  tree::Exp *exp_;

  explicit ExExp(tree::Exp *exp) : exp_(exp) {}

  [[nodiscard]] tree::Exp *UnEx() override { 
    return exp_;
  }
  [[nodiscard]] tree::Stm *UnNx() override {
    return new tree::ExpStm(exp_);
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override {
    tree::CjumpStm *stm = new tree::CjumpStm(tree::NE_OP, exp_, new tree::ConstExp(0), nullptr, nullptr);
    tr::PatchList trues = PatchList({&stm->true_label_});
    tr::PatchList falses = PatchList({&stm->false_label_});
    return Cx(trues, falses, stm);
  }
};

class NxExp : public Exp {
public:
  tree::Stm *stm_;

  explicit NxExp(tree::Stm *stm) : stm_(stm) {}

  [[nodiscard]] tree::Exp *UnEx() override {
    //这个函数在什么情况下会用到？
    //翻译seq时，需要把最后一个exp当作ExExp来处理
    return new tree::EseqExp(stm_, new tree::ConstExp(0));

  }
  [[nodiscard]] tree::Stm *UnNx() override { 
    return stm_;
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override {
    //error
  }
};

class CxExp : public Exp {
public:
  Cx cx_;

  CxExp(PatchList trues, PatchList falses, tree::Stm *stm)
      : cx_(trues, falses, stm) {}
  
  [[nodiscard]] tree::Exp *UnEx() override {
    temp::Temp *r = temp::TempFactory::NewTemp();
    temp::Label *t = temp::LabelFactory::NewLabel();
    temp::Label *f = temp::LabelFactory::NewLabel();
    cx_.trues_.DoPatch(t);
    cx_.falses_.DoPatch(f);

    return 
    new tree::EseqExp(new tree::MoveStm(new tree::TempExp(r), new tree::ConstExp(1)),
      new tree::EseqExp(cx_.stm_,
        new tree::EseqExp(new tree::LabelStm(f),
          new tree::EseqExp(new tree::MoveStm(new tree::TempExp(r), new tree::ConstExp(0)),
            new tree::EseqExp(new tree::LabelStm(t), new tree::TempExp(r))))));
  }
  [[nodiscard]] tree::Stm *UnNx() override {
    //什么情况下会用到这个函数？
    
  }
  [[nodiscard]] Cx UnCx(err::ErrorMsg *errormsg) override { 
    return cx_;
  }
};

void ProgTr::Translate() {
  FillBaseVEnv();
  FillBaseTEnv();
  tr::ExpAndTy *tr_tree = absyn_tree_->Translate(
    venv_.get(), tenv_.get(), main_level_.get(), nullptr, errormsg_.get()
  );
  // tr_tree->exp_->UnNx()->Print(stderr, 0);
}

} // namespace tr

namespace absyn {

tr::ExpAndTy *AbsynTree::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {                             
  return root_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *SimpleVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  env::VarEntry *var = (env::VarEntry *)venv->Look(sym_);     
  type::Ty *type = var->ty_;
  tree::Exp *retExp = new tree::TempExp(reg_manager->FramePointer());
  
  tr::Level *lv = level;
  while (var->access_->level_ != lv) {
    frame::Access *sl = lv->frame_->formals->front();
    retExp = sl->ToExp(retExp);
    lv = lv->parent_;
  }
  retExp = var->access_->access_->ToExp(retExp);

  return new tr::ExpAndTy(new tr::ExExp(retExp), type->ActualTy());
}

tr::ExpAndTy *FieldVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  type::Ty *type;
  tree::Exp *retExp;

  tr::ExpAndTy *base = var_->Translate(venv, tenv, level, label, errormsg);
  int i = 0;
  for (type::Field *field : ((type::RecordTy *)base->ty_)->fields_->GetList()) {
    if (field->name_ == sym_) {
      type = field->ty_;
      retExp = 
      new tree::MemExp(
        new tree::BinopExp(tree::PLUS_OP, new tree::MemExp(base->exp_->UnEx()),
          new tree::ConstExp(i * reg_manager->WordSize())));
      break;
    } 
    ++i;
  }

  return new tr::ExpAndTy(new tr::ExExp(retExp), type->ActualTy());
}

tr::ExpAndTy *SubscriptVar::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                      tr::Level *level, temp::Label *label,
                                      err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *base = var_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *exp = subscript_->Translate(venv, tenv, level, label, errormsg);

  tree::Exp *retExp = 
  new tree::MemExp(
    new tree::BinopExp(tree::PLUS_OP, new tree::MemExp(base->exp_->UnEx()),
      new tree::BinopExp(tree::MUL_OP, exp->exp_->UnEx(), new tree::ConstExp(reg_manager->WordSize()))));
  
  return new tr::ExpAndTy(new tr::ExExp(retExp), base->ty_->ActualTy());
}

tr::ExpAndTy *VarExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  return var_->Translate(venv, tenv, level, label, errormsg);
}

tr::ExpAndTy *NilExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  //这样处理可以吗？
  return new tr::ExpAndTy(
    new tr::ExExp(new tree::ConstExp(0)), type::NilTy::Instance()
  );
}

tr::ExpAndTy *IntExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  return new tr::ExpAndTy(
    new tr::ExExp(new tree::ConstExp(val_)), type::IntTy::Instance()
  );
}

tr::ExpAndTy *StringExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,
                                   err::ErrorMsg *errormsg) const {
  temp::Label *lab = temp::LabelFactory::NewLabel();
  frame::StringFrag *str = new frame::StringFrag(lab, str_);
  frags->PushBack(str);
  return new tr::ExpAndTy(new tr::ExExp(new tree::NameExp(lab)), type::StringTy::Instance());
}

tr::ExpAndTy *CallExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  env::FunEntry *func = (env::FunEntry *)venv->Look(func_);

  //建立static link
  tree::Exp *sl = new tree::TempExp(reg_manager->FramePointer());
  tr::Level *lv = level;
  while (lv != func->level_->parent_) {
    sl = lv->frame_->formals->front()->ToExp(sl);
    lv = lv->parent_;
  }

  //翻译arg
  tree::ExpList *args = new tree::ExpList;
  args->Append(sl);
  for (Exp *arg : args_->GetList()) {
    tr::ExpAndTy *x = arg->Translate(venv, tenv, level, label, errormsg);
    args->Append(x->exp_->UnEx());
  }

  tr::Exp *ret = new tr::ExExp(
    new tree::CallExp(new tree::NameExp(func_), args)
  );
  return new tr::ExpAndTy(ret, func->result_->ActualTy());
}

tr::ExpAndTy *OpExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  //还有string comparison!!!
  tr::ExpAndTy *left = left_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *right = right_->Translate(venv, tenv, level, label, errormsg);

  tr::Exp *ret;
  tree::CjumpStm *stm;
  tr::PatchList trues, falses;
  tr::Cx left_cx, right_cx;

  switch (oper_)
  {
  case PLUS_OP:
    ret = new tr::ExExp(
      new tree::BinopExp(tree::PLUS_OP, left->exp_->UnEx(), right->exp_->UnEx())
    );
    break;
  case MINUS_OP:
    ret = new tr::ExExp(
      new tree::BinopExp(tree::MINUS_OP, left->exp_->UnEx(), right->exp_->UnEx())
    );
    break;
  case TIMES_OP:
    ret = new tr::ExExp(
      new tree::BinopExp(tree::MUL_OP, left->exp_->UnEx(), right->exp_->UnEx())
    );
    break;
  case DIVIDE_OP:
    ret = new tr::ExExp(
      new tree::BinopExp(tree::DIV_OP, left->exp_->UnEx(), right->exp_->UnEx())
    );
    break;

  case LT_OP:
    stm = new tree::CjumpStm(tree::LT_OP, left->exp_->UnEx(), right->exp_->UnEx(), nullptr, nullptr);
    trues = tr::PatchList({&stm->true_label_});
    falses = tr::PatchList({&stm->false_label_});
    ret = new tr::CxExp(trues, falses, stm);
    break;
  case LE_OP:
    stm = new tree::CjumpStm(tree::LE_OP, left->exp_->UnEx(), right->exp_->UnEx(), nullptr, nullptr);
    trues = tr::PatchList({&stm->true_label_});
    falses = tr::PatchList({&stm->false_label_});
    ret = new tr::CxExp(trues, falses, stm);
    break;
  case GT_OP:
    stm = new tree::CjumpStm(tree::GT_OP, left->exp_->UnEx(), right->exp_->UnEx(), nullptr, nullptr);
    trues = tr::PatchList({&stm->true_label_});
    falses = tr::PatchList({&stm->false_label_});
    ret = new tr::CxExp(trues, falses, stm);
    break;
  case GE_OP:
    stm = new tree::CjumpStm(tree::GE_OP, left->exp_->UnEx(), right->exp_->UnEx(), nullptr, nullptr);
    trues = tr::PatchList({&stm->true_label_});
    falses = tr::PatchList({&stm->false_label_});
    ret = new tr::CxExp(trues, falses, stm);
    break;
  case EQ_OP:
    stm = new tree::CjumpStm(tree::EQ_OP, left->exp_->UnEx(), right->exp_->UnEx(), nullptr, nullptr);
    trues = tr::PatchList({&stm->true_label_});
    falses = tr::PatchList({&stm->false_label_});
    ret = new tr::CxExp(trues, falses, stm);
    break;
  case NEQ_OP:
    stm = new tree::CjumpStm(tree::NE_OP, left->exp_->UnEx(), right->exp_->UnEx(), nullptr, nullptr);
    trues = tr::PatchList({&stm->true_label_});
    falses = tr::PatchList({&stm->false_label_});
    ret = new tr::CxExp(trues, falses, stm);
    break;

  case AND_OP: {
    left_cx = left->exp_->UnCx(errormsg);
    right_cx = right->exp_->UnCx(errormsg);
    temp::Label *z = temp::LabelFactory::NewLabel();
    left_cx.trues_.DoPatch(z);
    trues = right_cx.trues_;
    falses = tr::PatchList::JoinPatch(left_cx.falses_, right_cx.falses_);
    ret = new tr::CxExp(trues, falses, 
      new tree::SeqStm(left_cx.stm_, new tree::SeqStm(new tree::LabelStm(z), right_cx.stm_)));
    break;
  }
  case OR_OP: {
    left_cx = left->exp_->UnCx(errormsg);
    right_cx = right->exp_->UnCx(errormsg);
    temp::Label *z = temp::LabelFactory::NewLabel();
    left_cx.falses_.DoPatch(z);
    trues = tr::PatchList::JoinPatch(left_cx.trues_, right_cx.trues_);
    falses = right_cx.falses_;
    ret = new tr::CxExp(trues, falses, 
      new tree::SeqStm(left_cx.stm_, new tree::SeqStm(new tree::LabelStm(z), right_cx.stm_)));
    break;
  }

  default:
    break;
  }

  return new tr::ExpAndTy(ret, type::IntTy::Instance());
}

tr::ExpAndTy *RecordExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,      
                                   err::ErrorMsg *errormsg) const {
  temp::Temp *r = temp::TempFactory::NewTemp();
  tree::Stm *ret;

  int sz = fields_->GetList().size();
  if (sz) {
    //创建的record需要初始化
    //将初始化的exp全部放入一个vector中
    std::vector<tr::Exp *> exp_vec;
    for (EField *efield : fields_->GetList()) {
      tr::ExpAndTy *tr_exp = efield->exp_->Translate(venv, tenv, level, label, errormsg);
      exp_vec.push_back(tr_exp->exp_);
    }
    //用最后一个exp组装move语句
    ret = new tree::MoveStm(
      new tree::MemExp(
        new tree::BinopExp(tree::PLUS_OP, new tree::TempExp(r), new tree::ConstExp((sz - 1)*reg_manager->WordSize()))),
      exp_vec[sz - 1]->UnEx()
    );
    //从倒数第二个exp开始，依次向前用seq连接新的move和原来的语句
    for (int i = sz - 2; i >= 0; --i) {
      ret = new tree::SeqStm(
        new tree::MoveStm(
          new tree::MemExp(
            new tree::BinopExp(tree::PLUS_OP, new tree::TempExp(r), new tree::ConstExp(i*reg_manager->WordSize()))),
          exp_vec[i]->UnEx()
        ),
        ret
      );
    }
    //最后添加实现malloc的语句
    ret = new tree::SeqStm(
      new tree::MoveStm(
        new tree::TempExp(r), 
        frame::ExternalCall("malloc", new tree::ExpList({new tree::ConstExp(sz * reg_manager->WordSize())}))
      ),
      ret
    );
  } else {
    //创建的record不需要初始化
    //直接malloc
    ret = new tree::MoveStm(
      new tree::TempExp(r),
      frame::ExternalCall("malloc", new tree::ExpList({new tree::ConstExp(sz * reg_manager->WordSize())}))
    );
  }

  return new tr::ExpAndTy(
    new tr::ExExp(new tree::EseqExp(ret, new tree::TempExp(r))),
    tenv->Look(typ_)->ActualTy()
  );
}

tr::ExpAndTy *SeqExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  type::Ty *type = nullptr;

  std::vector<tr::Exp *> exp_v;
  for (Exp *exp : seq_->GetList()) {
    tr::ExpAndTy *tr_exp = exp->Translate(venv, tenv, level, label, errormsg);
    exp_v.push_back(tr_exp->exp_);
    if (exp == seq_->GetList().back()) {
      type = tr_exp->ty_->ActualTy();
    }
  }

  int sz = exp_v.size();
  if (sz == 1) {
    return new tr::ExpAndTy(exp_v[0], type); //需要特意将其改成ExExp吗？
  } else {
    tree::EseqExp *ret = new tree::EseqExp(exp_v[sz-2]->UnNx(), exp_v[sz-1]->UnEx());
    for (int i = sz-3; i >= 0; --i) {
      ret = new tree::EseqExp(exp_v[i]->UnNx(), ret);
    }
    return new tr::ExpAndTy(new tr::ExExp(ret), type);
  }
}

tr::ExpAndTy *AssignExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   tr::Level *level, temp::Label *label,                       
                                   err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *var = var_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *exp = exp_->Translate(venv, tenv, level, label, errormsg);
  tr::Exp *ret = new tr::NxExp(
    new tree::MoveStm(var->exp_->UnEx(), exp->exp_->UnEx())
  );
  return new tr::ExpAndTy(ret, type::VoidTy::Instance());
}

tr::ExpAndTy *IfExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                               tr::Level *level, temp::Label *label,
                               err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *test = test_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *then = then_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *elsee;
  if (elsee_) elsee = elsee_->Translate(venv, tenv, level, label, errormsg);
  tr::Exp *ret;

  tr::Cx test_cx = test->exp_->UnCx(errormsg);
  temp::Label *t = temp::LabelFactory::NewLabel();
  temp::Label *f = temp::LabelFactory::NewLabel();
  temp::Label *joint = temp::LabelFactory::NewLabel();
  test_cx.trues_.DoPatch(t);
  test_cx.falses_.DoPatch(f);

  if (elsee_) { // 有else语句
    if (then->ty_->IsSameType(type::VoidTy::Instance()) && elsee->ty_->IsSameType(type::VoidTy::Instance())) {
      //exp无值的情况
      ret = new tr::NxExp(
        new tree::SeqStm(test_cx.stm_,
        new tree::SeqStm(new tree::LabelStm(t),
        new tree::SeqStm(then->exp_->UnNx(),
        new tree::SeqStm(new tree::JumpStm(new tree::NameExp(joint), nullptr),
        new tree::SeqStm(new tree::LabelStm(f),
        new tree::SeqStm(elsee->exp_->UnNx(),
        new tree::LabelStm(joint)))))))
      );
      return new tr::ExpAndTy(ret, type::VoidTy::Instance());     
    } else if (then->ty_->IsSameType(elsee->ty_)) {
      //exp有值的情况
      temp::Temp *r = temp::TempFactory::NewTemp();
      ret = new tr::ExExp(
        new tree::EseqExp(test_cx.stm_,
        new tree::EseqExp(new tree::LabelStm(t),
        new tree::EseqExp(new tree::MoveStm(new tree::TempExp(r), then->exp_->UnEx()),
        new tree::EseqExp(new tree::JumpStm(new tree::NameExp(joint), nullptr), //JumpStm的第二个参数会用到吗？
        new tree::EseqExp(new tree::LabelStm(f),
        new tree::EseqExp(new tree::MoveStm(new tree::TempExp(r), elsee->exp_->UnEx()),
        new tree::EseqExp(new tree::LabelStm(joint),
        new tree::TempExp(r))))))))
      );
      return new tr::ExpAndTy(ret, then->ty_->ActualTy());
    }
  } 
  else {
    //无else的情况，exp只能为无值
    ret = new tr::NxExp(
      new tree::SeqStm(test_cx.stm_,
      new tree::SeqStm(new tree::LabelStm(t),
      new tree::SeqStm(then->exp_->UnNx(),
      new tree::LabelStm(f))))
    );
    return new tr::ExpAndTy(ret, type::VoidTy::Instance());
  }
  fprintf(stderr, "一个正确的程序不该执行到这里1\n");
}

tr::ExpAndTy *WhileExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,            
                                  err::ErrorMsg *errormsg) const {                                  
  temp::Label *loop = temp::LabelFactory::NewLabel();
  temp::Label *not_done = temp::LabelFactory::NewLabel();
  temp::Label *done = temp::LabelFactory::NewLabel();

  tr::ExpAndTy *test = test_->Translate(venv, tenv, level, done, errormsg);
  tr::ExpAndTy *body = body_->Translate(venv, tenv, level, done, errormsg);

  tr::Cx test_cx = test->exp_->UnCx(errormsg);
  test_cx.trues_.DoPatch(not_done);
  test_cx.falses_.DoPatch(done);  

  tr::Exp *ret = new tr::NxExp(
    new tree::SeqStm(new tree::LabelStm(loop),
    new tree::SeqStm(test_cx.stm_,
    new tree::SeqStm(new tree::LabelStm(not_done),
    new tree::SeqStm(body->exp_->UnNx(),
    new tree::SeqStm(new tree::JumpStm(new tree::NameExp(loop), nullptr),
    new tree::LabelStm(done))))))
  );  
  return new tr::ExpAndTy(ret, type::VoidTy::Instance());
}

tr::ExpAndTy *ForExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *lo = lo_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *hi = hi_->Translate(venv, tenv, level, label, errormsg);
  
  venv->BeginScope();

  //定义循环变量，并将其加入venv中
  tr::Access *access = tr::Access::AllocLocal(level, escape_);
  venv->Enter(var_, new env::VarEntry(access, type::IntTy::Instance()));

  temp::Label *loop = temp::LabelFactory::NewLabel();
  temp::Label *not_done = temp::LabelFactory::NewLabel();
  temp::Label *done = temp::LabelFactory::NewLabel();

  tr::ExpAndTy *body = body_->Translate(venv, tenv, level, done, errormsg);

  tree::Exp *var = access->access_->ToExp(new tree::TempExp(reg_manager->FramePointer()));
  tr::Exp *ret = new tr::NxExp(
    new tree::SeqStm(new tree::MoveStm(var, lo->exp_->UnEx()),
    new tree::SeqStm(new tree::CjumpStm(tree::GT_OP, var, hi->exp_->UnEx(), done, not_done),
    new tree::SeqStm(new tree::LabelStm(not_done),
    new tree::SeqStm(body->exp_->UnNx(),
    new tree::SeqStm(new tree::CjumpStm(tree::EQ_OP, var, hi->exp_->UnEx(), done, loop),
    new tree::SeqStm(new tree::LabelStm(loop),
    new tree::SeqStm(new tree::MoveStm(var, new tree::BinopExp(tree::PLUS_OP, var, new tree::ConstExp(1))),
    new tree::SeqStm(body->exp_->UnNx(),
    new tree::SeqStm(new tree::CjumpStm(tree::LT_OP, var, hi->exp_->UnEx(), loop, done),
    new tree::LabelStm(done))))))))))
  );

  venv->EndScope();

  return new tr::ExpAndTy(ret, type::VoidTy::Instance());
}

tr::ExpAndTy *BreakExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,
                                  err::ErrorMsg *errormsg) const {
  return new tr::ExpAndTy(
    new tr::NxExp(new tree::JumpStm(new tree::NameExp(label), nullptr)),
    type::VoidTy::Instance()
  );
}

tr::ExpAndTy *LetExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  tenv->BeginScope();
  venv->BeginScope();

  //翻译Dec，修改tenv/venv，并生成初始化语句
  std::vector<tr::Exp *> init;
  for (Dec *decs : decs_->GetList()) {
    tr::Exp *stm = decs->Translate(venv, tenv, level, label, errormsg);
    init.push_back(stm);
  }

  tr::ExpAndTy *body = body_->Translate(venv, tenv, level, label, errormsg);
  //将初始化语句添加到body开头
  tree::Exp *ret = body->exp_->UnEx();
  for (int i = init.size() - 1; i >= 0; --i) {
    ret = new tree::EseqExp(init[i]->UnNx(), ret);
  }

  tenv->EndScope();
  venv->EndScope();

  return new tr::ExpAndTy(new tr::ExExp(ret), body->ty_->ActualTy());

}

tr::ExpAndTy *ArrayExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                  tr::Level *level, temp::Label *label,                    
                                  err::ErrorMsg *errormsg) const {
  tr::ExpAndTy *size = size_->Translate(venv, tenv, level, label, errormsg);
  tr::ExpAndTy *init = init_->Translate(venv, tenv, level, label, errormsg);
  tr::Exp *ret = new tr::ExExp(
    frame::ExternalCall("init_array", new tree::ExpList({size->exp_->UnEx(), init->exp_->UnEx()}))
  );
  return new tr::ExpAndTy(ret, tenv->Look(typ_)->ActualTy());
}

tr::ExpAndTy *VoidExp::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                 tr::Level *level, temp::Label *label,
                                 err::ErrorMsg *errormsg) const {
  return new tr::ExpAndTy(
    new tr::NxExp(new tree::ExpStm(new tree::ConstExp(0))), type::VoidTy::Instance()
  );
}

tr::Exp *FunctionDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                                tr::Level *level, temp::Label *label,
                                err::ErrorMsg *errormsg) const {
  //为每一个FunDec创建新的frame，并将该function的信息添加到venv中                                
  for (FunDec *fundec : functions_->GetList()) {
    std::list<bool> params;
    for (Field *field : fundec->params_->GetList()) {
      params.push_back(field->escape_);
    }
    tr::Level *new_level = tr::Level::NewLevel(level, fundec->name_, params);
    type::TyList *tlist = fundec->params_->MakeFormalTyList(tenv, errormsg);
    type::Ty *resTy = tenv->Look(fundec->result_);
    if (!resTy) resTy = type::VoidTy::Instance();

    venv->Enter(fundec->name_, new env::FunEntry(new_level, fundec->name_, tlist, resTy));
  }

  //开始翻译每一个fundec，生成最终的代码段和经过修改的frame
  for (FunDec *fundec : functions_->GetList()) {
    venv->BeginScope();
    //将函数的参数添加到venv中
    env::FunEntry *fun = (env::FunEntry *)venv->Look(fundec->name_);
    std::list<type::Field *> flist = fundec->params_->MakeFieldList(tenv, errormsg)->GetList();
    auto ait = fun->level_->frame_->formals->begin();
    auto fit = flist.begin();
    for (; ait != fun->level_->frame_->formals->end() && fit != flist.end(); ++ait,++fit) {
      venv->Enter((*fit)->name_, new env::VarEntry(new tr::Access(fun->level_, (*ait)),(*fit)->ty_));
    }
    //翻译函数体
    tr::ExpAndTy *body = fundec->body_->Translate(venv, tenv, fun->level_, label, errormsg);
    //这里应当有一系列的FunEntryExit

    //move返回值
    tree::Stm *ret;
    if (fundec->result_) {
      ret = new tree::MoveStm(new tree::TempExp(reg_manager->ReturnValue()), body->exp_->UnEx());
    } else {
      ret = body->exp_->UnNx();
    }

    //将ProFrag添加到frags中
    frags->PushBack(new frame::ProcFrag(ret, fun->level_->frame_));

    venv->EndScope();
  }

  return new tr::ExExp(new tree::ConstExp(0));
}


tr::Exp *VarDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                           tr::Level *level, temp::Label *label,
                           err::ErrorMsg *errormsg) const {
  tr::Access *access = tr::Access::AllocLocal(level, escape_);
  venv->Enter(var_, new env::VarEntry(access, tenv->Look(typ_)));
  tr::ExpAndTy *init = init_->Translate(venv, tenv, level, label, errormsg);
  tree::MoveStm *ret = new tree::MoveStm(
    access->access_->ToExp(new tree::TempExp(reg_manager->FramePointer())), 
    init->exp_->UnEx()
  );
  return new tr::NxExp(ret);
}

tr::Exp *TypeDec::Translate(env::VEnvPtr venv, env::TEnvPtr tenv,
                            tr::Level *level, temp::Label *label,
                            err::ErrorMsg *errormsg) const {
  for (NameAndTy *name_ty : types_->GetList()) {
    tenv->Enter(name_ty->name_, new type::NameTy(name_ty->name_, nullptr));
  }
  for (NameAndTy *name_ty : types_->GetList()) {
    type::Ty *ty = tenv->Look(name_ty->name_);
    ((type::NameTy *)ty)->ty_ = name_ty->ty_->Translate(tenv, errormsg);
  }
  return new tr::ExExp(new tree::ConstExp(0));
}

type::Ty *NameTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  type::Ty *ty = tenv->Look(name_);
  return new type::NameTy(name_, ty);
}

type::Ty *RecordTy::Translate(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  type::FieldList *list = record_->MakeFieldList(tenv, errormsg);
  return new type::RecordTy(list);
}

type::Ty *ArrayTy::Translate(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  type::Ty *ty = tenv->Look(array_);
  return new type::ArrayTy(ty);
}

} // namespace absyn
