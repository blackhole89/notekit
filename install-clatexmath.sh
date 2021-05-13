#!/bin/bash
git clone https://github.com/NanoMichael/cLaTeXMath.git
cd cLaTeXMath
git reset --hard 7cbc0b9e4185da3a7f633473a794ee1ef7b371e8
echo 'add_library(clatexmath STATIC ${SRC})' >> CMakeLists.txt
cmake -DCMAKE_BUILD_TYPE=Release -DHAVE_LOG=OFF -DGRAPHICS_DEBUG=OFF .
make
cp -r res ../data/latex
 
