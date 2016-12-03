#!/bin/bash
#Remove the result of verification and the TestCases.

verify_dir=/home/chengte/ch_verify

rm -f ${verify_dir}/success.txt ${verify_dir}/fail.txt ${verify_dir}/TestCases/*
