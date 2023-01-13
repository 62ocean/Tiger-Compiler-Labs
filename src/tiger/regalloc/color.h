#ifndef TIGER_COMPILER_COLOR_H
#define TIGER_COMPILER_COLOR_H

#include "tiger/codegen/assem.h"
#include "tiger/frame/temp.h"
#include "tiger/liveness/liveness.h"
#include "tiger/util/graph.h"
#include <unordered_map>
#include <stack>

namespace col {
struct Result {
  Result() : coloring(nullptr), spills(nullptr) {}
  Result(temp::Map *coloring, live::INodeListPtr spills)
      : coloring(coloring), spills(spills) {}
  temp::Map *coloring;
  live::INodeListPtr spills;
};

class Color {
public:
  Color(live::LiveGraph live_graph);

  void ColorReg();
  std::unique_ptr<Result> TransferResult() {
    return std::move(result_);
  }

private:
  live::LiveGraph live_graph_;
  std::unique_ptr<Result> result_;

  std::unordered_map<live::INode *, int> degree;
  tab::Table<live::INode, live::MoveList> *related_moves;
  std::unordered_map<live::INode *, live::INode *> alias;
  temp::Map *colors;
  int K;

  void init();
  void make_node_worklist();
  void simplify();
  void coalesce();
  void freeze();
  void select_spill();
  void assign_color();

  live::MoveList *node_moves(live::INode *);
  live::INodeList *adjacent_nodes_now(live::INode *);
  live::INodeList *adjacent_nodes_all(live::INode *);
  void enable_moves(live::INodeList *);
  live::INode *get_alias(live::INode *);
  void add_node_worklist(live::INode *);
  void freeze_moves(live::INode *);
  void decrement_degree(live::INode *);

  bool can_combine_1(live::INode *u, live::INode *v);
  bool can_combine_2(live::INode *u, live::INode *v);
  void combine(live::INode *u, live::INode *v);

  live::INodeList *precolored;
  live::INodeList *spill_worknodes;
  live::INodeList *freeze_worknodes;
  live::INodeList *simplify_worknodes;
  live::INodeList *spilled_nodes;
  live::INodeList *coalesced_nodes;
  live::INodeList *colored_nodes;
  live::INodeList *select_stack;

  live::MoveList *worklist_moves;
  live::MoveList *not_ready_moves;
  live::MoveList *frozen_moves;
  live::MoveList *constrained_moves;
  live::MoveList *coalesced_moves;

  void output_worklist_size();
  void output_worklist();
  
};
} // namespace col

#endif // TIGER_COMPILER_COLOR_H
