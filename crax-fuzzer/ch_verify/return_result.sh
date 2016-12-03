#!/bin/bash
#Return the result file of verification to host.

verify_dir=/home/chengte/ch_verify

if [ -f "${verify_dir}/success.txt" ]; then
    ${verify_dir}/return_success.exp
elif [ -f "${verify_dir}/fail.txt" ]; then
    ${verify_dir}/return_fail.exp
else
    echo "Warning: no file generated."
fi
${verify_dir}/clean_all.sh
