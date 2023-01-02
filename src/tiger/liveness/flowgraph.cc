#include "tiger/liveness/flowgraph.h"

namespace fg {

void FlowGraphFactory::AssemFlowGraph() {
  for (assem::Instr *instr : instr_list_->GetList()) {
    FNode *new_node = flowgraph_->NewNode(instr);
    if (typeid(*instr) == typeid(assem::LabelInstr)) {
      label_map_->Enter(((assem::LabelInstr *)instr)->label_, new_node);
    }
  }

  bool jump_next = true;
  FNode *last_node = nullptr;
  for (FNode *node : flowgraph_->Nodes()->GetList()) {
    if (jump_next && last_node) {
      flowgraph_->AddEdge(last_node, node);
    }

    assem::Instr *instr = node->NodeInfo();
    if (typeid(*instr) == typeid(assem::OperInstr) && ((assem::OperInstr *)instr)->jumps_) {
      for (temp::Label *jump : *((assem::OperInstr *)instr)->jumps_->labels_) {
        FNode *node2 = label_map_->Look(jump);
        assert(node2);
        flowgraph_->AddEdge(node, node2);
      }
      jump_next = false;
      continue;
    }

    jump_next = true;
    last_node = node;
  }

}

} // namespace fg

namespace assem {

temp::TempList *LabelInstr::Def() const {
  return nullptr;
}

temp::TempList *MoveInstr::Def() const {
  return dst_;
}

temp::TempList *OperInstr::Def() const {
  return dst_;
}

temp::TempList *LabelInstr::Use() const {
  return nullptr;
}

temp::TempList *MoveInstr::Use() const {
  return src_;
}

temp::TempList *OperInstr::Use() const {
  return src_;
}
} // namespace assem
