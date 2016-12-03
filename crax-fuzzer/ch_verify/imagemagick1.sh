#!/bin/bash

case_num=${1}
verify_dir=/home/chengte/ch_verify
target_dir=/home/chengte/verify_pool/ImageMagick-5.2.0

${verify_dir}/get_testcases.exp
${verify_dir}/args ${target_dir}/identify -verbose \
`cat ${verify_dir}/TestCases/TestCase_${case_num}.bin`
${verify_dir}/return_result.sh
