#!/bin/bash

cd "$(dirname "${BASH_SOURCE[0]}")"

IN_DIR=../layouts

for f in $(find "$IN_DIR" -name *.bls); do
    echo "$(realpath --relative-to="$IN_DIR" "$f")"
    ../build/blsdump "$f" > /dev/null
done