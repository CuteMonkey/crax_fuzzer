#!/bin/bash
#Usage:./ncompress.sh <test_case_number>

case_num=${1}
verify_dir=/home/chengte/SCraxF/crax-fuzzer/local_verify
testcase=${verify_dir}/TestCases/TestCase_${case_num}.bin
ncompress_root=/home/chengte/local_pool/ncompress-4.2.4

${verify_dir}/get_testcases.sh
cat ${testcase} > verify_result.txt
echo -e "\n------------" >> verify_result.txt
${verify_dir}/stdin ${ncompress_root}/local/bin/compress \
`cat ${testcase}` >> verify_result.txt
${verify_dir}/return_result.sh
