#include "tiger/absyn/absyn.h"
#include "tiger/semant/semant.h"

namespace absyn {

//labelcount:
// 0 普通
// 1 在while循环内
// 非0非1 内存有for循环变量的symbol指针

// 具体方法：
// 进入一个while循环时，将labelcount赋为1
// 进入一个for循环时，将labelcount赋为循环变量symbol指针
// 进入一个新的function时，将labelcount赋为0
// 其他情况直接向下传labelcount


void AbsynTree::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                           err::ErrorMsg *errormsg) const {
  root_->SemAnalyze(venv, tenv, 0, errormsg);
}


type::Ty *SimpleVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                long long labelcount, err::ErrorMsg *errormsg) const {
  env::EnvEntry *x = venv->Look(sym_);
  if (x && typeid(*x) == typeid(env::VarEntry)) {
    return ((env::VarEntry *)x)->ty_->ActualTy();
  } else {
    errormsg->Error(pos_, "undefined variable %s", sym_->Name().c_str());
    //该symbol绑定的为function or 该symbol未定义
  }
  return nullptr;
}

type::Ty *FieldVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               long long labelcount, err::ErrorMsg *errormsg) const {

  type::Ty *var = var_->SemAnalyze(venv, tenv, labelcount, errormsg);

  if (var && typeid(*var) == typeid(type::RecordTy)) {
    std::list<type::Field *> list = ((type::RecordTy *)var)->fields_->GetList();

    for (std::list<type::Field *>::iterator it = list.begin(); it != list.end(); ++it) {
      if ((*it)->name_ == sym_) {
        return (*it)->ty_->ActualTy();
      }
    }
    errormsg->Error(pos_, "field %s doesn't exist", sym_->Name().c_str());
  } else if (var) {
    errormsg->Error(pos_, "not a record type");
  }

  return nullptr;
}

type::Ty *SubscriptVar::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                   long long labelcount,
                                   err::ErrorMsg *errormsg) const {
  type::Ty *var = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *exp = subscript_->SemAnalyze(venv, tenv, labelcount, errormsg);

  if (var && exp && typeid(*var) == typeid(type::ArrayTy) && typeid(*exp) == typeid(type::IntTy)) {
    return ((type::ArrayTy *)var)->ty_->ActualTy();
  } else if (var && exp && typeid(*exp) == typeid(type::IntTy)) {
    errormsg->Error(pos_, "array type required");
  } else if (var && exp) {
    errormsg->Error(pos_, "index must be an integer");
  }
  return nullptr;
}

type::Ty *VarExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             long long labelcount, err::ErrorMsg *errormsg) const {
  return (var_->SemAnalyze(venv, tenv, labelcount, errormsg));
}

type::Ty *NilExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             long long labelcount, err::ErrorMsg *errormsg) const {
  return type::NilTy::Instance();
}

type::Ty *IntExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             long long labelcount, err::ErrorMsg *errormsg) const {
  return type::IntTy::Instance();
}

type::Ty *StringExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                long long labelcount, err::ErrorMsg *errormsg) const {
  // fprintf(stderr, "stringExp: %s\n", str_.c_str());
  return type::StringTy::Instance();
}

type::Ty *CallExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              long long labelcount, err::ErrorMsg *errormsg) const {
  // fprintf(stderr, "call %s\n", func_->Name().c_str());
  env::EnvEntry *fun = venv->Look(func_);
  if (fun && typeid(*fun) == typeid(env::FunEntry)) {
    std::list<Exp *> arglist = args_->GetList();
    std::list<type::Ty *> typelist = ((env::FunEntry *)fun)->formals_->GetList();

    std::list<Exp *>::iterator argit = arglist.begin();
    std::list<type::Ty *>::iterator tyit = typelist.begin();
    while (argit != arglist.end()) {
      if (tyit == typelist.end()) {
        errormsg->Error(pos_, "too many params in function %s", func_->Name().c_str());
        return ((env::FunEntry *)fun)->result_->ActualTy();
        //参数个数太多，直接返回
      }
      type::Ty *paramTy = (*argit)->SemAnalyze(venv, tenv, labelcount, errormsg);
      if (!paramTy) return ((env::FunEntry *)fun)->result_->ActualTy();
        //参数exp中有错误，直接返回
      
      if (!paramTy->IsSameType(*tyit)) {
        errormsg->Error(pos_, "para type mismatch");
        return ((env::FunEntry *)fun)->result_->ActualTy();
        //参数exp的类型与函数定义类型不匹配，直接返回
      }
      ++argit; ++tyit;
    }
    if (tyit != typelist.end()) {
      errormsg->Error(pos_, "too little params in function %s", func_->Name().c_str());
        return ((env::FunEntry *)fun)->result_->ActualTy();
        //参数个数太少，直接返回
    }
    return ((env::FunEntry *)fun)->result_->ActualTy(); //无错误，正常返回
  } else {
    errormsg->Error(pos_, "undefined function %s", func_->Name().c_str());
  }
  return type::VoidTy::Instance();
}

