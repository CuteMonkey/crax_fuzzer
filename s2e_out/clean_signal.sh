#!/bin/bash
#Clear all the signal files from guest edge or from verify edge or from local verify.

#echo "Clear signal files"
rm -f s2e-last/input_g.txt s2e-last/success.txt s2e-last/fail.txt s2e-last/confirm.txt \
s2e-last/no_file.txt s2e-last/action.txt
