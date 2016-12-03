#!/bin/bash
#Usage:./ncompress.sh <test_case_number>

case_num=${1}
verify_dir=/home/chengte/ch_verify
target_dir=/home/chengte/verify_pool/ncompress-4.2.4

${verify_dir}/get_testcases.exp
${verify_dir}/args ${target_dir}/local/bin/compress \
`cat ${verify_dir}/TestCases/TestCase_${case_num}.bin`
${verify_dir}/return_result.sh
