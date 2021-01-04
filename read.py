import subprocess
import sys
import json
import datetime
import time
import os
import threading
import queue
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
                    x['layout'] = json_out['globals']['layout'][0]
            if 'layout' in x and 'values' in x:
                layout_file = controllo.parent.joinpath('../bls/{0}.bls'.format(x['layout']))
                if os.path.getmtime(str(layout_file)) < os.path.getmtime(str(output_file)):
                    out.append(x)
                    skipped = True

    if not skipped:
        args = [layout_reader, '-p', pdf_file, '-s', controllo]
        proc = subprocess.run(args, capture_output=True, text=True)

        file_obj = {'filename':str(rel_path), 'lastmodified':os.stat(str(path)).st_mtime}
        try:
            json_out = json.loads(proc.stdout)
            if json_out['error']:
                file_obj['error'] = json_out['message']
                print(f'### Errore {rel_path}')
            elif 'layout' in json_out['globals']:
                file_obj['layout'] = json_out['globals']['layout'][0]
                file_obj['values'] = json_out['values']
                print(rel_path)
        except:
            file_obj['error'] = f'errore {proc.returncode}'
            print(f'### Errore {rel_path}')

        out.append(file_obj)

if len(sys.argv) < 3:
    print('Argomenti richiesti: input_directory [output] [controllo]')
    sys.exit()

input_directory = Path(sys.argv[1])
output_file = Path(sys.argv[2])
controllo = Path(sys.argv[3] if len(sys.argv) >= 4 else 'layout/controllo.out')
nthreads = int(sys.argv[4]) if len(sys.argv) >= 5 else 5

if output_file.exists():
    with open(output_file, 'r') as fin:
        old_out = json.loads(fin.read())

if not input_directory.exists():
    print(f'La directory {input_directory} non esiste')
    sys.exit(1)

if not controllo.exists():
    print(f'Il file di layout {controllo} non esiste')
    sys.exit(1)

q = queue.Queue()
for path in input_directory.rglob('*.pdf'):
    q.put_nowait(path)

def worker():
    while True:
        try:
            file = q.get(timeout=1)
            read_pdf(file)
            q.task_done()
        except queue.Empty:
            return

for _ in range(nthreads):
    threading.Thread(target=worker).start()

q.join()

with open(output_file, 'w') as fout:
    fout.write(json.dumps(out))