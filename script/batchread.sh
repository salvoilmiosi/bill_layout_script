#!/bin/bash
parent_path=$( cd "$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )

cd "$parent_path"

./makelayouts.sh

IN_DIR=../work/fatture
OUT_DIR=../work/letture

start=$(date +%s)

mkdir -p $OUT_DIR
for d in $IN_DIR/*; do
    echo "======== $(basename "$d") ========"
    python read.py "$d" "$OUT_DIR/$(basename "$d").json"
    python export.py "$OUT_DIR/$(basename "$d").json"
done

runtime=$(($(date +%s)-start))
echo "======== Finito in $((runtime/60))m $((runtime%60))s ========"