from multiprocessing import Pool, cpu_count
from pathlib import Path
from termcolor import colored
import sys
import os
import json
import datetime
import time

app_dir = Path(sys.argv[0]).parent
bin_dir = str(app_dir / '../bin')
sys.path.append(bin_dir)
os.environ['PATH'] = bin_dir
import pyreader

if len(sys.argv) < 3:
    print('Argomenti richiesti: input_directory [output] [controllo] [nthreads]')
    sys.exit()

input_directory = Path(sys.argv[1])
output_file = Path(sys.argv[2])
controllo = Path(sys.argv[3] if len(sys.argv) >= 4 else app_dir / '../work/layouts/controllo.out')

try:
    nthreads = int(sys.argv[4]) if len(sys.argv) >= 5 else cpu_count()
except NotImplementedError:
    nthreads = 1

def check_conguagli(results):
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

    return sorted_data + error_data

def read_pdf(pdf_file):
    rel_path = pdf_file.relative_to(input_directory)
    ret = {'filename':str(rel_path)}

    try:
        out_dict = pyreader.readpdf_autolayout(str(pdf_file), str(controllo))

        ret['values'] = out_dict['values']
        if 'layout' in out_dict['globals']:
            ret['layout'] = out_dict['globals']['layout'][0]
        if 'warnings' in out_dict:
            ret['warnings'] = out_dict['warnings']
            print(colored('{0} ### {1}'.format(rel_path, ', '.join(out_dict['warnings'])), 'yellow'))
        elif 'layout' in ret:
            print('{0} ### {1}'.format(rel_path, ret['layout']))
        else:
            print(rel_path)
    except pyreader.error as err:
        ret['error'] = str(err)
        print(colored('{0} ### {1}'.format(rel_path, ret['error']), 'red'))
    except:
        ret['error'] = 'Errore sconosciuto'
        print(colored('{0} ### {1}'.format(rel_path, ret['error']), 'magenta'))

    return ret

if __name__ == '__main__':
    if not input_directory.exists():
        print(f'La directory {input_directory} non esiste')
        sys.exit(1)

    if not controllo.exists():
        print(f'Il file di layout {controllo} non esiste')
        sys.exit(1)
        
    files = input_directory.rglob('*.pdf')

    results = []

    # Rilegge i vecchi file solo se il layout e' stato ricompilato
    if output_file.exists():
        with open(output_file, 'r') as fin:
            in_data = json.loads(fin.read())

        in_files = []
        for f in files:
            ignore = False
            rel_path = f.relative_to(input_directory)
            for old_obj in filter(lambda x: x['filename'] == str(rel_path), in_data):
                if 'layout' in old_obj and 'values' in old_obj:
                    layout_file = controllo.parent / '{0}.out'.format(old_obj['layout'])
                    if os.path.getmtime(str(layout_file)) < os.path.getmtime(str(output_file)):
                        results.append(old_obj)
                        ignore = True
            if not ignore:
                in_files.append(f)
    else:
        in_files = list(files)

    if in_files:
        with Pool(min(len(in_files), nthreads)) as pool:
            results.extend(pool.map(read_pdf, in_files))

    results = check_conguagli(results)

    with open(output_file, 'w') as fout:
        fout.write(json.dumps(results))