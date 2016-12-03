#ifndef S2E_PLUGINS_INFOCOLLECTOR_H
#define S2E_PLUGINS_INFOCOLLECTOR_H

#include <s2e/Plugin.h>
#include <s2e/Plugins/CorePlugin.h>
#include <s2e/S2EExecutionState.h>

namespace s2e {
namespace plugins {

class InfoCollector: public Plugin
{
    S2E_PLUGIN
public:
    InfoCollector(S2E *s2e): Plugin(s2e), real_input_num(0), case_id(1), call_stream(NULL), try_stream(NULL), pc_result(0)
    {}

    void initialize();

    //the same function of class destructor
    void infoDestructor(int = 0);
private:
    /*functions*/
    //collect informaiton needed from the result of symbolic execution
    void collectInformation(S2EExecutionState*, const std::string&);
    //print the collected information to the output file
    void outputInformation(const std::string&);

    //delete input constraints from path constraints
    void pruneInputConstraints(S2EExecutionState*, klee::ExecutionState*);
    //generate argument constraints by symbolic information
    void generateArgsConstraints(S2EExecutionState*, klee::ExecutionState*);
    //generate the possible exploit input(s)
    //return value: if generate test case, return true, otherwise, return false
    bool generateExploitInput(S2EExecutionState*, klee::ExecutionState*);

    //select which scheduling algorithm to be used
    void selectSchedulingAlgorithm(S2EExecutionState*, klee::ExecutionState*, klee::ref<klee::Expr>&);
    //scheduling algorithm 1: using combination that combination size in ascending order and constraints choosing in collecting order
    void combinationFromHead(S2EExecutionState*, klee::ExecutionState*, klee::ref<klee::Expr>&);
    //scheduling algorithm 2: using combination that combination size in ascending order and constraints choosing in reverse
    //collecting order
    void combinationFromTail(S2EExecutionState*, klee::ExecutionState*, klee::ref<klee::Expr>&);
    //scheduling algorithm 3: select combination by Q-learning
    void QLearning(S2EExecutionState*, klee::ExecutionState*, klee::ref<klee::Expr>&);

    //send input generated fail signal file
    void sendInputGenFail();
    //send input generated signal file
    void sendInputGenerate();
    //confirm the result of verification
    //return value: if verify successfully, return 1, fail, return 0, no_file, return -1
    int confirmResult();
    //send confirm end signal file
    void sendConfirmEnd();
    //send fuzzing complete signal file then kill the state
    void sendAllComplete(S2EExecutionState*);
    //send Q-learning start signal file
    //void sendStartQLearning();
    //read the result of Q-learning stage
    void readQLearningResult();


    /*data structures*/
    //node for environment features of a (path) constraint
    struct constraint_node {
        unsigned int id;
        klee::ref<klee::Expr> raw_constraint;
    };
    typedef std::vector<constraint_node> con_info_ty;

    con_info_ty constraints_info;
    unsigned int constraints_size;

    typedef std::pair<std::string, std::vector<unsigned char> > var_val_pair_ty;
    typedef std::vector<var_val_pair_ty> solution_ty;

    //max length of P.C. combinations
    unsigned int max_pc_len;
    //size of a level of P.C. id feature
    unsigned int pc_level_size;
    //vector for average feature weight
    std::vector<double> weight_vec;

    typedef std::vector<unsigned int> pc_combination_ty;
    //typedef std::pair<pc_combination_ty, double> pc_score_pair_ty;
    struct PSPair {
        friend class InfoCollector;

        PSPair(): score(0.0) {}
        PSPair(const PSPair &other): pc_vec(other.pc_vec), score(other.score) {}
        bool operator< (const PSPair &other) const {
            return (this->score < other.score);
        }

        InfoCollector::pc_combination_ty pc_vec;
        double score;
    };


    /*config parameters*/
    //to select scheduling algorithm
    unsigned int algorithm;
    //the number of expected generated exploit inputs
    unsigned int expect_input_num;
    //to switch debug mode
    bool debug_mode;
    //whether make the copy of generated input
    bool input_copy;
    //the ID of learning result(only use in algorithm 3)
    unsigned int learning_result_id;


    /*control and other variables*/
    //the number of exploit inputs which have been generated and verified successfully
    unsigned int real_input_num;
    //the serial  number for the file name of test cases
    unsigned int case_id;
    //raw stream for call record
    llvm::raw_ostream *call_stream;
    //raw stream for try record
    llvm::raw_ostream *try_stream;
    //to record the result of the current combination of P.C.
    //0: in proess, 1: input generating failed, 2: verification failed,
    //3: no file generated, 4: verification success
    unsigned int pc_result;
};

}
}

#endif
