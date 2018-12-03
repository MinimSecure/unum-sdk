#!/usr/bin/env bash
set -eo pipefail

WORKING_DIR=$(dirname $(readlink -e "$BASH_SOURCE"))
BUILD_ROOT=$(readlink -e "$WORKING_DIR/..")


VERSION="2018.1.0"

export AGENT_VERSION="$VERSION"
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$BUILD_ROOT/build/linux_generic/rfs/lib"
ln -sf "$WORKING_DIR/debian" "$BUILD_ROOT/debian"
trap "rm -f \"$BUILD_ROOT\"/debian; trap - EXIT" EXIT

dh clean
dh binary-arch
