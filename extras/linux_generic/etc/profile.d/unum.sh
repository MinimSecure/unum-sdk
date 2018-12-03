#!/bin/sh

export UNUM_INSTALL_ROOT="/opt/unum"

# Ensure that Unum and its dependencies have the correct library path.
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$UNUM_INSTALL_ROOT/lib/"
# Ensure the current PATH contains the unum binary.
export PATH="$PATH:$UNUM_INSTALL_ROOT/bin"
