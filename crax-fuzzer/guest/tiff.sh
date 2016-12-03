#!/bin/sh
export LD_LIBRARY_PATH=../pool/tiff-v3.6.1/libtiff/
#mv s2e_input input.tif
#./symfile ../pool/tiff-v3.6.1/sample18.tif 0 18278 ../pool/tiff-v3.6.1/tools/tiffinfo ../pool/tiff-v3.6.1/sample18.tif
./symfile2 ../pool/tiff-v3.6.1/sample46.tif 130 100 ../pool/tiff-v3.6.1/tools/tiffinfo ../pool/tiff-v3.6.1/sample46.tif
