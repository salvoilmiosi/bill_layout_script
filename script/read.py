import subprocess
import sys
import json
import datetime
import time
import os
from pathlib import Path

out = []
old_out = []

def read_pdf(pdf_file):
    rel_path = pdf_file.relative_to(input_directory)
    print(rel_path)

    # Rilegge i vecchi file solo se il layout e' stato ricompilato
    for x in old_out:
        if x['filename'] == str(rel_path):
            args = [app_dir.joinpath('../bin/release/layout_reader'), '-p', pdf_file, controllo]
            proc = subprocess.run(args, capture_output=True, text=True)
            json_out = json.loads(proc.stdout)
            if not json_out['error'] and 'layout' in json_out['globals']:
                layout_file = controllo.parent.joinpath(json_out['globals']['layout']).with_suffix('.out')
                if os.path.getmtime(str(layout_file)) < os.path.getmtime(str(output_file)):
                    out.append(x)
                    return

    args = [app_dir.joinpath('../bin/release/layout_reader'), '-p', pdf_file, '-s', controllo]
    proc = subprocess.run(args, capture_output=True, text=True)

    file_obj = {'filename':str(rel_path), 'lastmodified':os.stat(str(path)).st_mtime}

    json_out = json.loads(proc.stdout)
    if json_out['error']:
        file_obj['error'] = json_out['message']
    else:
        file_obj['values'] = ['values']

    out.append(file_obj)

if len(sys.argv) < 2:
    print('Argomenti richiesti: input_directory [script_controllo.out] [output.json]')
    sys.exit()

app_dir = Path(sys.argv[0]).parent

input_directory = Path(sys.argv[1])
controllo = Path(sys.argv[2]) if len(sys.argv) >= 3 else app_dir.joinpath('../layout/controllo.out')
output_file = Path(sys.argv[3]) if len(sys.argv) >= 4 else app_dir.joinpath('out/{0}.json'.format(input_directory.name))

if output_file.exists():
    with open(output_file, 'r') as fin:
        old_out = json.loads(fin.read())

if not input_directory.exists():
    print('La directory {0} non esiste'.format(input_directory))
    sys.exit(1)

if not controllo.exists():
    print('Il file di layout {0} non esiste'.format(controllo))
    sys.exit(1)

for path in input_directory.rglob('*.pdf'):
    read_pdf(path)

with open(output_file, 'w') as fout:
    fout.write(json.dumps(out))