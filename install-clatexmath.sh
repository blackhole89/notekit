#!/bin/bash
git clone https://github.com/NanoMichael/cLaTeXMath.git
cd cLaTeXMath
git reset --hard HEAD
# patch out C++17 feature use in code, but compile with -std=c++17
(cat <<EOF
diff --git a/CMakeLists.txt b/CMakeLists.txt
index 9654360..a070437 100644
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
diff --git a/src/config.h b/src/config.h
index b43a395..d8aabe5 100644
--- a/src/config.h
+++ b/src/config.h
@@ -40,10 +40,6 @@
 #include "vcruntime.h"
 #endif
 
-#if (__cplusplus >= 201703L) || (defined(_MSC_VER) && defined(_HAS_CXX17) && _HAS_CXX17)
-#define CLATEX_CXX17 1
-#else
 #define CLATEX_CXX17 0
-#endif
 
 #endif  // CONFIG_H_INCLUDED
EOF
) | patch -p1
cmake -DCMAKE_BUILD_TYPE=Release -DHAVE_LOG=OFF -DGRAPHICS_DEBUG=OFF .
make
cp -r res ../data/latex
