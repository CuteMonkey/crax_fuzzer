#!/bin/bash

case_num=${1}
verify_dir=/home/chengte/ch_verify
target_dir=/home/chengte/verify_pool/sudo-1.8.0

${verify_dir}/get_testcases.exp
${verify_dir}/args 2 ${target_dir}/src/sudo -D9 
${verify_dir}/return_result.sh
