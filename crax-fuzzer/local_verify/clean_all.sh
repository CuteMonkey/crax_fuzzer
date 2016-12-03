#!/bin/bash
#Remove the TestCases and the result of verification.

verify_dir=/home/chengte/SCraxF/crax-fuzzer/local_verify

rm -f ${verify_dir}/TestCases/* ${verify_dir}/success.txt ${verify_dir}/fail.txt ${verify_dir}/no_file.txt
