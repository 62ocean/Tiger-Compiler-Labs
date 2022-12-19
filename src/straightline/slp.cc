#include "straightline/slp.h"

#include <iostream>

namespace A {
int A::CompoundStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  return std::max(this->stm1->MaxArgs(), this->stm2->MaxArgs());
}

Table *A::CompoundStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  // printf("compoundstm\n");
  Table *env = t;
  env = stm1->Interp(env);
  env = stm2->Interp(env);
  // printf("compoundstm\n");
  return env;
}

int A::AssignStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  return this->exp->MaxArgs();
}

Table *A::AssignStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  // printf("assignstm\n");
  Table *env = t;
  IntAndTable expResult = exp->Interp(env);
  // std::cout << id << ' ' << expResult.i << std::endl;
  env = expResult.t;
  env = env->Update(id, expResult.i);
  // printf("assignstm\n");
  // env->PrintTable();
  return env;
}

int A::PrintStm::MaxArgs() const {
  // TODO: put your code here (lab1).
  return std::max(exps->MaxArgs(), exps->expNum);
}

Table *A::PrintStm::Interp(Table *t) const {
  // TODO: put your code here (lab1).
  // printf("printstm\n");
  Table *env = t;
  for (ExpList *p = exps; p != nullptr; p = p->getNext()) {
    IntAndTable expResult = p->Interp(env);
    env = expResult.t;
    printf("%d ",expResult.i);
  }
  printf("\n");
  // printf("printstm\n");
  return env;
}

int A::IdExp::MaxArgs() const {
  return 0;
}
IntAndTable A::IdExp::Interp(Table *t) const {
  // printf("idexp\n");
  return IntAndTable(t->Lookup(id), t);
}

int A::NumExp::MaxArgs() const {
  return 0;
}

IntAndTable A::NumExp::Interp(Table *t) const {
  // printf("numexp\n");
  return IntAndTable(num, t);
}

int A::OpExp::MaxArgs() const {
  return std::max(this->left->MaxArgs(), this->right->MaxArgs());
}
IntAndTable A::OpExp::Interp(Table *t) const {
  // printf("opexp\n");
  Table *env = t;
  IntAndTable expResult1 = left->Interp(env);
  env = expResult1.t;
  IntAndTable expResult2 = right->Interp(env);
  env = expResult2.t;
  // printf("opexp\n");
  switch (oper)
  {
  case 0:
    return IntAndTable(expResult1.i + expResult2.i, env);
    break;
  case 1:
    return IntAndTable(expResult1.i - expResult2.i, env);
    break;
  case 2:
    return IntAndTable(expResult1.i * expResult2.i, env);
    break;
  case 3:
    return IntAndTable(expResult1.i / expResult2.i, env);
    break;

  default:
    break;
  }
}

int A::EseqExp::MaxArgs() const {
  return std::max(this->stm->MaxArgs(), this->exp->MaxArgs());
}

IntAndTable A::EseqExp::Interp(Table *t) const {
  // printf("eseqexp\n");
  Table *env = t;
  env = stm->Interp(env);
  return exp->Interp(env);
}

int A::PairExpList::MaxArgs() const {
  return std::max(exp->MaxArgs(), tail->MaxArgs());
}
IntAndTable A::PairExpList::Interp(Table *t) const {
  // printf("pairexplist\n");
  return exp->Interp(t);
}
ExpList *A::PairExpList::getNext() const {
  return tail;
}

int A::LastExpList::MaxArgs() const {
  return exp->MaxArgs();
}
IntAndTable A::LastExpList::Interp(Table *t) const {
  // printf("lastexplist\n");
  return exp->Interp(t);
}
ExpList *A::LastExpList::getNext() const {
  return nullptr;
}


int Table::Lookup(const std::string &key) const {
  if (id == key) {
    return value;
  } else if (tail != nullptr) {
    return tail->Lookup(key);
  } else {
    assert(false);
  }
}

Table *Table::Update(const std::string &key, int val) const {
  return new Table(key, val, this);
}

void Table::PrintTable() const {
  for (Table *p = (Table *)this; p != nullptr; p = (Table *)p->tail) {
    std::cout << "key" << ' ' << p->id << ' ' << "value" << ' ' << p->value << std::endl;
  }
  printf("end\n");
}
}  // namespace A
