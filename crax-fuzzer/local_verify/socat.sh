#!/bin/bash
#Usage:./socat.sh <test_case_number>

case_num=${1}
verify_dir=/home/chengte/SCraxF/crax-fuzzer/local_verify
testcase=${verify_dir}/TestCases/TestCase_${case_num}.bin
socat_root=/home/chengte/local_pool/socat-1.4

${verify_dir}/get_testcases.sh
cat ${testcase} >> verify_result.txt
echo -e "\n------------" >> verify_result.txt
${verify_dir}/stdin ${socat_root}/socat -ly`cat ${testcase}`
${verify_dir}/return_result.sh
