for d in /c/Users/salvo/Desktop/fatture/*; do
    name=$(basename "$d")
    python script/batch.py "$d" fatture/controllo.txt "out/$name.xlsx"
done