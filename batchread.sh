cd build
make compiler
cd ..

mkdir -p work/layouts
for f in layouts/*.bls; do
    out_file="work/layouts/$(basename "$f" .bls).out"
    [ "$f" -nt "$out_file" ] && build/compiler -o "$out_file" "$f"
done

mkdir -p work/letture
for d in work/fatture/*; do
    echo "======== $(basename "$d") ========"
    python read.py "$d" "$OUT_DIR/$(basename "$d").json"
    python export.py "$OUT_DIR/$(basename "$d").json"
done