type::Ty *OpExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            long long labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *left = left_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *right = right_->SemAnalyze(venv, tenv, labelcount, errormsg);
  // fprintf(stderr, "op:%d\n", oper_);
  if (left && right) {
    switch (oper_)
    {
    case AND_OP:
    case OR_OP:
    case PLUS_OP:
    case MINUS_OP:
    case TIMES_OP:
    case DIVIDE_OP:
      if (typeid(*left) == typeid(type::IntTy) && typeid(*right) == typeid(type::IntTy)) {}
      else {
        errormsg->Error(pos_, "integer required");
      }
      break;
    
    case LT_OP:
    case LE_OP:
    case GT_OP:
    case GE_OP:
      if (!((left->IsSameType(type::IntTy::Instance()) || left->IsSameType(type::StringTy::Instance())) &&
            (right->IsSameType(type::IntTy::Instance()) || right->IsSameType(type::StringTy::Instance())))) {
            errormsg->Error(pos_, "integer/string required");
          }
      else if (!left->IsSameType(right)) {
        errormsg->Error(pos_, "same type required");
      }
    break;

    case EQ_OP:
    case NEQ_OP:
    if (!((left->IsSameType(type::IntTy::Instance()) || left->IsSameType(type::StringTy::Instance()) ||
            typeid(*left) == typeid(type::RecordTy) || typeid(*left) == typeid(type::ArrayTy) || 
            typeid(*left) == typeid(type::NilTy)) &&
          (right->IsSameType(type::IntTy::Instance()) || right->IsSameType(type::StringTy::Instance()) ||
            typeid(*right) == typeid(type::RecordTy) || typeid(*right) == typeid(type::ArrayTy) ||
            typeid(*right) == typeid(type::NilTy)))) {
          errormsg->Error(pos_, "integer/string/record/array required");
        }
    else if (!left->IsSameType(right)) {
      errormsg->Error(pos_, "same type required");
    }
    break;

    default:
      break;
    }
  }
  return type::IntTy::Instance();
}

type::Ty *RecordExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                long long labelcount, err::ErrorMsg *errormsg) const {
  // fprintf(stderr, "start record exp\n");                                
  type::Ty *ty = tenv->Look(typ_);
  if (ty) ty = ty->ActualTy();
  else {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().c_str());
    return nullptr;
  }
  if (ty && typeid(*ty) == typeid(type::RecordTy)) {
    // fprintf(stderr, "record exist\n"); 
    std::list<EField *> efieldList = fields_->GetList();
    std::list<type::Field *> fieldList = ((type::RecordTy *)ty)->fields_->GetList();

    if (efieldList.size() == fieldList.size()) {
      // fprintf(stderr, "size equal\n"); 
      std::list<EField *>::iterator eit = efieldList.begin();
      std::list<type::Field *>::iterator fit = fieldList.begin();
      while (eit != efieldList.end()) {
        if ((*eit)->name_ != (*fit)->name_ || 
          ((*eit)->exp_->SemAnalyze(venv, tenv, labelcount, errormsg)->IsSameType((*fit)->ty_)) == false) {
            errormsg->Error(pos_, "mismatch field name/type");
            return nullptr;
          }
        ++eit; ++fit;
      }
    } else {
      errormsg->Error(pos_, "mismatch field number");
      return nullptr;
    }
    // fprintf(stderr, "type equal\n"); 
    return ty;
  } else {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().c_str());
  }
  // fprintf(stderr, "?????\n");
  return nullptr;
}

type::Ty *SeqExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             long long labelcount, err::ErrorMsg *errormsg) const {
  std::list<Exp *> list = seq_->GetList();
  type::Ty *lastExp;

  for (std::list<Exp *>::iterator it = list.begin(); it != list.end(); ++it) {
    if ((*it) == list.back()) {
      lastExp = (*it)->SemAnalyze(venv, tenv, labelcount, errormsg);
    } else {
      (*it)->SemAnalyze(venv, tenv, labelcount, errormsg);
    }
  }                            
  return lastExp;
}

