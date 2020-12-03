if [ -d "$1" ]; then
    make layouts
    make build=release reader
    mkdir -p "/w/letture"
    for d in $1/*; do
        echo "======== $(basename "$d") ========"
        python read.py "$d" "layout/controllo.out" "/w/letture/$(basename "$d").json"
    done
    echo "======== export ========"
    python export.py
else
    echo "Specificare directory input"
fi