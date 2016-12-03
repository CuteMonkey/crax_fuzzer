#!/bin/bash
#Usage:./xmail.sh <test_case_number>

case_num=${1}
verify_dir=/home/chengte/SCraxF/crax-fuzzer/local_verify
testcase=${verify_dir}/TestCases/TestCase_${case_num}.bin

xmail_root=/home/chengte/local_pool/xmail-1.21
export MAIL_ROOT=${xmail_root}/MailRoot

${verify_dir}/get_testcases.sh
cat ${testcase} > verify_result.txt
echo -e "\n------------" >> verify_result.txt
${verify_dir}/stdin ${xmail_root}/bin/sendmail -t --input-file \
`cat ${testcase}` >> verify_result.txt
${verify_dir}/return_result.sh
