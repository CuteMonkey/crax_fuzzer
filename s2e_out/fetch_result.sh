#!/bin/bash
#Read the certain information of result of crax fuzzer execution.

grep --before-context=1 "true" s2e-last/messages.txt
echo "--------"
grep "Combination =" s2e-last/debug.txt
