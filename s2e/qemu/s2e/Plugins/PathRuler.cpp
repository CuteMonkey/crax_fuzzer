#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include "PathRuler.h"

namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(PathRuler, "Measure the length of the certain path", "PathRuler",);

void PathRuler::initialize()
{
    count_jump = s2e()->getConfig()->getBool(getConfigKey() + ".countJump");
    instruction_count = 0;
    jump_count = 0;

    s2e()->getCorePlugin()->onTranslateInstructionStart.connect(sigc::mem_fun(*this, &PathRuler::onTranslateInstruction));
    if(count_jump) {
        s2e()->getCorePlugin()->onTranslateJumpStart.connect(sigc::mem_fun(*this, &PathRuler::onTranslateJump));
    }
    s2e()->getCorePlugin()->onStateKill.connect(sigc::mem_fun(*this, &PathRuler::onStateKill));
}


void PathRuler::onTranslateInstruction(ExecutionSignal *signal, S2EExecutionState *state, TranslationBlock *tb, uint64_t pc)
{
    instruction_count++;
}

void PathRuler::onTranslateJump(ExecutionSignal *signal, S2EExecutionState *state, TranslationBlock *tb, uint64_t pc, int j_type)
{
    jump_count++;
}

void PathRuler::onStateKill(S2EExecutionState *state)
{
    s2e()->getDebugStream() << "Length of path: " << instruction_count << " instrunction(s)\n";
    if(count_jump) {
        s2e()->getDebugStream() << "The path contains " << jump_count << " jump(s)\n";
    }
}

} //namespace plugins
} //namespace s2e
