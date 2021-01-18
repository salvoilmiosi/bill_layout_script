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

in_data = []
results = []

app_dir = Path(sys.argv[0]).parent
layout_reader = app_dir.joinpath('../work/bin/reader')

def check_conguagli():
    global results
    sorted_data = []
    error_data = []
    for x in results:
        if 'error' in x:
            error_data.append({'filename': x['filename'], 'error': x['error'], 'values': []})
            continue
        for v in x['values']:
            if all(y in v for y in ('mese_fattura', 'data_fattura', 'codice_pod')):
                sorted_data.append({'filename': x['filename'], 'layout': x['layout'], 'values': [v]})
            else:
                error_data.append({'filename': x['filename'], 'error': 'Dati mancanti', 'values': []})
        
    sorted_data.sort(key = lambda obj : (
        obj['values'][0]['codice_pod'][0],
        datetime.datetime.strptime(obj['values'][0]['mese_fattura'][0], '%Y-%m').date(),
        datetime.datetime.strptime(obj['values'][0]['data_fattura'][0], '%Y-%m-%d').date()))

    old_pod = None
    old_mesefatt = None
    old_datafatt = None
    for x in sorted_data:
        new_pod = x['values'][0]['codice_pod'][0]
        new_mesefatt = datetime.datetime.strptime(x['values'][0]['mese_fattura'][0], '%Y-%m').date()
        new_datafatt = datetime.datetime.strptime(x['values'][0]['data_fattura'][0], '%Y-%m-%d').date()

        if old_pod == new_pod and old_mesefatt == new_mesefatt and new_datafatt > old_datafatt:
            x['conguaglio'] = True

        old_pod = new_pod
        old_mesefatt = new_mesefatt
        old_datafatt = new_datafatt

    results = sorted_data + error_data

def read_pdf(pdf_file):
    rel_path = pdf_file.relative_to(input_directory)

    # Rilegge i vecchi file solo se il layout e' stato ricompilato
    ignore = False
    
    for old_obj in filter(lambda x: x['filename'] == str(rel_path), in_data):
        if not 'layout' in old_obj:
            args = [layout_reader, '-p', pdf_file, controllo]
            proc = subprocess.run(args, capture_output=True, text=True)
            json_out = json.loads(proc.stdout)
            if not 'error' in json_out and 'layout' in json_out['globals']:
                old_obj['layout'] = json_out['globals']['layout'][0]
        if 'layout' in old_obj and 'values' in old_obj:
            layout_file = controllo.parent.joinpath('{0}.out'.format(old_obj['layout']))
            if os.path.getmtime(str(layout_file)) < os.path.getmtime(str(output_file)):
                results.append(old_obj)
                ignore = True

    if ignore: return
    
    args = [layout_reader, '-p', pdf_file, '-s', controllo]
    proc = subprocess.run(args, capture_output=True, text=True)

    file_obj = {'filename':str(rel_path)}
    try:
        json_out = json.loads(proc.stdout)
        if 'error' in json_out:
            file_obj['error'] = json_out['error']
            print(colored('{0} ### {1}'.format(rel_path, json_out['error']),'red'))
        else:
            file_obj['values'] = json_out['values']
            if 'layout' in json_out['globals']:
                file_obj['layout'] = json_out['globals']['layout'][0]
            if 'warnings' in json_out:
                file_obj['warnings'] = json_out['warnings']
                print(colored('{0} ### {1}'.format(rel_path, ', '.join(json_out['warnings'])), 'yellow'))
            elif 'layout' in file_obj:
                print('{0} ### {1}'.format(rel_path, file_obj['layout']))
            else:
                print(rel_path)
    except:
        file_obj['error'] = f'Return code {proc.returncode}'
        print(colored('{0} ### {1}'.format(rel_path, file_obj['error']), 'blue'))

    results.append(file_obj)

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
        in_data = json.loads(fin.read())

if not input_directory.exists():
    print(f'La directory {input_directory} non esiste')
    sys.exit(1)

if not controllo.exists():
    print(f'Il file di layout {controllo} non esiste')
    sys.exit(1)

files = list(input_directory.rglob('*.pdf'))

with ThreadPool(min(len(files), nthreads)) as pool:
    pool.map(read_pdf, files)

check_conguagli()

with open(output_file, 'w') as fout:
    fout.write(json.dumps(results))