#!/bin/bash
#Usage:./xmail.sh <test_case_number>

case_num=${1}
verify_dir=/home/chengte/ch_verify
target_dir=/home/chengte/verify_pool/xmail-1.21

export MAIL_ROOT=${target_dir}/MailRoot

${verify_dir}/get_testcases.exp
${verify_dir}/stdin `cat ${verify_dir}/TestCases/TestCase_${case_num}.bin` ${target_dir}/bin/sendmail -t
${verify_dir}/return_result.sh
