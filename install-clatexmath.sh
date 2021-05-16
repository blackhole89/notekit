#!/bin/bash
git clone https://github.com/NanoMichael/cLaTeXMath.git
cd cLaTeXMath
git reset --hard HEAD
cmake -DCMAKE_BUILD_TYPE=Release -DHAVE_LOG=OFF -DGRAPHICS_DEBUG=OFF .
make
cp -r res ../data/latex