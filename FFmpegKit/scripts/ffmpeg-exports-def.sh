#!/usr/bin/env bash
#
# Writes a .def that exports the FFmpeg API (av*, swr*, sws*) defined in the
# given static libraries. Data symbols get DATA, so no call thunk is made for them.
#
#   ffmpeg-exports-def.sh <nm> <output.def> <lib.a>...

set -euo pipefail

nm="$1"
output="$2"
shift 2

{
    echo "EXPORTS"
    "$nm" -g --defined-only "$@" |
        awk 'NF == 3 && $3 ~ /^(av|swr|sws)/ { print $3 ($2 ~ /^[BDGRS]$/ ? " DATA" : "") }' |
        sort -u
} > "$output"
