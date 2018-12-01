#!/usr/bin/env bash
set -eo pipefail

WORKING_DIR=$(dirname $(readlink -e "$BASH_SOURCE"))
BUILD_ROOT=$(readlink -e "$WORKING_DIR/..")
VERSION=$(cat "$BUILD_ROOT/build/linux_generic/rfs/version" 2> /dev/null || echo "2018.1.0-nightly")
RULES="$WORKING_DIR/debian/rules"
export AGENT_VERISON="$VERSION"

# Build unum first since we need help text to generate the manpage
cd "$BUILD_ROOT"
make clean
make AGENT_VERSION="$VERSION"

export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$BUILD_ROOT/build/linux_generic/rfs/lib"
help2man --no-discard-stderr "$BUILD_ROOT/build/linux_generic/rfs/bin/unum" > "$WORKING_DIR/debian/manpage.1"

ln -sf "$WORKING_DIR/debian" "$BUILD_ROOT/debian"
trap "rm -f \"$BUILD_ROOT\"/debian; trap - EXIT" EXIT

dh binary-arch
