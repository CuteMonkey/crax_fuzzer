#!/bin/bash
#Usage:./socat.sh <test_case_number>

socat_root=/home/chengte/socat-1.4
case_num=${1}

./get_testcases.exp
./args ${socat_root}/socat \
-ly`cat TestCases/TestCase_${case_num}.bin`
./return_result.sh