type::Ty *AssignExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                                long long labelcount, err::ErrorMsg *errormsg) const {

  //检验被赋值的变量是否为循环变量
  if (labelcount != 0 && labelcount != 1 && 
      typeid(*var_) == typeid(SimpleVar) && ((SimpleVar *)var_)->sym_ == (sym::Symbol *)labelcount) {
    errormsg->Error(pos_, "loop variable can't be assigned");
    return type::VoidTy::Instance(); 
  }     
                         
  type::Ty *left = var_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *right = exp_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (left && right && left->IsSameType(right)) {
    return type::VoidTy::Instance();
  } else if (left && right) {
    errormsg->Error(pos_, "unmatched assign exp");
  }
  return type::VoidTy::Instance();
}

type::Ty *IfExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                            long long labelcount, err::ErrorMsg *errormsg) const {
  if (test_ && then_ && elsee_ == nullptr) {
    type::Ty *con = test_->SemAnalyze(venv, tenv, labelcount, errormsg);
    type::Ty *act = then_->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (typeid(*con) == typeid(type::IntTy) && typeid(*act) == typeid(type::VoidTy)) {
      return type::VoidTy::Instance();
    } 
    if (typeid(*con) != typeid(type::IntTy)) {
      errormsg->Error(pos_, "condition exp requires integer");
    }
    if (typeid(*act) != typeid(type::VoidTy)) {
      errormsg->Error(pos_, "if-then exp's body must produce no value");
    }
    
  } else if (test_ && then_ && elsee_) {
    type::Ty *con = test_->SemAnalyze(venv, tenv, labelcount, errormsg);
    type::Ty *act1 = then_->SemAnalyze(venv, tenv, labelcount, errormsg);
    type::Ty *act2 = elsee_->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (typeid(*con) == typeid(type::IntTy) && act1->IsSameType(act2)) {
      return act1->ActualTy();
    } 
    if (typeid(*con) != typeid(type::IntTy)) {
      errormsg->Error(pos_, "condition exp requires integer");
    }
    if (!act1->IsSameType(act2)) {
      errormsg->Error(pos_, "then exp and else exp type mismatch");
    }
  }
  return nullptr;
}

type::Ty *WhileExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               long long labelcount, err::ErrorMsg *errormsg) const {
  labelcount = 1;

  if (test_ && body_) {
    type::Ty *con = test_->SemAnalyze(venv, tenv, labelcount, errormsg);
    type::Ty *act = body_->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (typeid(*con) == typeid(type::IntTy) && typeid(*act) == typeid(type::VoidTy)) {
      return type::VoidTy::Instance();
    } 
    if (typeid(*con) != typeid(type::IntTy)) {
      errormsg->Error(pos_, "condition exp requires integer");
    }
    if (typeid(*act) != typeid(type::VoidTy)) {
      errormsg->Error(pos_, "while body must produce no value");
    }
  }
  return type::VoidTy::Instance();
}

type::Ty *ForExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             long long labelcount, err::ErrorMsg *errormsg) const {
  labelcount =  (long long)var_;                       
                            
  type::Ty *lo = lo_->SemAnalyze(venv, tenv, labelcount, errormsg);
  type::Ty *hi = hi_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!lo->IsSameType(type::IntTy::Instance()) || !hi->IsSameType(type::IntTy::Instance())) {
    errormsg->Error(pos_, "for exp's range type is not integer");
  }
  
  venv->BeginScope();
  venv->Enter(var_, new env::VarEntry(type::IntTy::Instance()));
  type::Ty *body = body_->SemAnalyze(venv, tenv, labelcount, errormsg);
  if (!body->IsSameType(type::VoidTy::Instance())) {
    errormsg->Error(pos_, "for should produce no value");
  }
  venv->EndScope();

  return type::VoidTy::Instance();
}

type::Ty *BreakExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               long long labelcount, err::ErrorMsg *errormsg) const {
  if (!labelcount) {
    errormsg->Error(pos_, "break is not inside any loop");
  }         
  return type::VoidTy::Instance();
}

