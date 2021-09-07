# redo-depfile
Automatic C/C++ header dependency tracking for the redo build-system.

Modern C compilers can generate partial Makefiles that specify the dependency relations between C source files and the header files they include.
`redo-depfile` is a companion / extension to redo that can read such dependency-only Makefiles and feed them into redo.

(It does this by calling `redo-ifchange` on all the declared dependencies it can find in a given dependency file.)

Example usage in a typical `default.o.do`:

```
CC=cc
CFLAGS=-g
redo-ifchange "$2.c"
$CC $CFLAGS -c "$2.c" -o "$3" -MMD -MF "$2.d"
redo-depfile "$2.d"
```

`redo-depfile` should be compatible with any redo variant, as long as it implements `redo-ifchange` as a separate executable (as opposed to a shell alias).

Altough `redo-depfile` only understands a subset of Makefile syntax, its parser is reasonably robust and can be extended to cover more syntax features as needed.

## Installation
Simply execute `compile.sh` followed by `install.sh`.

(Since `redo-depfile` is so tiny, it doesn't even make sense to build it with redo.)
