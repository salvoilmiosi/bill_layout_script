mkdir -p work/layouts
for f in layouts/*.bls; do
    out_file="work/layouts/$(basename "$f" .bls).out"
    if [[ "$f" -nt "$out_file" ]]; then
        echo "Compiling $(basename "$f")"
        build/compiler -o "$out_file" "$f"
    fi
done