type::Ty *LetExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             long long labelcount, err::ErrorMsg *errormsg) const {
  tenv->BeginScope();
  venv->BeginScope();
  // fprintf(stderr, "beginscope\n");
  std::list<Dec *> list = decs_->GetList();
  // fprintf(stderr, "getlist\n");
  for (std::list<Dec *>::iterator it = list.begin(); it != list.end(); ++it) {
    (*it)->SemAnalyze(venv, tenv, labelcount, errormsg);
  }
  // fprintf(stderr, "insert params\n");
  type::Ty *ty = body_->SemAnalyze(venv, tenv, labelcount, errormsg);
  // fprintf(stderr, "body\n");
  tenv->EndScope();
  venv->EndScope();
  // fprintf(stderr, "endscope\n");
  return ty;
}

type::Ty *ArrayExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                               long long labelcount, err::ErrorMsg *errormsg) const {
  type::Ty *ty = tenv->Look(typ_);
  if (ty) ty = ty->ActualTy();
  else {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().c_str());
    return nullptr;
  }
  if (ty && typeid(*ty) == typeid(type::ArrayTy)) {
    type::Ty *exp1 = size_->SemAnalyze(venv, tenv, labelcount, errormsg);
    type::Ty *exp2 = init_->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (typeid(*exp1) == typeid(type::IntTy) && exp2->IsSameType(((type::ArrayTy *)ty)->ty_)) {
      return ty;
    } else {
      errormsg->Error(pos_, "type mismatch");
    }
  } else {
    errormsg->Error(pos_, "undefined type %s", typ_->Name().c_str());
  }
  return nullptr;

}

type::Ty *VoidExp::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                              long long labelcount, err::ErrorMsg *errormsg) const {
  return type::VoidTy::Instance();
}

void FunctionDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv,
                             long long labelcount, err::ErrorMsg *errormsg) const {
  std::list<FunDec *> list = functions_->GetList();
  for (std::list<FunDec *>::iterator it = list.begin(); it != list.end(); ++it) {
    // fprintf(stderr, "1: %s\n", (*it)->name_->Name().c_str());

    bool sameName = false;
    for (std::list<FunDec *>::iterator itt = list.begin(); itt != it; ++itt) {
      if (venv->Look((*it)->name_)) {
        errormsg->Error(pos_, "two functions have the same name");
        sameName = true;
        continue;
      }
    }
    if (sameName) continue;
    

    type::TyList *tlist = (*it)->params_->MakeFormalTyList(tenv, errormsg);
    type::Ty *resTy;
    if ((*it)->result_) {
      resTy = tenv->Look((*it)->result_);
      if (!resTy) {
        errormsg->Error(pos_, "undefined type %s", (*it)->result_->Name().c_str());
      } else {
        resTy = resTy->ActualTy();
      }
    }
    else resTy = type::VoidTy::Instance();
    
    venv->Enter((*it)->name_, new env::FunEntry(tlist, resTy));
  }

  for (std::list<FunDec *>::iterator it = list.begin(); it != list.end(); ++it) {
    // fprintf(stderr, "2: %s\n", (*it)->name_->Name().c_str());
    std::list<type::Field *> flist = (*it)->params_->MakeFieldList(tenv, errormsg)->GetList();
    type::Ty *resTy;
    if ((*it)->result_) {
      resTy = tenv->Look((*it)->result_);
      if (resTy) resTy = resTy->ActualTy();
    }
    else resTy = type::VoidTy::Instance();

    venv->BeginScope();
    for (std::list<type::Field *>::iterator fit = flist.begin(); fit != flist.end(); ++fit) {
      venv->Enter((*fit)->name_, new env::VarEntry((*fit)->ty_));
    }
    // fprintf(stderr, "%s", typeid((*it)->body_).name());
    if (resTy) {
      type::Ty *res = (*it)->body_->SemAnalyze(venv, tenv, 0, errormsg);
      if (resTy->IsSameType(type::VoidTy::Instance()) && !res->IsSameType(type::VoidTy::Instance())) {
        errormsg->Error(pos_, "procedure returns value");
      } else if (!res->IsSameType(resTy)) {
        errormsg->Error(pos_, "mismatch function result type");
      }
    }
    
    venv->EndScope();
  }

}

void VarDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, long long labelcount,
                        err::ErrorMsg *errormsg) const {
  if (typ_) {
    // fprintf(stderr, "var dec (result type)\n");
    type::Ty *ty = tenv->Look(typ_);
    if (!ty->IsSameType(init_->SemAnalyze(venv, tenv, labelcount, errormsg))) {
      errormsg->Error(pos_, "type mismatch");
    }
    venv->Enter(var_, new env::VarEntry(ty));
  } else {
    // fprintf(stderr, "var dec (no result type)\n");
    type::Ty *ty = init_->SemAnalyze(venv, tenv, labelcount, errormsg);
    if (ty && typeid(*ty) == typeid(type::NilTy)) {
      errormsg->Error(pos_, "init should not be nil without type specified");
    }
    venv->Enter(var_, new env::VarEntry(ty));
    //如果exp表达式有错，venv中可能存在<symbol, env::VarEntry(nullptr)>
  }
}

