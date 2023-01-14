#include "tiger/regalloc/regalloc.h"

#include "tiger/output/logger.h"

extern frame::RegManager *reg_manager;

namespace ra {

void RegAllocator::RegAlloc() {

    fprintf(stderr, "start regalloc!!!!!\n");

    fg::FlowGraphFactory flowgraph_factory(assem_instr_->GetInstrList());
    fprintf(stderr, "flowgraph factory ok\n");
    flowgraph_factory.AssemFlowGraph();
    fprintf(stderr, "build flowgraph ok\n");

    live::LiveGraphFactory livegraph_factory(flowgraph_factory.GetFlowGraph());
    fprintf(stderr, "livegraph factory ok\n");
    livegraph_factory.Liveness();
    fprintf(stderr, "build interfere graph ok\n");
    livegraph_factory.output_in_out();
    livegraph_factory.output_livegraph();
    col::Color color(livegraph_factory.GetLiveGraph(), livegraph_factory.GetDefUseNum());
    color.ColorReg();
    fprintf(stderr, "assign color ok\n");
    std::unique_ptr<col::Result> color_result = color.TransferResult();
    if (!color_result->spills->isEmpty()) {
        fprintf(stderr, "start rewrite program\n");
        rewrite_program(color_result->spills);
        fprintf(stderr, "rewrite program ok\n");
        RegAlloc();
    } else {
        result_ = std::make_unique<Result>(color_result->coloring, assem_instr_->GetInstrList());
    }

}

void RegAllocator::rewrite_program(live::INodeList *spilled_nodes) {
    temp::Map *color1 = temp::Map::LayerMap(reg_manager->temp_map_, temp::Map::Name());
    fprintf(stderr, "spilled ndoes: \n");
    for (live::INode *spill_node : spilled_nodes->GetList()) {
        fprintf(stderr, "%s ", color1->Look(spill_node->NodeInfo())->data());
    }
    fprintf(stderr, "\n");

    assem::InstrList *instrs = assem_instr_->GetInstrList();
    std::string fs = frame_->GetLabel()+"_framesize";

    for (live::INode *spill_node : spilled_nodes->GetList()) {
        fprintf(stderr, "start spill %s:\n", color1->Look(spill_node->NodeInfo())->data());
        frame::Access *m0 = frame_->AllocLocal(true);
        frame::InFrameAccess *m = (frame::InFrameAccess *)m0;

        bool exist_spill = true;
        while (exist_spill) {
            exist_spill = false;
            // fprintf(stderr, "start rewrite a new instr:\n");
            
            for (auto it = instrs->GetList().begin(); it != instrs->GetList().end(); ++it) {
                // auto next_it = it;
                // (*it)->Print(stderr, color1);
                if ((*it)->Use()->Contain(spill_node->NodeInfo())) {
                    fprintf(stderr, "use: ");
                    (*it)->Print(stderr, color1);
                    frame::Access *v0 = frame_->AllocLocal(false);
                    frame::InRegAccess *v = (frame::InRegAccess *)v0;
                    (*it)->Use()->Temp2Temp(spill_node->NodeInfo(), v->reg);

                    instrs->Insert(it, new assem::OperInstr(
                        "movq "+std::to_string(m->offset)+"+"+fs+"(`s0),`d0",
                        new temp::TempList(v->reg),
                        new temp::TempList(reg_manager->StackPointer()),
                        nullptr
                    ));
                    exist_spill = true;
                    break;
                } else if ((*it)->Def()->Contain(spill_node->NodeInfo())) {
                    fprintf(stderr, "def: ");
                    (*it)->Print(stderr, color1);
                    frame::Access *v0 = frame_->AllocLocal(false);
                    frame::InRegAccess *v = (frame::InRegAccess *)v0;

                    // (*it)->Def()->Print(color1);
                    (*it)->Def()->Temp2Temp(spill_node->NodeInfo(), v->reg);
                    // (*it)->Def()->Print(color1);

                    ++it;
                    instrs->Insert(it, new assem::OperInstr(
                        "movq `s0,"+std::to_string(m->offset)+"+"+fs+"(`d0)",
                        new temp::TempList({reg_manager->StackPointer()}),
                        new temp::TempList({v->reg}),
                        nullptr
                    ));
                    exist_spill = true;
                    break;
                }
            }
            // fprintf(stderr, "flag: %d\n", exist_spill);
            
        }
        // fprintf(stderr, "after one rewrite*******\n");
        // instrs->Print(stderr, color1);
        // fprintf(stderr, "\n");
        
        // exit(0);
    }
}

} // namespace ra