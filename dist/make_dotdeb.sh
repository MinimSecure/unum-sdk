#!/usr/bin/env bash
set -eo pipefail

WORKING_DIR=$(dirname $(readlink -e "$BASH_SOURCE"))
BUILD_ROOT=$(readlink -e "$WORKING_DIR/..")

declare -i OPT_NO_CLEAN=
declare -i OPT_ENTER_WORKSPACE=

for arg in $@; do
    case "$arg" in
        --no-clean|-C)
            OPT_NO_CLEAN=1
            shift
            ;;
        --enter-workspace|-X)
            OPT_ENTER_WORKSPACE=1
            shift
            ;;
        *)  break
            ;;
    esac
done

VERSION_BASE="2019.2.0"
VERSION_STABILITY="stable"
VERSION=${1:-"${VERSION_BASE}-snapshot"}

export AGENT_VERSION="$VERSION"
export LD_LIBRARY_PATH="$LD_LIBRARY_PATH:$BUILD_ROOT/build/linux_generic/rfs/lib"

envsubst < "$WORKING_DIR"/debian/changelog.base > "$WORKING_DIR"/debian/changelog

if [[ ! -z "$1" ]]; then
    sed -i -e 's/UNRELEASED/'"$VERSION_STABILITY"'/g' "$WORKING_DIR"/debian/changelog
fi

ln -sf "$WORKING_DIR/debian" "$BUILD_ROOT/debian"
trap "rm -f \"$BUILD_ROOT\"/debian; trap - EXIT" EXIT

if (( OPT_ENTER_WORKSPACE )); then
    bash -l
else
    if ! (( OPT_NO_CLEAN )); then
        dh clean
    fi
    dh binary-arch
    cp -fv ../unum-aio*.deb out/linux_generic
fi
