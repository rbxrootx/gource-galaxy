#!/bin/bash
# Build gource-galaxy in an MSYS2 UCRT64 shell. Output: /tmp/g/gource.exe
#
# Prerequisites (pacman, UCRT64):
#   mingw-w64-ucrt-x86_64-{gcc,SDL2,SDL2_image,glew,glm,boost,freetype,pcre2,libpng,libjpeg-turbo,pkgconf}
#   autoconf automake libtool make
set -e
SRC="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)/src"
rm -rf /tmp/g && mkdir -p /tmp/g
cp -r "$SRC"/. /tmp/g/
cd /tmp/g
[ -f configure ] || ./autogen.sh
# The ac_cv_* values skip configure link checks that fail on current MSYS2
# (SDL2_image 2.8 / Boost 1.9x) even though the libraries are present.
./configure \
  ac_cv_lib_boost_filesystem_mt_exit=yes ac_cv_func_IMG_LoadPNG_RW=yes ac_cv_func_IMG_LoadJPG_RW=yes \
  LIBS=-lSDL2_image --with-boost=/ucrt64 --with-boost-libdir=/ucrt64/lib --disable-dependency-tracking \
  CXXFLAGS="-O2 -std=gnu++17 -Wno-deprecated-declarations" LDFLAGS="-static-libgcc -static-libstdc++" 2>&1 | tail -15
make -j8 2>&1 | grep -E "error|Error|undefined" -A4 | head -80 || true
ls -la gource.exe
