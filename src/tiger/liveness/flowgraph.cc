#include "tiger/liveness/flowgraph.h"

extern frame::RegManager *reg_manager;

namespace fg {

void FlowGraphFactory::AssemFlowGraph() {
  for (assem::Instr *instr : instr_list_->GetList()) {
    FNode *new_node = flowgraph_->NewNode(instr);
    if (typeid(*instr) == typeid(assem::LabelInstr)) {
      label_map_->Enter(((assem::LabelInstr *)instr)->label_, new_node);
    }
  }
  // fprintf(stderr, "build node ok\n");

  bool jump_next = true;
  FNode *last_node = nullptr;
  for (FNode *node : flowgraph_->Nodes()->GetList()) {
    if (jump_next && last_node) {
      flowgraph_->AddEdge(last_node, node);
      // fprintf(stderr, "add last edge ok\n");
    }
    // fprintf(stderr, "node %d\n", node->Key());
    temp::Map *color = temp::Map::LayerMap(reg_manager->temp_map_, temp::Map::Name());
    // node->NodeInfo()->Print(stderr, color);

    assem::Instr *instr = node->NodeInfo();
    if (typeid(*instr) == typeid(assem::OperInstr) && ((assem::OperInstr *)instr)->jumps_) {
      // fprintf(stderr, "jump\n");
      for (temp::Label *jump : *((assem::OperInstr *)instr)->jumps_->labels_) {
        FNode *node2 = label_map_->Look(jump);
        assert(node2);
        flowgraph_->AddEdge(node, node2);
      }
      // fprintf(stderr, "jump ok\n");
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
  return new temp::TempList();
}

temp::TempList *MoveInstr::Def() const {
  return dst_;
}

temp::TempList *OperInstr::Def() const {
  return dst_;
}

temp::TempList *LabelInstr::Use() const {
  return new temp::TempList();
}

temp::TempList *MoveInstr::Use() const {
  return src_;
}

temp::TempList *OperInstr::Use() const {
  return src_;
}
} // namespace assem
