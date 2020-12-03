IN_DIR=/w/fatture
OUT_DIR=/w/letture

make layouts
make build=release reader

mkdir -p "$OUT_DIR"
for d in $IN_DIR/*; do
    echo "======== $(basename "$d") ========"
    python read.py "$d" "$OUT_DIR/$(basename "$d").json" "layout/controllo.out"
    python export.py "$OUT_DIR/$(basename "$d").json"
done