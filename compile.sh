#!/bin/sh
test "$CC" || CC="cc"
test "$CFLAGS" || CFLAGS="-Wall -Wextra -pedantic -std=c99 -Os"
SRC="redo-depfile.c"
OUT="redo-depfile"
"$CC" $CFLAGS "$SRC" -o "$OUT"
