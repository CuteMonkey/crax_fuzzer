#ifndef S2E_PLUGINS_PATHRULER_H
#define S2E_PLUGINS_PATHRULER_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>

namespace s2e {
namespace plugins {

class PathRuler: public Plugin
{
    S2E_PLUGIN
public:
    PathRuler(S2E *s2e): Plugin(s2e) {}
    void initialize();

private:
    bool count_jump;
    unsigned int instruction_count, jump_count;

    void onTranslateInstruction(ExecutionSignal*, S2EExecutionState*, TranslationBlock*, uint64_t);
    void onTranslateJump(ExecutionSignal*, S2EExecutionState*, TranslationBlock*, uint64_t, int);
    void onStateKill(S2EExecutionState*);
};
 
} //namespace plugins
} //namespace s2e

#endif
