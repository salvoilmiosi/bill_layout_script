for f in $(dirname "$0")/out/*.json; do
    echo "======== $(basename "$f" .json) ========"
    python "$(dirname "$0")/export.py" "$f"
done