from multiprocessing.pool import ThreadPool
from pathlib import Path
from collections import Counter
import subprocess
import sys
import json

app_dir = Path(sys.argv[0]).parent
layout_reader = app_dir.joinpath('../build/reader')
controllo = app_dir.joinpath('../work/layouts/controllo.out')
input_directory = app_dir.joinpath('../work/fatture')

def check_layout(pdf_file):
    args = [layout_reader, '-p', pdf_file, controllo]
    proc = subprocess.run(args, capture_output=True, text=True)

    print(pdf_file.relative_to(input_directory))

    try:
        json_out = json.loads(proc.stdout)
        if json_out['error']:
            return None
        elif 'layout' in json_out['globals']:
            return json_out['globals']['layout'][0]
    except:
        return None

files = list(input_directory.rglob('*.pdf'))

with ThreadPool(min(len(files), 5)) as pool:
    out = pool.map(check_layout, files)

print(Counter(out))