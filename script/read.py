import subprocess
import sys
import json
import datetime
import time
import os
from pathlib import Path

out = []

def read_file(pdf_file):
    rel_path = pdf_file.relative_to(input_directory)
    print(rel_path)

    args = [app_dir.joinpath('../bin/release/layout_reader'), '-p', pdf_file, '-s', file_layout]
    proc = subprocess.run(args, capture_output=True, text=True)

    file_obj = {'filename':str(rel_path), 'lastmodified':os.stat(str(path)).st_mtime}

    try:
        file_obj['values'] = json.loads(proc.stdout)['values']
    except:
        file_obj['error'] = proc.stderr

    out.append(file_obj)

if len(sys.argv) < 2:
    print('Argomenti richiesti: input_directory [script_controllo.out] [output.json]')
    sys.exit()

app_dir = Path(sys.argv[0]).parent

input_directory = Path(sys.argv[1])
file_layout = Path(sys.argv[2]) if len(sys.argv) >= 3 else app_dir.joinpath('../layout/controllo.out')
output_file = Path(sys.argv[3]) if len(sys.argv) >= 4 else app_dir.joinpath('out/{0}.json'.format(input_directory.name))

if not input_directory.exists():
    print('La directory {0} non esiste'.format(input_directory))
    sys.exit(1)

if not file_layout.exists():
    print('Il file di layout {0} non esiste'.format(file_layout))
    sys.exit(1)

for path in input_directory.rglob('*.pdf'):
    read_file(path)

with open(output_file, 'w') as fout:
    fout.write(json.dumps(out))