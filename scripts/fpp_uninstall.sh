#!/bin/bash
PLUGINDIR="$(dirname "$(dirname "$(readlink -f "$0")")")"
cd "$PLUGINDIR" || exit 1
make clean "$@"
