#include "tiger/regalloc/color.h"
#include <set>

extern frame::RegManager *reg_manager;

namespace col {

void Color::ColorReg() {
    init();
    make_node_worklist();
    output_worklist();
    while (!simplify_worknodes->isEmpty() || !freeze_worknodes->isEmpty() || !spill_worknodes->isEmpty() || !worklist_moves->isEmpty()) {
        if (!simplify_worknodes->isEmpty()) {simplify(); fprintf(stderr, "[[[[[simplify]]]]]\n");}
        else if (!worklist_moves->isEmpty()) {coalesce(); fprintf(stderr, "[[[[[coalesce]]]]]\n");}
        else if (!freeze_worknodes->isEmpty()) {freeze(); fprintf(stderr, "[[[[[freeze]]]]]\n");}
        else if (!spill_worknodes->isEmpty()) {select_spill(); fprintf(stderr, "[[[[[spill]]]]]\n");}
        output_worklist();
    }
    assign_color();

    result_ = std::make_unique<Result>(colors, spilled_nodes);
}

void Color::init() {
    //初始化degree[]和moveList[]
    for (live::INode *inode : live_graph_.interf_graph->Nodes()->GetList()) {
        degree[inode] = inode->OutDegree();
        related_moves->Enter(inode, new live::MoveList());
    }
    for (auto move_edge : live_graph_.moves->GetList()) {
        live::MoveList *movelist_first = related_moves->Look(move_edge.first);
        movelist_first->Append(move_edge.first, move_edge.second);
        live::MoveList *movelist_second = related_moves->Look(move_edge.second);
        movelist_second->Append(move_edge.first, move_edge.second);

        //将所有边添加到worklistMoves中
        worklist_moves->Append(move_edge.first, move_edge.second);
    }


}

void Color::make_node_worklist() {
    for (live::INode *inode : live_graph_.interf_graph->Nodes()->GetList()) {
        if (reg_manager->Registers()->Contain(inode->NodeInfo())) {
            precolored->Append(inode);
        } else if (degree[inode] >= K) {
            spill_worknodes->Append(inode);
        } else if (!node_moves(inode)->isEmpty()) {
            freeze_worknodes->Append(inode);
        } else {
            simplify_worknodes->Append(inode);
        }
    }
}

live::MoveList *Color::node_moves(live::INode *inode) {
    live::MoveList *list = related_moves->Look(inode);
    return list->Intersect(not_ready_moves->Union(worklist_moves));
}
live::INodeList *Color::adjacent_nodes_now(live::INode *inode) {
    return inode->Pred()->Diff(select_stack->Union(coalesced_nodes))->Diff(precolored);
}
live::INodeList *Color::adjacent_nodes_all(live::INode *inode) {
    return inode->Pred();
}

void Color::decrement_degree(live::INode *inode) {
    assert(!precolored->Contain(inode));
    int d = degree[inode];
    degree[inode] = d - 1;

    if (d == K) {
        live::INodeList *list = adjacent_nodes_now(inode);
        list->Append(inode);
        enable_moves(list);

        spill_worknodes->DeleteNode(inode);

        if (!node_moves(inode)->isEmpty()) {
            freeze_worknodes->Append(inode);
        } else {
            simplify_worknodes->Append(inode);
        }
    }
}

void Color::simplify() {
    live::INode *inode = simplify_worknodes->GetList().front();
    simplify_worknodes->DeleteNode(inode);
    select_stack->Prepend(inode);
    for (live::INode *adj : adjacent_nodes_now(inode)->GetList()) {
        // int d = degree[adj];
        // degree[adj] = d - 1;

        // if (d == K) {
        //     live::INodeList *list = adjacent_nodes_now(adj);
        //     list->Append(adj);
        //     enable_moves(list);

        //     spill_worknodes->DeleteNode(adj);

        //     if (!node_moves(adj)->isEmpty()) {
        //         freeze_worknodes->Append(adj);
        //     } else {
        //         simplify_worknodes->Append(adj);
        //     }
        // }
        decrement_degree(adj);
    }
}

void Color::enable_moves(live::INodeList *list) {
    for (live::INode *inode : list->GetList()) {
        for (auto move : node_moves(inode)->GetList()) {
            if (not_ready_moves->Contain(move.first, move.second)) {
                not_ready_moves->Delete(move.first, move.second);
                worklist_moves->Append(move.first, move.second);
            }
        } 
    }
}

void Color::coalesce() {
    auto move = worklist_moves->GetList().front();
    live::INode *u = get_alias(move.first);
    live::INode *v = get_alias(move.second);
    if (precolored->Contain(v)) std::swap(u, v);
    worklist_moves->Delete(move.first, move.second);

    if (u == v) {
        coalesced_moves->Append(move.first, move.second);
        add_node_worklist(u);
    } else if (precolored->Contain(v) || u->GoesTo(v)) {
        constrained_moves->Append(move.first, move.second);
        add_node_worklist(u);
        add_node_worklist(v);
    } else if ((precolored->Contain(u) && can_combine_1(u,v)) ||
                (!precolored->Contain(u) && can_combine_2(u,v))) {
        coalesced_moves->Append(move.first, move.second);
        combine(u, v);
        add_node_worklist(u);
    } else {
        not_ready_moves->Append(move.first, move.second);
    }
}

bool Color::can_combine_1(live::INode *u, live::INode *v) {
    bool flag = true;
    for (live::INode *adj : adjacent_nodes_now(v)->GetList()) {
        if (!(degree[adj] < K || precolored->Contain(adj) || adj->GoesTo(u))) {
            flag = false;
        }
    }
    return flag;
}
bool Color::can_combine_2(live::INode *u, live::INode *v) {
    live::INodeList *list = adjacent_nodes_now(u)->Union(adjacent_nodes_now(v));
    int k = 0;
    for (live::INode *inode : list->GetList()) {
        if (degree[inode] >= K) ++k;
    }
    return k < K;
}

void Color::combine(live::INode *u, live::INode *v) {
    assert(!precolored->Contain(v));
    if (freeze_worknodes->Contain(v)) {
        freeze_worknodes->DeleteNode(v);
    } else {
        spill_worknodes->DeleteNode(v);
    }
    coalesced_nodes->Append(v);
    alias[v] = u;
    live::MoveList *moves_u = related_moves->Look(u);
    live::MoveList *moves_v = related_moves->Look(v);
    related_moves->Set(u, moves_u->Union(moves_v));

    live::INodeList *list = new live::INodeList();
    list->Append(v);
    enable_moves(list);

    for (live::INode *adj : adjacent_nodes_now(v)->GetList()) {
        live_graph_.interf_graph->AddEdge(adj, u);
        live_graph_.interf_graph->AddEdge(u, adj);
        if (adj->GoesTo(u)) {
            // degree[adj]--;
            decrement_degree(adj);
        } else {
            degree[u]++;
        }
    }
    if (degree[u] >= K && freeze_worknodes->Contain(u)) {
        freeze_worknodes->DeleteNode(u);
        spill_worknodes->Append(u);
    }
}


live::INode *Color::get_alias(live::INode *inode) {
    if (coalesced_nodes->Contain(inode)) {
        return get_alias(alias[inode]);
    }
    return inode;
}

void Color::add_node_worklist(live::INode *inode) {
    if (!precolored->Contain(inode) && node_moves(inode)->isEmpty() && degree[inode] < K) {
        freeze_worknodes->DeleteNode(inode);
        simplify_worknodes->Append(inode);
    } 
}

void Color::freeze() {
    live::INode *inode = freeze_worknodes->GetList().front();
    freeze_worknodes->DeleteNode(inode);
    simplify_worknodes->Append(inode);
    freeze_moves(inode);
}
void Color::freeze_moves(live::INode *u) {
    for (auto move : node_moves(u)->GetList()) {
        live::INode *v;
        if (get_alias(move.second) == get_alias(u)) {
            v = get_alias(move.first);
        } else {
            v = get_alias(move.second);
        }
        not_ready_moves->Delete(move.first, move.second);
        frozen_moves->Append(move.first, move.second);
        if (node_moves(v)->isEmpty() && degree[v] < K) {
            freeze_worknodes->DeleteNode(v);
            simplify_worknodes->Append(v);
        }
    }
}

void Color::select_spill() {
    //简化了选择策略，忽略了use和def的次数（？）
    live::INode *m;
    int max_degree = -1;
    for (live::INode *inode : spill_worknodes->GetList()) {
        fprintf(stderr, "degree_______:%d\n",degree[inode]);
        if (degree[inode] > max_degree) {
            m = inode;
            max_degree = degree[inode];
        }
    }
    spill_worknodes->DeleteNode(m);
    // fprintf(stderr, "after delete spill:");
    // output_worklist_size();
    simplify_worknodes->Append(m);
    freeze_moves(m);
}

void Color::assign_color() {
    //precolored registers
    std::string reg_name[16] = {"%rax","%rbx","%rcx","%rdx","%rsi","%rdi","%rbp","%rsp","%r8","%r9","%r10","%r11","%r12","%r13","%r14","%r15"};
    // std::unordered_map<std::string, int> reg_name2num;
    int i = 0;
    for (temp::Temp *t : reg_manager->Registers()->GetList()) {
        colors->Enter(t, new std::string(reg_name[i++]));
        // reg_name2num[reg_name[i]] = i;
    }
    // fprintf(stderr, "[[[[[[[[[[[[select stack size: %d\n", select_stack->GetList().size());
    while (!select_stack->isEmpty()) {
        live::INode *inode = select_stack->GetList().front();
        select_stack->DeleteNode(inode);
        std::set<std::string> ok_colors;
        for (int i = 0; i < K; ++i) ok_colors.insert(reg_name[i]);

        // fprintf(stderr, "adj colors: ");
        for (live::INode *adj : adjacent_nodes_all(inode)->GetList()) {
            live::INode *adj_alias = get_alias(adj);
            if (colored_nodes->Contain(adj_alias) || precolored->Contain(adj_alias)) {
                std::string adj_color = *(colors->Look(adj_alias->NodeInfo()));
                // fprintf(stderr, "%s ", adj_color.data());
                ok_colors.erase(adj_color);
            }
        } 
        // fprintf(stderr, "\n");
        // fprintf(stderr, "ok colors: ");
        // for (std::string s : ok_colors) {
            // fprintf(stderr, "%s ", s.data());
        // }
        // fprintf(stderr, "\n");

        if (ok_colors.empty()) {
            spilled_nodes->Append(inode);
        } else {
            colored_nodes->Append(inode);
            std::string inode_color = *(ok_colors.begin());
            colors->Enter(inode->NodeInfo(), new std::string(inode_color));
        }
    }

    for (live::INode *inode : coalesced_nodes->GetList()) {
        std::string *c = colors->Look(get_alias(inode)->NodeInfo());
        if (c) {
            colors->Enter(inode->NodeInfo(), c);
        }
    }
}

void Color::output_worklist_size() {
    fprintf(stderr, "simplify size:%d\n", simplify_worknodes->GetList().size());
    fprintf(stderr, "move size:%d\n", worklist_moves->GetList().size());
    fprintf(stderr, "spill size:%d\n", spill_worknodes->GetList().size());
    fprintf(stderr, "freeze size:%d\n", freeze_worknodes->GetList().size());
}
void Color::output_worklist() {
    temp::Map *color = temp::Map::LayerMap(reg_manager->temp_map_, temp::Map::Name());
    fprintf(stderr, "precolored: ");
    for (live::INode *n : precolored->GetList()) {
        fprintf(stderr, "%s ", color->Look(n->NodeInfo())->data());
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "simplify: ");
    for (live::INode *n : simplify_worknodes->GetList()) {
        fprintf(stderr, "%s ", color->Look(n->NodeInfo())->data());
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "freeze: ");
    for (live::INode *n : freeze_worknodes->GetList()) {
        fprintf(stderr, "%s ", color->Look(n->NodeInfo())->data());
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "spill: ");
    for (live::INode *n : spill_worknodes->GetList()) {
        fprintf(stderr, "%s ", color->Look(n->NodeInfo())->data());
    }
    fprintf(stderr, "\n");
    fprintf(stderr, "moves:\n");
    for (auto move : worklist_moves->GetList()) {
        fprintf(stderr, "%s->%s\n", color->Look(move.first->NodeInfo())->data(), color->Look(move.second->NodeInfo())->data());
    }
    fprintf(stderr, "\n");
}


Color::Color(live::LiveGraph live_graph)
    : live_graph_(live_graph),
      related_moves(new tab::Table<live::INode, live::MoveList>()) {
        precolored = new live::INodeList();
        spill_worknodes = new live::INodeList();
        freeze_worknodes = new live::INodeList();
        simplify_worknodes = new live::INodeList();
        spilled_nodes = new live::INodeList();
        coalesced_nodes = new live::INodeList();
        colored_nodes = new live::INodeList();
        select_stack = new live::INodeList();

        worklist_moves = new live::MoveList();
        not_ready_moves = new live::MoveList();
        frozen_moves = new live::MoveList();
        constrained_moves = new live::MoveList();
        coalesced_moves = new live::MoveList();

        K = reg_manager->GetRegNum();
        colors = temp::Map::Empty();
      }

} // namespace col
