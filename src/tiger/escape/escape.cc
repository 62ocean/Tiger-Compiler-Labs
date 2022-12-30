#include "tiger/escape/escape.h"
#include "tiger/absyn/absyn.h"

namespace esc {
void EscFinder::FindEscape() { absyn_tree_->Traverse(env_.get()); }
} // namespace esc

namespace absyn {

void AbsynTree::Traverse(esc::EscEnvPtr env) {
  root_->Traverse(env, 1);
}

void SimpleVar::Traverse(esc::EscEnvPtr env, int depth) {
  esc::EscapeEntry *x = env->Look(sym_);
  // fprintf(stderr, "encounter var: %s\n", sym_->Name().c_str()); 
  if (x->depth_ < depth) {
    *(x->escape_) = true;
  }
  // fprintf(stderr, "%d %d %d\n", x->depth_, depth, ); 
}

void FieldVar::Traverse(esc::EscEnvPtr env, int depth) {
  // fprintf(stderr, "field var\n"); 
  var_->Traverse(env, depth);
}

void SubscriptVar::Traverse(esc::EscEnvPtr env, int depth) {
  // fprintf(stderr, "sub var\n"); 
  var_->Traverse(env, depth);
}

void VarExp::Traverse(esc::EscEnvPtr env, int depth) {
  var_->Traverse(env, depth);
}

void NilExp::Traverse(esc::EscEnvPtr env, int depth) {
  return;
}

void IntExp::Traverse(esc::EscEnvPtr env, int depth) {
  // fprintf(stderr, "int %d\n", val_); 
  return;
}

void StringExp::Traverse(esc::EscEnvPtr env, int depth) {
  return;
}

void CallExp::Traverse(esc::EscEnvPtr env, int depth) {
  // fprintf(stderr, "call\n"); 
  for (absyn::Exp *exp : args_->GetList()) {
    exp->Traverse(env, depth);
  }
  // fprintf(stderr, "end call\n"); 
}

void OpExp::Traverse(esc::EscEnvPtr env, int depth) {
  // fprintf(stderr, "op\n"); 
  left_->Traverse(env, depth);
  right_->Traverse(env, depth);
  // fprintf(stderr, "end op\n"); 
}

void RecordExp::Traverse(esc::EscEnvPtr env, int depth) {
  for (absyn::EField *efield : fields_->GetList()) {
    efield->exp_->Traverse(env, depth);
  }
}

void SeqExp::Traverse(esc::EscEnvPtr env, int depth) {
  // fprintf(stderr, "seq\n"); 
  for (absyn::Exp *exp : seq_->GetList()) {
    // fprintf(stderr, "111111\n"); 
    exp->Traverse(env, depth);
  }
  // fprintf(stderr, "end seq\n"); 
}

void AssignExp::Traverse(esc::EscEnvPtr env, int depth) {
  // fprintf(stderr, "assign\n"); 
  var_->Traverse(env, depth);
  exp_->Traverse(env, depth);
}

void IfExp::Traverse(esc::EscEnvPtr env, int depth) {
  // fprintf(stderr, "if\n"); 
  test_->Traverse(env, depth);
  then_->Traverse(env, depth);
  if (elsee_) elsee_->Traverse(env, depth);
}

void WhileExp::Traverse(esc::EscEnvPtr env, int depth) {
  // fprintf(stderr, "while\n"); 
  test_->Traverse(env, depth);
  body_->Traverse(env, depth);
}

void ForExp::Traverse(esc::EscEnvPtr env, int depth) {
  lo_->Traverse(env, depth);
  hi_->Traverse(env, depth);

  env->BeginScope();
  env->Enter(var_, new esc::EscapeEntry(depth, &escape_));
  escape_ = false;
  body_->Traverse(env, depth);
  env->EndScope();
}

void BreakExp::Traverse(esc::EscEnvPtr env, int depth) {
  return;
}

void LetExp::Traverse(esc::EscEnvPtr env, int depth) {
  env->BeginScope();
  for (absyn::Dec *dec : decs_->GetList()) {
    dec->Traverse(env, depth);
  }
  body_->Traverse(env, depth);
  env->EndScope();
}

void ArrayExp::Traverse(esc::EscEnvPtr env, int depth) {
  size_->Traverse(env, depth);
  init_->Traverse(env, depth);
}

void VoidExp::Traverse(esc::EscEnvPtr env, int depth) {
  return;
}

void FunctionDec::Traverse(esc::EscEnvPtr env, int depth) {
  for (absyn::FunDec *funDec : functions_->GetList()) {
    // fprintf(stderr, "funcdec\n"); 
    env->BeginScope();
    for (absyn::Field *field : funDec->params_->GetList()) {
      env->Enter(field->name_, new esc::EscapeEntry(depth+1, &(field->escape_)));
      field->escape_ = false;
    }
    funDec->body_->Traverse(env, depth+1);
    // fprintf(stderr, "before endcsope\n"); 
    env->EndScope();
    // fprintf(stderr, "end funcdec\n"); 
  }
}

void VarDec::Traverse(esc::EscEnvPtr env, int depth) {
  init_->Traverse(env, depth);
  env->Enter(var_, new esc::EscapeEntry(depth, &escape_));
  escape_ = false;
}

void TypeDec::Traverse(esc::EscEnvPtr env, int depth) {
  return;
}

} // namespace absyn
