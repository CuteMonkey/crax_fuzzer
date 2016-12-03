#!/bin/bash
#Usage:./taint_t.sh <test_case_number>

case_num=${1}
verify_dir=/home/chengte/ch_verify
target_dir=/home/chengte/verify_pool/taint_test

${verify_dir}/get_testcases.exp
${verify_dir}/args ${target_dir}/taint_t \
`cat ${verify_dir}/TestCases/TestCase_${case_num}.bin`
${verify_dir}/return_result.sh
