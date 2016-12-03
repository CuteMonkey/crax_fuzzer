#!/bin/bash

calltom_root=/home/chengte/verify_pool/calltom_mod
case_num=${1}

./get_testcases.exp
./args ${calltom_root}/calltom `cat TestCases/TestCase_${case_num}.bin`
./return_result.sh
