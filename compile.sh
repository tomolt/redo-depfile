#!/bin/sh
test "$CC" || CC="cc"
test "$CFLAGS" || CFLAGS="-Wall -Wextra -pedantic -g"
SRC="redo-depfile.c"
OUT="redo-depfile"
"$CC" $CFLAGS "$SRC" -o "$OUT"
