import subprocess
import sys
import json
import datetime
import time
import os
from pathlib import Path

out = []
old_out = []

app_dir = Path(sys.argv[0]).parent
layout_reader = app_dir.joinpath('bin/release/layout_reader')

def read_pdf(pdf_file):
    rel_path = pdf_file.relative_to(input_directory)

    skipped = False

    # Rilegge i vecchi file solo se il layout e' stato ricompilato
    for x in old_out:
        if x['filename'] == str(rel_path):
            if not 'layout' in x:
                args = [layout_reader, '-p', pdf_file, controllo]
                proc = subprocess.run(args, capture_output=True, text=True)
                json_out = json.loads(proc.stdout)
                if not json_out['error'] and 'layout' in json_out['globals']:
                    x['layout'] = json_out['globals']['layout']
            if 'layout' in x and 'values' in x:
                layout_file = controllo.parent.joinpath('../bls/{0}.bls'.format(x['layout']))
                if os.path.getmtime(str(layout_file)) < os.path.getmtime(str(output_file)):
                    out.append(x)
                    skipped = True

    if not skipped:
        print(rel_path)
        args = [layout_reader, '-p', pdf_file, '-s', controllo]
        proc = subprocess.run(args, capture_output=True, text=True)

        file_obj = {'filename':str(rel_path), 'lastmodified':os.stat(str(path)).st_mtime}

        try:
            json_out = json.loads(proc.stdout)
            if json_out['error']:
                file_obj['error'] = json_out['message']
            elif 'layout' in json_out['globals']:
                file_obj['layout'] = json_out['globals']['layout']
                file_obj['values'] = json_out['values']
        except:
            file_obj['error'] = 'errore {0}'.format(proc.returncode)
            print('### Errore',rel_path)

        out.append(file_obj)

if len(sys.argv) < 4:
    print('Argomenti richiesti: input_directory [output] [controllo]')
    sys.exit()

input_directory = Path(sys.argv[1])
output_file = Path(sys.argv[2])
controllo = Path(sys.argv[3])

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