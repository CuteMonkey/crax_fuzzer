#!/bin/bash

case_num=${1}
verify_dir=/home/chengte/SCraxF/crax-fuzzer/local_verify
taint_dir=/home/chengte/local_pool/taint_test

${verify_dir}/get_testcases.sh
${verify_dir}/stdin ${taint_dir}/taint_t \
`cat ${verify_dir}/TestCases/TestCase_${case_num}.bin`
${verify_dir}/return_result.sh
