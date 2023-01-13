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
    col::Color color(livegraph_factory.GetLiveGraph());
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
    assem::InstrList *instrs = assem_instr_->GetInstrList();
    std::string fs = frame_->GetLabel()+"_framesize";

    for (live::INode *spill_node : spilled_nodes->GetList()) {
        frame::Access *m0 = frame_->AllocLocal(true);
        frame::InFrameAccess *m = (frame::InFrameAccess *)m0;
        for (auto it = instrs->GetList().begin(); it != instrs->GetList().end(); ++it) {
            // auto next_it = it;
            if ((*it)->Use()->Contain(spill_node->NodeInfo())) {
                frame::Access *v0 = frame_->AllocLocal(false);
                frame::InRegAccess *v = (frame::InRegAccess *)v0;
                (*it)->Use()->Temp2Temp(spill_node->NodeInfo(), v->reg);

                instrs->Insert(it, new assem::OperInstr(
                    "movq "+std::to_string(m->offset)+"+"+fs+"(`s0),`d0",
                    new temp::TempList(v->reg),
                    new temp::TempList(reg_manager->StackPointer()),
                    nullptr
                ));
            }
            if ((*it)->Def()->Contain(spill_node->NodeInfo())) {
                frame::Access *v0 = frame_->AllocLocal(false);
                frame::InRegAccess *v = (frame::InRegAccess *)v0;
                (*it)->Def()->Temp2Temp(spill_node->NodeInfo(), v->reg);

                ++it;
                instrs->Insert(it, new assem::OperInstr(
                    "movq `s0,"+std::to_string(m->offset)+"+"+fs+"(`d0)",
                    new temp::TempList({reg_manager->StackPointer()}),
                    new temp::TempList({v->reg}),
                    nullptr
                ));
                --it;
            }
        }
    }
}

} // namespace ra