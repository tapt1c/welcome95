#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
if command -v i686-w64-mingw32-gcc >/dev/null 2>&1; then
    compiler=i686-w64-mingw32-gcc
elif [ -x /workspace/.tools/mingw/usr/bin/i686-w64-mingw32-gcc-win32 ]; then
    compiler=/workspace/.tools/mingw/usr/bin/i686-w64-mingw32-gcc-win32
else
    echo 'Install the i686 MinGW-w64 compiler (see README.md).' >&2
    exit 1
fi
make CC="$compiler" "$@"
