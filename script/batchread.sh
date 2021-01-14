#!/bin/bash
parent_path=$( cd "$(dirname "${BASH_SOURCE[0]}")" ; pwd -P )

cd "$parent_path"

./makelayouts.sh

IN_DIR=../work/fatture
OUT_DIR=../work/letture

start=$(date +%s)

YELLOW='\033[93m'
GREEN='\033[32m'
NC='\033[0m'

mkdir -p $OUT_DIR
for d in $IN_DIR/*; do
    echo -e "${YELLOW}Lettura $(basename "$d")...${NC}"
    python read.py "$d" "$OUT_DIR/$(basename "$d").json"
done

runtime=$(($(date +%s)-start))
echo -e "${GREEN}Finito in $((runtime/60))m $((runtime%60))s${NC}"