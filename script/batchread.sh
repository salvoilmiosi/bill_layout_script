if [ -d "$1" ]; then
    make layouts
    make build=release reader
    mkdir -p "$(dirname "$0")/out"
    for d in $1/*; do
        echo "======== $(basename "$d") ========"
        python "$(dirname "$0")/read.py" "$d"
    done
else
    echo "Specificare directory input"
fi