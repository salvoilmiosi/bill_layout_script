#!/bin/bash

cd "$(dirname "${BASH_SOURCE[0]}")"

IN_DIR=../layouts

for f in $(find "$IN_DIR" -name *.bls); do
    echo -e "\033[32m$(realpath --relative-to="$IN_DIR" "$f")\033[0m"
    ../build/blsdump "$f" > /dev/null
done