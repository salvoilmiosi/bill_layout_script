#!/bin/bash
parent_path=$( cd "$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )

cd "$parent_path"

IN_DIR=../layouts
OUT_DIR=../work/layouts

mkdir -p $OUT_DIR
for f in $IN_DIR/*.bls; do
    out_file="$OUT_DIR/$(basename "$f" .bls).out"
    if [[ "$f" -nt "$out_file" ]]; then
        echo "Compilo $(basename "$f")"
        ../build/compiler -o "$out_file" "$f"
    fi
done
