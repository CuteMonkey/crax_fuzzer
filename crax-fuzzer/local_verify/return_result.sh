#!/bin/bash

verify_dir=/home/chengte/SCraxF/crax-fuzzer/local_verify

if [ -f ${verify_dir}/success.txt ]; then
    #echo "Verified successfully."
    ${verify_dir}/return_success.sh
elif [ -f ${verify_dir}/fail.txt ]; then
    #echo "Verified fail."
    ${verify_dir}/return_fail.sh
else
    #echo "Warn: No file generated."
    ${verify_dir}/return_no_file.sh
fi
${verify_dir}/clean_all.sh
