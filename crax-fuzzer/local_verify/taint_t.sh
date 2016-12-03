#!/bin/bash

case_num=${1}
verify_dir=/home/chengte/SCraxF/crax-fuzzer/local_verify
testcase=${verify_dir}/TestCases/TestCase_${case_num}.bin

taint_dir=/home/chengte/local_pool/taint_test

${verify_dir}/get_testcases.sh
cat ${testcase} > verify_result.txt
echo -e "\n------------" >> verify_result.txt
${verify_dir}/stdin ${taint_dir}/taint_t `cat ${testcase}` >> verify_result.txt
${verify_dir}/return_result.sh
