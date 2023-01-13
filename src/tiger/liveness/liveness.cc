#include "tiger/liveness/liveness.h"
#include <queue>

extern frame::RegManager *reg_manager;

namespace live {

bool MoveList::Contain(INodePtr src, INodePtr dst) {
  return std::any_of(move_list_.cbegin(), move_list_.cend(),
                     [src, dst](std::pair<INodePtr, INodePtr> move) {
                       return move.first == src && move.second == dst;
                     });
}

void MoveList::Delete(INodePtr src, INodePtr dst) {
  assert(src && dst);
  auto move_it = move_list_.begin();
  for (; move_it != move_list_.end(); move_it++) {
    if (move_it->first == src && move_it->second == dst) {
      break;
    }
  }
  move_list_.erase(move_it);
}

MoveList *MoveList::Union(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : move_list_) {
    res->move_list_.push_back(move);
  }
  if (list != nullptr) {
    for (auto move : list->GetList()) {
      if (!res->Contain(move.first, move.second))
        res->move_list_.push_back(move);
    }
  }
  return res;
}

MoveList *MoveList::Intersect(MoveList *list) {
  auto *res = new MoveList();
  for (auto move : list->GetList()) {
    if (Contain(move.first, move.second))
      res->move_list_.push_back(move);
  }
  return res;
}

//得到in-table和out-table
void LiveGraphFactory::LiveMap() {
  std::queue<fg::FNode *> process_node;
  //将最后一条指令推入队列
  fg::FNode *last = flowgraph_->Nodes()->GetList().back();
  process_node.push(last);

  while (!process_node.empty()) {
    //取队头元素
    fg::FNode *node = process_node.front();
    process_node.pop();

    temp::TempList *in_temp = new temp::TempList();
    temp::TempList *out_temp = new temp::TempList();

    //计算该结点的out并更新table
    if (node == last) {
      out_temp = node->NodeInfo()->Use();
      out_->Enter(node, out_temp);
    } else {
      fg::FNodeList *succs = node->Succ();
      for (fg::FNode *succ : succs->GetList()) {
        temp::TempList *succ_in = in_->Look(succ);
        out_temp = out_temp->Union(succ_in);
      }
      out_->Enter(node, out_temp);
    }
    //计算该结点的in
    in_temp = (out_temp->Diff(node->NodeInfo()->Def()))->Union(node->NodeInfo()->Use());
    
    //将计算所得in与table中原始的in进行比较，如相同则不再继续更新，如不同则将该结点的前驱全部推入队列
    temp::TempList *old_in_temp = in_->Look(node);
    if (in_temp->Equal(old_in_temp)) continue;
    else {
      in_->Enter(node, in_temp);
      fg::FNodeList *preds = node->Pred();
      for (fg::FNode *pred : preds->GetList()) {
        process_node.push(pred);
      }
    }
  }
}

void LiveGraphFactory::InterfGraph() {

  //precolored registers
  temp::TempList *precolored = reg_manager->Registers();
  //建node
  for (temp::Temp *reg : precolored->GetList()) {
    INode *node = live_graph_.interf_graph->NewNode(reg);
    temp_node_map_->Enter(reg, node);
  }
  //两两之间连边
  for (temp::Temp *reg : precolored->GetList()) {
    for (temp::Temp *reg2 : precolored->GetList()) {
      if (reg == reg2) continue;
      //AddEdge中会判断改边是否已存在
      live_graph_.interf_graph->AddEdge(temp_node_map_->Look(reg), temp_node_map_->Look(reg2));
      live_graph_.interf_graph->AddEdge(temp_node_map_->Look(reg2), temp_node_map_->Look(reg));
    }
  }

  //将指令中存在的寄存器全部变为node
  for (fg::FNode *fnode : flowgraph_->Nodes()->GetList()) {
    temp::TempList *def_list = fnode->NodeInfo()->Def();
    temp::TempList *use_list = fnode->NodeInfo()->Use();
    if (def_list) {
      for (temp::Temp *t : def_list->GetList()) {
        INode *node = temp_node_map_->Look(t);
        if (!node) {
          node = live_graph_.interf_graph->NewNode(t);
          temp_node_map_->Enter(t, node);
        }
      }
    }
    if (use_list) {
      for (temp::Temp *t : use_list->GetList()) {
        INode *node = temp_node_map_->Look(t);
        if (!node) {
          node = live_graph_.interf_graph->NewNode(t);
          temp_node_map_->Enter(t, node);
        }
      }
    }
  }

  
  for (fg::FNode *fnode : flowgraph_->Nodes()->GetList()) {
    if (!fnode->NodeInfo()->Def()) continue;
    temp::TempList *out_list = out_->Look(fnode);

    //处理move
    if (typeid(*fnode->NodeInfo()) == typeid(assem::MoveInstr)) {
      INode *dst = temp_node_map_->Look(fnode->NodeInfo()->Def()->GetList().front());
      INode *src = temp_node_map_->Look(fnode->NodeInfo()->Use()->GetList().front());
      live_graph_.moves->Append(src, dst);
      
      out_list = out_list->Diff(fnode->NodeInfo()->Use());
    }

    for (temp::Temp *def : fnode->NodeInfo()->Def()->GetList()) {
      INode *node1 = temp_node_map_->Look(def);
      assert(node1);
      for (temp::Temp *out : out_list->GetList()) {
        INode *node2 = temp_node_map_->Look(out);
        assert(node2);
        if (def != out) {
          live_graph_.interf_graph->AddEdge(node1, node2);
          live_graph_.interf_graph->AddEdge(node2, node1);
        }
      }
    }
  }
}

void LiveGraphFactory::Liveness() {
  LiveMap();
  InterfGraph();
}

//调试函数
void LiveGraphFactory::output_in_out() {
  temp::Map *color = temp::Map::LayerMap(reg_manager->temp_map_, temp::Map::Name());
  for (fg::FNode *node : flowgraph_->Nodes()->GetList()) {
    node->NodeInfo()->Print(stderr, color);

    temp::TempList *in_list = in_->Look(node);
    fprintf(stderr, "in: ");
    for (temp::Temp *t : in_list->GetList()) {
      fprintf(stderr, "%s ", color->Look(t)->data());
    }
    fprintf(stderr, "\n");

    temp::TempList *out_list = out_->Look(node);
    fprintf(stderr, "out: ");
    for (temp::Temp *t : out_list->GetList()) {
      fprintf(stderr, "%s ", color->Look(t)->data());
    }
    fprintf(stderr, "\n\n");
  }
}
void LiveGraphFactory::output_livegraph() {
  temp::Map *color = temp::Map::LayerMap(reg_manager->temp_map_, temp::Map::Name());
  for (live::INode *inode : live_graph_.interf_graph->Nodes()->GetList()) {
    if (reg_manager->Registers()->Contain(inode->NodeInfo())) continue;
    fprintf(stderr, "%s::: ", color->Look(inode->NodeInfo())->data());

    for (live::INode *adj : inode->Pred()->GetList()) {
      fprintf(stderr, "%s ", color->Look(adj->NodeInfo())->data());
    }
    fprintf(stderr, "\n");
  }

  fprintf(stderr, "moves:\n");
  for (auto move : live_graph_.moves->GetList()) {
    fprintf(stderr, "%s->%s\n", color->Look(move.first->NodeInfo())->data(), color->Look(move.second->NodeInfo())->data());
  }
}



} // namespace live
