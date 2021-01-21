from multiprocessing import Pool, cpu_count
from termcolor import colored
from pathlib import Path
from datetime import datetime
import sys
import json
import pyreader

if len(sys.argv) < 3:
    print('Argomenti richiesti: input_directory [output] [controllo] [nthreads]')
    sys.exit()

app_dir = Path(sys.argv[0]).parent
input_directory = Path(sys.argv[1]).resolve()
output_file = Path(sys.argv[2])
controllo = Path(sys.argv[3] if len(sys.argv) >= 4 else app_dir / '../layouts/controllo.bls')

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
        datetime.strptime(obj['values'][0]['mese_fattura'][0], '%Y-%m').date(),
        datetime.strptime(obj['values'][0]['data_fattura'][0], '%Y-%m-%d').date()))

    old_pod = None
    old_mesefatt = None
    old_datafatt = None
    for x in sorted_data:
        new_pod = x['values'][0]['codice_pod'][0]
        new_mesefatt = datetime.strptime(x['values'][0]['mese_fattura'][0], '%Y-%m').date()
        new_datafatt = datetime.strptime(x['values'][0]['data_fattura'][0], '%Y-%m-%d').date()

        if old_pod == new_pod and old_mesefatt == new_mesefatt and new_datafatt > old_datafatt:
            x['conguaglio'] = True

        old_pod = new_pod
        old_mesefatt = new_mesefatt
        old_datafatt = new_datafatt

    return sorted_data + error_data

def read_pdf(pdf_file):
    rel_path = pdf_file.relative_to(input_directory)
    ret = {'filename':str(pdf_file)}

    try:
        out_dict = pyreader.readpdf(pdf_file, controllo)

        ret['values'] = out_dict['values']
        if 'layout' in out_dict:
            ret['layout'] = out_dict['layout']
        if 'warnings' in out_dict:
            ret['warnings'] = out_dict['warnings']
            print(colored('{0} ### {1}'.format(rel_path, ', '.join(out_dict['warnings'])), 'yellow'))
        elif 'layout' in ret:
            print('{0} ### {1}'.format(rel_path, ret['layout']))
        else:
            print(rel_path)
    except pyreader.Error as err:
        ret['error'] = str(err)
        print(colored('{0} ### {1}'.format(rel_path, ret['error']), 'red'))
    except pyreader.Timeout as err:
        ret['error'] = str(err)
        print(colored('{0} ### {1}'.format(rel_path, ret['error']), 'magenta'))

    return ret

if __name__ == '__main__':
    if not input_directory.exists():
        print(f'La directory {input_directory} non esiste')
        sys.exit(1)

    if not controllo.exists():
        print(f'Il file di layout {controllo} non esiste')
        sys.exit(1)
        
    in_files = list(input_directory.rglob('*.pdf'))

    results = []
    files = []

    # Rilegge i vecchi file solo se il layout e' stato ricompilato
    if output_file.exists():
        with open(output_file, 'r') as fin:
            in_data = json.loads(fin.read())

        for f in in_files:
            skip = False
            for old_obj in filter(lambda x : x['filename'] == str(f), in_data):
                if 'layout' in old_obj:
                    layout_file = controllo.parent / '{0}.bls'.format(old_obj['layout'])
                    if layout_file.stat().st_mtime < output_file.stat().st_mtime and f.stat().st_mtime < output_file.stat().st_mtime:
                        results.append(old_obj)
                        skip = True
            if not skip: files.append(f)
    else:
        files = in_files

    if files:
        with Pool(min(len(files), nthreads)) as pool:
            results.extend(pool.map(read_pdf, files))

    results = check_conguagli(results)

    with open(output_file, 'w') as fout:
        fout.write(json.dumps(results))