#!/bin/bash

imagemagick_dir=/home/chengte/ImageMagick-5.2.0
case_num=${1}

./get_testcases.exp
./stdin ${imagemagick_dir}/identify -verbose \
`cat TestCases/TestCase_${case_num}.bin` > verify.log
./return_result.sh
