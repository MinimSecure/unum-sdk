#!/usr/bin/env bash
set -eo pipefail

WORKING_DIR=$(dirname $(readlink -e "$BASH_SOURCE"))
BUILD_ROOT=$(readlink -e "$WORKING_DIR/..")

VERSION_BASE="2018.1.1"
VERSION=${1:-"${VERSION_BASE}-snapshot"}

export AGENT_VERSION="$VERSION"
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$BUILD_ROOT/build/linux_generic/rfs/lib"

envsubst < "$WORKING_DIR"/debian/changelog.base > "$WORKING_DIR"/debian/changelog

ln -sf "$WORKING_DIR/debian" "$BUILD_ROOT/debian"
trap "rm -f \"$BUILD_ROOT\"/debian; trap - EXIT" EXIT

dh clean
dh binary-arch
