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
  for (auto move : list->GetList()) {
    if (!res->Contain(move.first, move.second))
      res->move_list_.push_back(move);
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
  for (temp::Temp *reg : precolored->GetList()) {
    INode *node = live_graph_.interf_graph->NewNode(reg);
    temp_node_map_->Enter(reg, node);
  }
  for (temp::Temp *reg : precolored->GetList()) {
    for (temp::Temp *reg2 : precolored->GetList()) {
      if (reg == reg2) continue;
      live_graph_.interf_graph->AddEdge(temp_node_map_->Look(reg), temp_node_map_->Look(reg2));
      live_graph_.interf_graph->AddEdge(temp_node_map_->Look(reg2), temp_node_map_->Look(reg));
    }
  }

  //没有考虑move的情况！
  for (fg::FNode *fnode : flowgraph_->Nodes()->GetList()) {
    if (!fnode->NodeInfo()->Def()) continue;

    temp::TempList *out_list = out_->Look(fnode);
    for (temp::Temp *def : fnode->NodeInfo()->Def()->GetList()) {
      INode *node1 = temp_node_map_->Look(def) ? temp_node_map_->Look(def) : live_graph_.interf_graph->NewNode(def);
      for (temp::Temp *out : out_list->GetList()) {
        INode *node2 = temp_node_map_->Look(out) ? temp_node_map_->Look(out) : live_graph_.interf_graph->NewNode(out);
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

} // namespace live
