#!/bin/bash

cd "$(dirname "${BASH_SOURCE[0]}")"

IN_DIR=../work/fatture
OUT_DIR=../work/letture

start=$(date +%s)

mkdir -p $OUT_DIR
for d in $IN_DIR/*; do
    echo -e "\033[32mLettura $(basename "$d")...\033[0m"
    python read.py "$d" "$OUT_DIR/$(basename "$d").json" ${@:1}
done

runtime=$(($(date +%s)-start))
echo -e "\033[32mFinito in $((runtime/60))m $((runtime%60))s\033[0m"