void TypeDec::SemAnalyze(env::VEnvPtr venv, env::TEnvPtr tenv, long long labelcount,
                         err::ErrorMsg *errormsg) const {
  std::list<NameAndTy *> list = types_->GetList();
  for (std::list<NameAndTy *>::iterator it = list.begin(); it != list.end(); ++it) {

    bool sameName = false;
    for (std::list<NameAndTy *>::iterator itt = list.begin(); itt != it; ++itt) {
      if ((*itt)->name_ == (*it)->name_) {
        errormsg->Error(pos_, "two types have the same name");
        sameName = true;
        break;
      }
    }
    if (sameName) continue;
    
    // fprintf(stderr, "1: %s\n", (*it)->name_->Name().c_str());
    tenv->Enter((*it)->name_, new type::NameTy((*it)->name_, nullptr));
    
  }
  for (std::list<NameAndTy *>::iterator it = list.begin(); it != list.end(); ++it) {
    // fprintf(stderr, "2: %s\n", (*it)->name_->Name().c_str());
    type::Ty *ty = tenv->Look((*it)->name_);
    ((type::NameTy *)ty)->ty_ = (*it)->ty_->SemAnalyze(tenv, errormsg);
  }

  // 检测type cycle:
  // 遍历typelist中的每一个type，对于每一个nameTy，都在while循环中寻找它的真正Ty
  // 如寻找到它自己，则这是一个type cycle
  // 此时将它自己的Ty置NilTy，打破循环
  for (std::list<NameAndTy *>::iterator it = list.begin(); it != list.end(); ++it) {
    type::Ty *ty = tenv->Look((*it)->name_);
    ty = ((type::NameTy *)ty)->ty_;
    // fprintf(stderr, "new loop\n");
    while (typeid(*ty) == typeid(type::NameTy)) {
      // fprintf(stderr, "type name: %s\n", (*it)->name_->Name().c_str());
      // fprintf(stderr, "symbol: %s\n", ((type::NameTy *)ty)->sym_->Name().c_str());
      
      if (((type::NameTy *)ty)->sym_ == (*it)->name_) {
        // fprintf(stderr, "cycle symbol: %s\n", ((type::NameTy *)ty)->sym_->Name().c_str());
        errormsg->Error(pos_, "illegal type cycle");

        type::Ty *ty = tenv->Look((*it)->name_);
        ((type::NameTy *)ty)->ty_ = type::NilTy::Instance();
        break;
        
      }
      ty = ((type::NameTy *)ty)->ty_;
      // if (typeid(*ty) == typeid(type::NameTy)) 
      //   // fprintf(stderr, "next symbol: %s\n", ((type::NameTy *)ty)->sym_->Name().c_str());
    }
  }
}

type::Ty *NameTy::SemAnalyze(env::TEnvPtr tenv, err::ErrorMsg *errormsg) const {
  type::Ty *ty = tenv->Look(name_);
  if (!ty) {
    errormsg->Error(pos_, "undefined type %s", name_->Name().c_str());
  }
  return new type::NameTy(name_, ty);
  //只报undefined type一个错误，剩下的type checking正常进行
  //如返回nullptr，则剩下都会报错了
  //即：找到一个错误，首先报错，然后修正（不影响下面的检查）
}

type::Ty *RecordTy::SemAnalyze(env::TEnvPtr tenv,
                               err::ErrorMsg *errormsg) const {
  // fprintf(stderr, "create record type\n");
  type::FieldList *list = record_->MakeFieldList(tenv, errormsg);
  return new type::RecordTy(list);
}

type::Ty *ArrayTy::SemAnalyze(env::TEnvPtr tenv,
                              err::ErrorMsg *errormsg) const {
  type::Ty *ty = tenv->Look(array_);
  if (!ty) {
    errormsg->Error(pos_, "undefined type %s", array_->Name().c_str());
  }
  return new type::ArrayTy(ty);
}

} // namespace absyn

namespace sem {

void ProgSem::SemAnalyze() {
  FillBaseVEnv();
  FillBaseTEnv();
  absyn_tree_->SemAnalyze(venv_.get(), tenv_.get(), errormsg_.get());
}

} // namespace tr
