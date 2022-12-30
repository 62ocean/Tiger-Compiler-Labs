#include "tiger/absyn/absyn.h"
#include "tiger/escape/escape.h"
#include "tiger/frame/x64frame.h"
#include "tiger/output/logger.h"
#include "tiger/output/output.h"
#include "tiger/parse/parser.h"
#include "tiger/translate/translate.h"
#include "tiger/semant/semant.h"

frame::RegManager *reg_manager;
frame::Frags *frags;

int main(int argc, char **argv) {
  std::string_view fname;
  std::unique_ptr<absyn::AbsynTree> absyn_tree;
  reg_manager = new frame::X64RegManager();
  frags = new frame::Frags();

  if (argc < 2) {
    fprintf(stderr, "usage: tiger-compiler file.tig\n");
    exit(1);
  }

  fname = std::string_view(argv[1]);

  {
    std::unique_ptr<err::ErrorMsg> errormsg;

    {
      // Lab 3: parsing
      TigerLog("-------====Parse=====-----\n");
      Parser parser(fname, std::cerr);
      parser.parse();
      absyn_tree = parser.TransferAbsynTree();
      errormsg = parser.TransferErrormsg();
    }

    {
      // Lab 4: semantic analysis
      TigerLog("-------====Semantic analysis=====-----\n");
      sem::ProgSem prog_sem(std::move(absyn_tree), std::move(errormsg));
      prog_sem.SemAnalyze();
      absyn_tree = prog_sem.TransferAbsynTree();
      errormsg = prog_sem.TransferErrormsg();
    }

    {
      // Lab 5: escape analysis
      TigerLog("-------====Escape analysis=====-----\n");
      esc::EscFinder esc_finder(std::move(absyn_tree));
      esc_finder.FindEscape();
      absyn_tree = esc_finder.TransferAbsynTree();
    }
    fprintf(stderr, "after escape\n");

    {
      // Lab 5: translate IR tree
      TigerLog("-------====Translate=====-----\n");
      tr::ProgTr prog_tr(std::move(absyn_tree), std::move(errormsg));
      prog_tr.Translate();
      errormsg = prog_tr.TransferErrormsg();
    }
    fprintf(stderr, "after translate\n");
    // for (frame::Frag *frag : frags->GetList()) {
    //   if (typeid(*frag) == typeid(frame::ProcFrag)) {
    //     frame::ProcFrag *profrag = (frame::ProcFrag *)frag;
    //     profrag->body_->Print(stderr,0);
    //   }
    //   fprintf(stderr, "~~~~~~end~~~~~\n\n");
    // }

    if (errormsg->AnyErrors())
      return 1; // Don't continue if error occurrs
  }

  {
    // Output assembly
    output::AssemGen assem_gen(fname);
    fprintf(stderr, "after init assemgen\n");
    assem_gen.GenAssem(false);
  }

  return 0;
}