#include <s2e/S2E.h>
#include <s2e/ConfigFile.h>
#include <s2e/S2EExecutor.h>
#include "BaseInstructions.h"
#include <algorithm>
#include <sstream>
#include <fstream>
#include <iomanip>
#include <cstdlib>
#include "InfoCollector.h"


namespace s2e {
namespace plugins {

S2E_DEFINE_PLUGIN(InfoCollector, "Collect infomations for learning to schedule constraints", "InfoCollector",);

void InfoCollector::infoDestructor(int exit_status)
{
    constraints_info.clear();
    constraints_size = 0;

    weight_vec.clear();

    if(debug_mode) {
        call_stream->flush();
        try_stream->flush();

        delete call_stream;
        delete try_stream;
    }

    exit(exit_status);
}


void InfoCollector::initialize()
{
    algorithm = s2e()->getConfig()->getInt(getConfigKey() + ".algorithm");
    expect_input_num = s2e()->getConfig()->getInt(getConfigKey() + ".input_num");
    learning_result_id = s2e()->getConfig()->getInt(getConfigKey() + ".learn_id");
    debug_mode = s2e()->getConfig()->getBool(getConfigKey() + ".debug");
    input_copy = s2e()->getConfig()->getBool(getConfigKey() + ".copy");

    if(debug_mode) {
        call_stream = s2e()->openOutputFile("call_record.txt");
        try_stream = s2e()->openOutputFile("try_record.txt");
    }

    s2e()->getCorePlugin()->onTestCaseGeneration.connect(
        sigc::mem_fun(*this, &InfoCollector::collectInformation));
}


void InfoCollector::collectInformation(S2EExecutionState *state, const std::string &message)
{
    if(debug_mode) {
        (*call_stream) << "Enter collectInformation().\n";
        call_stream->flush();
    }

    //struct stat file_state;
    //if all fuzzing work has been done, directly return.
    //if(stat(s2e()->getOutputFilename("complete.txt").c_str(), &file_state) == 0) return;

    if(state->argsConstraintsAll.size() == 0) {
        s2e()->getWarningsStream() << "Warning: Did not taint any sensitive function.\n";
        //s2e()->getExecutor()->terminateStateEarly(*state, "");
        if(debug_mode) {
            (*call_stream) << "Leave collectInformation().\n";
            call_stream->flush();
        }

        infoDestructor(1);
    }

    klee::ExecutionState *exploit_state = new klee::ExecutionState(*state);
    pruneInputConstraints(state, exploit_state);

    unsigned int id = 0;

    klee::ConstraintManager::const_iterator c_it;
    for(c_it = exploit_state->constraints.begin(); c_it != exploit_state->constraints.end(); c_it++) {
        id++;

        constraint_node *c_node = new constraint_node;
        c_node->id = id;
        c_node->raw_constraint = *c_it;

        constraints_info.push_back(*c_node);
    }
    constraints_size = id;

    if(constraints_info.size() == 0) {
        s2e()->getWarningsStream() << "Warning: Did not collect any information about constraints.\n";
    }
    outputInformation("info_collection.txt");

    generateArgsConstraints(state, exploit_state);

    if(debug_mode) {
        (*call_stream) << "Leave collectInformation().\n";
        call_stream->flush();
    }
}

void InfoCollector::outputInformation(const std::string &info_file_name)
{
    if(debug_mode) {
        (*call_stream) << "Enter outputInformation().\n";
        call_stream->flush();
    }

    llvm::raw_ostream *info_out = s2e()->openOutputFile(info_file_name);

    (*info_out) << "Total " << constraints_size << "\n\n"; 
    for(con_info_ty::const_iterator c_it = constraints_info.begin(); c_it != constraints_info.end(); c_it++) {
        (*info_out) << "path constraint " << (c_it->id) << ":\n";
        (*info_out) << (c_it->raw_constraint) << "\n";
        (*info_out) << "------------\n";
        info_out->flush();
    }

    delete info_out;

    if(debug_mode) {
        (*call_stream) << "Leave outputInformation().\n";
        call_stream->flush();
    }
}


void InfoCollector::pruneInputConstraints(S2EExecutionState *state, klee::ExecutionState *exploit_state)
{
    if(debug_mode) {
        (*call_stream) << "Enter pruneInputConstraints().\n";
        call_stream->flush();
    }

    klee::ConstraintManager::constraints_ty new_path_constraints(state->constraints.size());
    klee::ConstraintManager::iterator it;

    it = std::set_difference(state->constraints.begin(), state->constraints.end(),
        state->inputConstraints.begin(), state->inputConstraints.end(), new_path_constraints.begin());
    new_path_constraints.resize(it - new_path_constraints.begin());

    exploit_state->constraints = *(new klee::ConstraintManager(new_path_constraints));

    if(debug_mode) {
        (*call_stream) << "Leave pruneInputConstraints().\n";
        call_stream->flush();
    }
}

void InfoCollector::generateArgsConstraints(S2EExecutionState *state, klee::ExecutionState *exploit_state)
{
    if(debug_mode) {
        (*call_stream) << "Enter generateArgsConstraints().\n";
        call_stream->flush();
    }

    size_t args_constraints_all_size =  state->argsConstraintsAll.size();
    klee::ref<klee::Expr> args_constraints, byte;

    for(unsigned int i = 0; i < args_constraints_all_size; i++) {
        if(state->argsConstraintsType[i].compare("vfprintf_format") == 0 || state->argsConstraintsType[i].compare("syslog_format") == 0 || state->argsConstraintsType[i].compare("strcpy_src") == 0 || state->argsConstraintsType[i].compare("strncpy_src") == 0) {
            args_constraints = klee::ConstantExpr::create(0x1, klee::Expr::Bool);
            for(unsigned int j = 0; j < state->argsConstraintsAll[i].size(); j++) {
                //if the argument constraint is a ConstantExpr
                if(isa<klee::ConstantExpr>(state->argsConstraintsAll[i][j])) continue;
                //create the constraint that let every byte of certain argument equal character 'a'
                byte = klee::EqExpr::create(state->argsConstraintsAll[i][j], klee::ConstantExpr::create(0x61, klee::Expr::Int32));
                //connect each argument constraint by AND
                args_constraints = klee::AndExpr::create(args_constraints, byte);
            }
            selectSchedulingAlgorithm(state, exploit_state, args_constraints);
        } else if(state->argsConstraintsType[i].compare("malloc_size") == 0) {
            args_constraints = klee::ConstantExpr::create(0x1, klee::Expr::Bool);
            for(unsigned int j = 0; j < state->argsConstraintsAll[i].size(); j++) {
                //create the constraint that let certain argument equal interger 0
                byte = klee::EqExpr::create(state->argsConstraintsAll[i][j], klee::ConstantExpr::create(0x0, klee::Expr::Int32));
                //connect each argument constraint by AND
                args_constraints = klee::AndExpr::create(args_constraints, byte);
            }
            selectSchedulingAlgorithm(state, exploit_state, args_constraints);
        }
    }

    if(debug_mode) {
        (*call_stream) << "Leave generateArgsConstraints().\n";
        call_stream->flush();
    }

    s2e()->getWarningsStream() << "Warning: Tried all tainted arugments but not success.\n";
    //s2e()->getExecutor()->terminateStateEarly(*state, "");
    infoDestructor(1);
}

bool InfoCollector::generateExploitInput(S2EExecutionState *state, klee::ExecutionState *exploit_state)
{
    if(debug_mode) {
        (*call_stream) << "Enter generateExploitInput().\n";
        call_stream->flush();
    }

    solution_ty solution;
    bool solve_result = s2e()->getExecutor()->getSymbolicSolution(*exploit_state, solution);
    if(!solve_result) {
        if(debug_mode) {
            (*call_stream) << "Leave generateExploitInput() - false.\n";
            call_stream->flush();
        }

        pc_result = 1;
        s2e()->getWarningsStream() << "Warning: Could not get symbolic solution.\n";
        return false;
    }

    std::stringstream filename, copyname;
    filename << "TestCase_" << case_id << ".bin";
    copyname << "CopyCase_" << case_id << ".bin";
    std::ofstream fout(s2e()->getOutputFilename(filename.str()).c_str(), std::ios::out | std::ios::binary), fcopy;
    if(input_copy) fcopy.open(s2e()->getOutputFilename(copyname.str()).c_str(), std::ios::out | std::ios::binary);

    solution_ty::iterator sol_it;
    size_t val_sz;
    for(sol_it = solution.begin(); sol_it != solution.end(); sol_it++) {
        val_sz = sol_it->second.size();
        for(unsigned int i = 0; i < val_sz; i++) {
            fout.put((unsigned)sol_it->second[i]);
            if(input_copy) fcopy.put((unsigned)sol_it->second[i]);
        }
        fout.put(' ');
        if(input_copy) fcopy.put(' ');
    }
    fout.close();
    if(input_copy) fcopy.close();
    sendInputGenerate();

    if(debug_mode) {
        (*call_stream) << "Leave generateExploitInput() - true.\n";
        call_stream->flush();
    }

    return true;
}


void InfoCollector::selectSchedulingAlgorithm(S2EExecutionState *state, klee::ExecutionState *exploit_state, klee::ref<klee::Expr> &args_constraints)
{
    if(debug_mode) {
        (*call_stream) << "Enter selectSchedulingAlgorithm().\n";
        call_stream->flush();
    }

    klee::ExecutionState *temp_state = new klee::ExecutionState(*exploit_state);
    temp_state->constraints.constraints.push_back(args_constraints);
    int confirm_result;
    //try all path constraints and argument constraints first
    s2e()->getMessagesStream() << "Try ALL constraints.\n";
    if(generateExploitInput(state, temp_state)) {
        s2e()->getMessagesStream() << "Verify the input from ALL constraints.\n";

        confirm_result = confirmResult();
        if(debug_mode) {
            (*try_stream) << "ALL: " << pc_result << '\n';
            try_stream->flush();
        }

        if(confirm_result == 1) {
            if(real_input_num == expect_input_num) sendAllComplete(state);
        }
    } else if(debug_mode) {
        (*try_stream) << "ALL: " << pc_result << '\n';
        try_stream->flush();
    }
    pc_result = 0;

    temp_state->constraints.constraints.clear();
    temp_state->constraints.constraints.push_back(args_constraints);
    //try null constraints (only argument constraints) second
    s2e()->getMessagesStream() << "Try NULL constraints.\n";
    if(generateExploitInput(state, temp_state)) {
        s2e()->getMessagesStream() << "Verify the input from NULL constraints.\n";

        confirm_result = confirmResult();
        if(debug_mode) {
            (*try_stream) << "NULL: " << pc_result << '\n';
            try_stream->flush();
        }

        if(confirm_result == 1) {
            if(real_input_num == expect_input_num) sendAllComplete(state);
        }
    } else if(debug_mode) {
        (*try_stream) << "ALL: " << pc_result << '\n';
        try_stream->flush();
    }
    pc_result = 0;

    //if all above stages failed, then select and execute a scheduling algorithm
    switch(algorithm) {
        case 1:
            combinationFromHead(state, exploit_state, args_constraints);
            break;
        case 2:
            combinationFromTail(state, exploit_state, args_constraints);
            break;
        case 3:
            QLearning(state, exploit_state, args_constraints);
        default:
            ; 
    }

    if(debug_mode) {
        (*call_stream) << "Leave selectSchedulingAlgorithm().\n";
        call_stream->flush();
    }
}

void InfoCollector::combinationFromHead(S2EExecutionState *state, klee::ExecutionState *exploit_state, klee::ref<klee::Expr> &args_constraints)
{
    if(debug_mode) {
        (*call_stream) << "Enter combinationFromHead().\n";
        call_stream->flush();
    }
    s2e()->getMessagesStream() << "Start Algo. 1\n";

    unsigned int n = constraints_size;
    int confirm_result;
    klee::ExecutionState *temp_state = new klee::ExecutionState(*exploit_state);
    for(unsigned int r = 1; r <= n; r++) {
        std::vector<bool> not_select_vec(n, true);
        std::fill(not_select_vec.begin(), not_select_vec.begin() + r, false);

        std::vector<unsigned short int> index_vec;
        do {
            temp_state->constraints.constraints.clear();

            for(unsigned int i = 0; i < n; i++) {
                if(!not_select_vec[i]) {
                    temp_state->constraints.constraints.push_back((constraints_info[i]).raw_constraint);

                    if(debug_mode) {
                        index_vec.push_back(i + 1);
                    }
                }
            }

            temp_state->constraints.constraints.push_back(args_constraints);

            if(generateExploitInput(state, temp_state)) {
                confirm_result = confirmResult();
                if(debug_mode) {
                    (*try_stream) << "PC";
                    for(unsigned int i = 0; i < index_vec.size(); i++) {
                        (*try_stream) << index_vec[i];
                        if(i != index_vec.size() - 1) (*try_stream) << ' ';
                    }
                    (*try_stream) << ": " << pc_result << '\n';
                    try_stream->flush();
                }

                if(confirm_result == 1) {
                    if(real_input_num == expect_input_num) {
                        delete temp_state;
                        sendAllComplete(state);
                    }
                }
            } else if(debug_mode) {
                (*try_stream) << "PC";
                for(unsigned int i = 0; i < index_vec.size(); i++) {
                    (*try_stream) << index_vec[i] << ' ';
                }
                (*try_stream) << ": " << pc_result << '\n';
                try_stream->flush();
            }
            pc_result = 0;

            if(debug_mode) index_vec.clear();
        } while(std::next_permutation(not_select_vec.begin(), not_select_vec.end()));
    }

    if(debug_mode) {
        (*call_stream) << "Leave combinationFromHead().\n";
        call_stream->flush();
    }
    s2e()->getWarningsStream() << "Warning: Tried all possible combinations but not success.\n";
    delete temp_state;
}

void InfoCollector::combinationFromTail(S2EExecutionState *state, klee::ExecutionState *exploit_state, klee::ref<klee::Expr> &args_constraints)
{
    if(debug_mode) {
        (*call_stream) << "Enter combinationFromTail().\n";
        call_stream->flush();
    }
    s2e()->getMessagesStream() << "Start Algo. 2\n";

    unsigned int n = constraints_size;
    int confirm_result;
    klee::ExecutionState *temp_state = new klee::ExecutionState(*exploit_state);
    for(unsigned int r = 1; r <= n; r++) {
        std::vector<bool> select_vec(n, false);
        std::fill(select_vec.begin() + n - r, select_vec.end(), true);

        std::vector<unsigned short int> index_vec;
        do {
            temp_state->constraints.constraints.clear();

            for(unsigned int i = 0; i < n; i++) {
                if(select_vec[i]) {
                    temp_state->constraints.constraints.push_back((constraints_info[i]).raw_constraint);

                    if(debug_mode) {
                        index_vec.push_back(i + 1);
                    }
                }
            }

            temp_state->constraints.constraints.push_back(args_constraints);

            if(generateExploitInput(state, temp_state)) {
                confirm_result = confirmResult();
                if(debug_mode) {
                    (*try_stream) << "PC";
                    for(unsigned int i = 0; i < index_vec.size(); i++) {
                        (*try_stream) << index_vec[i];
                        if(i != index_vec.size() - 1) (*try_stream) << ' ';
                    }
                    (*try_stream) << ": " << pc_result << '\n';
                    try_stream->flush();
                }

                if(confirm_result == 1) {
                    if(real_input_num == expect_input_num) {
                        delete temp_state;
                        sendAllComplete(state);
                    }
                }
            } else if(debug_mode) {
                (*try_stream) << "PC";
                for(unsigned int i = 0; i < index_vec.size(); i++) {
                    (*try_stream) << index_vec[i] << ' ';
                }
                (*try_stream) << ": " << pc_result << '\n';
                try_stream->flush();
            }
            pc_result = 0;

            if(debug_mode) index_vec.clear();
        } while(std::next_permutation(select_vec.begin(), select_vec.end()));
    }

    if(debug_mode) {
        (*call_stream) << "Leave combinationFromTail().\n";
        call_stream->flush();
    }
    s2e()->getWarningsStream() << "Warning: Tried all possible combinations but not success.\n";
    delete temp_state;
}

void InfoCollector::QLearning(S2EExecutionState *state, klee::ExecutionState *exploit_state, klee::ref<klee::Expr> &args_constraints)
{
    if(debug_mode) {
        (*call_stream) << "Enter QLearning().\n";
        call_stream->flush();
    }

    //get feature weights and other needed infomations
    readQLearningResult();

    pc_combination_ty pc_combination;
    double f_score;
    PSPair pcc_score_pair;
    std::vector<PSPair> pcc_score_vec;
    //the vector map index to P.C. level id
    std::vector<unsigned int> pc_level_vec;
    unsigned int level_index = 0;
    for(unsigned int i = 0; i < constraints_size ; i++) {
        pc_level_vec.push_back(level_index);
        if((i + 1) % pc_level_size == 0) level_index++;
    }
    //caculate scores for all P.C. combinations
    for(unsigned int l = 1; l <= max_pc_len; l++) {
        std::vector<bool> rev_select_vec(constraints_size, true);
        std::fill(rev_select_vec.begin(), rev_select_vec.begin() + l, false);

        do {
            f_score = 0;

            for(unsigned int i = 0; i < constraints_size; i++) {
                if(!rev_select_vec[i]) {
                    pc_combination.push_back(i);
                    level_index = pc_level_vec[i];
                    if(level_index < weight_vec.size()) f_score += weight_vec[level_index];
                }
            }
            pcc_score_pair.pc_vec = pc_combination;
            pcc_score_pair.score = f_score;
            pcc_score_vec.push_back(pcc_score_pair);

            pc_combination.clear();
        } while(std::next_permutation(rev_select_vec.begin(), rev_select_vec.end()));
    }
    //sort vector by feature scores in ascending order
    std::sort(pcc_score_vec.begin(), pcc_score_vec.end());

    //P.C. scheduling by that order
    int confirm_result;
    klee::ExecutionState *temp_state = new klee::ExecutionState(*exploit_state);
    pc_combination_ty::const_iterator pcc_it;
    for(std::vector<PSPair>::const_reverse_iterator psp_it = pcc_score_vec.rbegin(); psp_it != pcc_score_vec.rend(); psp_it++) {
        temp_state->constraints.constraints.clear();
        for(pcc_it = psp_it->pc_vec.begin(); pcc_it != psp_it->pc_vec.end(); pcc_it++) {
            temp_state->constraints.constraints.push_back((constraints_info[*pcc_it]).raw_constraint);
        }

        temp_state->constraints.constraints.push_back(args_constraints);

        if(generateExploitInput(state, temp_state)) {
            confirm_result = confirmResult();
            if(confirm_result == 1) {
                if(real_input_num == expect_input_num) {
                    delete temp_state;
                    sendAllComplete(state);
                }
            }
        }
    }

    if(debug_mode) {
        (*call_stream) << "Leave QLearning().\n";
        call_stream->flush();
    }
    s2e()->getWarningsStream() << "Warning: Tried all combinations in pre-decided range but not success.\n";
    delete temp_state;
}


void InfoCollector::sendInputGenFail()
{
    if(debug_mode) {
        (*call_stream) << "Enter sendInputGenFail().\n";
        call_stream->flush();
    }

    llvm::raw_ostream *input_out = s2e()->openOutputFile("input_gf.txt");
    delete input_out;

    if(debug_mode) {
        (*call_stream) << "Leave sendInputGenFail().\n";
        call_stream->flush();
    }
}

void InfoCollector::sendInputGenerate()
{
    if(debug_mode) {
        (*call_stream) << "Enter sendInputGenerate().\n";
        call_stream->flush();
    }

    llvm::raw_ostream *input_out = s2e()->openOutputFile("input_g.txt");
    delete input_out;

    if(debug_mode) {
        (*call_stream) << "Leave sendInputGenerate().\n";
        call_stream->flush();
    }
}

int InfoCollector::confirmResult()
{
    if(debug_mode) {
        (*call_stream) << "Enter confirmResult().\n";
        call_stream->flush();
    }

    struct stat file_state;
    while(true) {
        //confirm successful result
        if(stat(s2e()->getOutputFilename("success.txt").c_str(), &file_state) == 0) {
            real_input_num++;
            case_id++;

            sendConfirmEnd();

            if(debug_mode) {

                (*call_stream) << "Leave confirmResult() - 1.\n";
                call_stream->flush();
            }

            pc_result = 4;
            return 1;
        }
        //comfirm failed result
        if(stat(s2e()->getOutputFilename("fail.txt").c_str(), &file_state) == 0) {
            sendConfirmEnd();

            if(debug_mode) {
                (*call_stream) << "Leave confirmResult() - 0.\n";
                call_stream->flush();
            }

            pc_result = 2;
            return 0;
        }
        //no file of result
        if(stat(s2e()->getOutputFilename("no_file.txt").c_str(), &file_state) == 0) {
            sendConfirmEnd();

            if(debug_mode) {
                (*call_stream) << "Leave confirmResult() - -1.\n";
                call_stream->flush();
            }

            pc_result = 3;
            return -1;
        }
    }
}

void InfoCollector::sendConfirmEnd()
{
    if(debug_mode) {
        (*call_stream) << "Enter sendConfirmEnd().\n";
        call_stream->flush();
    }

    llvm::raw_ostream *confirm_out = s2e()->openOutputFile("confirm.txt");
    delete confirm_out;

    if(debug_mode) {
        (*call_stream) << "Leave sendConfirmEnd().\n";
        call_stream->flush();
    }
}

void InfoCollector::sendAllComplete(S2EExecutionState *state)
{
    if(debug_mode) {
        (*call_stream) << "Enter sendAllComplete().\n";
        call_stream->flush();
    }
    s2e()->getMessagesStream() << "All complete!\n";

    llvm::raw_ostream *complete_out = s2e()->openOutputFile("complete.txt");
    delete complete_out;

    if(debug_mode) {
        (*call_stream) << "Leave sendAllComplete().\n";
        call_stream->flush();
    }
    //s2e()->getExecutor()->terminateStateEarly(*state, "");
    infoDestructor();
}

/*void InfoCollector::sendStartQLearning()
{
    if(debug_mode) {
        (*call_stream) << "Enter sendStartQLearning()\n";
        call_stream->flush();
    }

    llvm::raw_ostream *start_ql = s2e()->openOutputFile("start_ql.txt");
    delete start_ql;

    if(debug_mode) {
        (*call_stream) << "Leave sendStartQLearning()\n";
        call_stream->flush();
    }
}*/

void InfoCollector::readQLearningResult()
{
    if(debug_mode) {
        (*call_stream) << "Enter readQLearningResult()\n";
        call_stream->flush();
    }

    //hard code
    std::string fw_file_path("/home/chengte/SCraxF/RL_codes/result"), fw_file_name;
    std::stringstream fw_file_name_ss;
    fw_file_name_ss << "feature_weight";
    fw_file_name_ss << std::setw(3) << std::setfill('0') << learning_result_id;
    fw_file_name_ss << ".txt";
    fw_file_name_ss >> fw_file_name;
    fw_file_path += fw_file_name;

    std::ifstream fw_file(fw_file_path.c_str());
    double f_weight;
    fw_file >> max_pc_len >> pc_level_size;
    while(!fw_file.eof()) {
        fw_file >> f_weight;
        weight_vec.push_back(f_weight);
    }
    fw_file.close();

    if(debug_mode) {
        (*call_stream) << "Leave readQLearningResult()\n";
        call_stream->flush();
    }
}

}
}
