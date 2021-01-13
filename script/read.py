import subprocess
import sys
import json
import datetime
import time
import os
from multiprocessing.pool import ThreadPool
from multiprocessing import cpu_count
from pathlib import Path
from termcolor import colored

out = []
old_out = []

app_dir = Path(sys.argv[0]).parent
layout_reader = app_dir.joinpath('../build/reader')

def read_pdf(pdf_file):
    rel_path = pdf_file.relative_to(input_directory)

    ignore = False

    # Rilegge i vecchi file solo se il layout e' stato ricompilato
    for old_obj in filter(lambda x: x['filename'] == str(rel_path), old_out):
        if not 'layout' in old_obj:
            args = [layout_reader, '-p', pdf_file, controllo]
            proc = subprocess.run(args, capture_output=True, text=True)
            json_out = json.loads(proc.stdout)
            if not 'error' in json_out and 'layout' in json_out['globals']:
                old_obj['layout'] = json_out['globals']['layout'][0]
        if 'layout' in old_obj and 'values' in old_obj:
            layout_file = controllo.parent.joinpath('{0}.out'.format(old_obj['layout']))
            if os.path.getmtime(str(layout_file)) < os.path.getmtime(str(output_file)):
                out.append(old_obj)
                ignore = True

    if ignore: return
    
    args = [layout_reader, '-p', pdf_file, '-s', controllo]
    proc = subprocess.run(args, capture_output=True, text=True)

    file_obj = {'filename':str(rel_path)}
    try:
        json_out = json.loads(proc.stdout)
        if 'error' in json_out:
            error_message = json_out['error']
            file_obj['error'] = error_message
            print(colored(f'{rel_path} ### {error_message}','red'))
        elif 'layout' in json_out['globals']:
            file_obj['layout'] = json_out['globals']['layout'][0]
            file_obj['values'] = json_out['values']
            print(rel_path)
    except:
        error_message = f'Return code {proc.returncode}'
        file_obj['error'] = error_message
        print(colored(f'{rel_path} ### {error_message}','red'))

    out.append(file_obj)

if len(sys.argv) < 3:
    print('Argomenti richiesti: input_directory [output] [controllo] [nthreads]')
    sys.exit()

input_directory = Path(sys.argv[1])
output_file = Path(sys.argv[2])
controllo = Path(sys.argv[3] if len(sys.argv) >= 4 else app_dir.joinpath('../work/layouts/controllo.out'))

try:
    nthreads = int(sys.argv[4]) if len(sys.argv) >= 5 else cpu_count()
except NotImplementedError:
    nthreads = 1

if output_file.exists():
    with open(output_file, 'r') as fin:
        old_out = json.loads(fin.read())

if not input_directory.exists():
    print(f'La directory {input_directory} non esiste')
    sys.exit(1)

if not controllo.exists():
    print(f'Il file di layout {controllo} non esiste')
    sys.exit(1)

files = list(input_directory.rglob('*.pdf'))

with ThreadPool(min(len(files), nthreads)) as pool:
    pool.map(read_pdf, files)

with open(output_file, 'w') as fout:
    fout.write(json.dumps(out))