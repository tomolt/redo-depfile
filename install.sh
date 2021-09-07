#!/bin/sh
test "$PREFIX" || PREFIX="/usr/local"
BIN_DIR="$DESTDIR$PREFIX/bin"
mkdir -p "$BIN_DIR"
cp -f "redo-depfile" "$BIN_DIR/"
chmod 755 "$BIN_DIR/redo-depfile"
