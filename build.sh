#!/bin/bash

START_DIR="$PWD"

MY_DIR=$(dirname "$0")
cd "$MY_DIR"
ROOT_DIR="$PWD"

_onexit() {
   cd "$START_DIR"
}
trap _onexit EXIT SIGHUP SIGINT SIGTERM

make $*
