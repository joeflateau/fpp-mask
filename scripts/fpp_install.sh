#!/bin/bash
# Invoked by FPP's plugin manager as root with positional args like:
#   FPPDIR=/opt/fpp SRCDIR=/opt/fpp/src
# We forward those to make so SRCDIR/FPPDIR overrides reach the Makefile.
PLUGINDIR="$(dirname "$(dirname "$(readlink -f "$0")")")"
cd "$PLUGINDIR" || exit 1
make "$@"
