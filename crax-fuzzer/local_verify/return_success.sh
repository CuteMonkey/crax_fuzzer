#!/bin/bash

scraxf_root=/home/chengte/SCraxF
verify_dir=${scraxf_root}/crax-fuzzer/local_verify
out_dir=${scraxf_root}/s2e_out/s2e-last
rl_dir=${scraxf_root}/RL_codes

cp ${verify_dir}/success.txt ${out_dir}
if [ -f ${verify_dir}/start_ql.txt ]; then
    cp ${verify_dir}/success.txt ${rl_dir}
fi
