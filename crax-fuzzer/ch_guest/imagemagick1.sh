#!/bin/bash

imagemagick_dir=/home/chengte/ImageMagick-5.2.0

./symargs 4  ${imagemagick_dir}/identify -verbose ${imagemagick_dir}/images/\
ball.png
