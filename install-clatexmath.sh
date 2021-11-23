#!/bin/bash
git clone https://github.com/NanoMichael/cLaTeXMath.git
cd cLaTeXMath
git reset --hard HEAD
(cat <<EOF
--- a/CMakeLists.txt
+++ b/CMakeLists.txt
@@ -25,10 +25,6 @@ else ()
             # needs extra lib to use std::filesystem
             target_link_libraries(LaTeX PUBLIC "stdc++fs")
         endif ()
-        if ("\${CMAKE_CXX_COMPILER_VERSION}" VERSION_LESS 8)
-            # dose not have full c++17 features
-            set(COMPILER_SUPPORTS_CXX17 OFF)
-        endif ()
     endif ()
 
     if (COMPILER_SUPPORTS_CXX17)
EOF
) | patch -p1 -
cmake -DCMAKE_BUILD_TYPE=Release -DHAVE_LOG=OFF -DGRAPHICS_DEBUG=OFF .
make
cp -r res ../data/latex
