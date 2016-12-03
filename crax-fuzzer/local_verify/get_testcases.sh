#!/bin/bash

testcase_dir=/home/chengte/SCraxF/s2e_out/s2e-last
verify_dir=/home/chengte/SCraxF/crax-fuzzer/local_verify

mv ${testcase_dir}/TestCase_*.bin ${verify_dir}/TestCases
rm -f ${testcase_dir}/input_g.txt
#echo "Get TestCase"
