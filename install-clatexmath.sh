#!/bin/bash
git clone https://github.com/NanoMichael/cLaTeXMath.git
cd cLaTeXMath
git reset --hard HEAD
echo 'add_library(clatexmath STATIC ${SRC})' >> CMakeLists.txt
cmake -DCMAKE_BUILD_TYPE=Release -DHAVE_LOG=OFF -DGRAPHICS_DEBUG=OFF .
make
cp -r res ../data/latex
 
