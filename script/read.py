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

input_directory = Path(sys.argv[1]).resolve()
output_file = Path(sys.argv[2])
controllo = Path(sys.argv[3] if len(sys.argv) >= 4 else Path(__file__).parent.parent / 'layouts/controllo.bls')

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
            sorted_data.append({'filename': x['filename'], 'layouts': x['layouts'], 'values': [v]})
        
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
        if not all(y in v for y in ('mese_fattura', 'data_fattura', 'codice_pod') for v in ret['values']):
            ret['values'] = []
            if 'warnings' not in out_dict: out_dict['warnings'] = []
            out_dict['warnings'].append('Dati Mancanti') 
        ret['layouts'] = out_dict['layouts']
        if 'warnings' in out_dict:
            ret['warnings'] = out_dict['warnings']
            print(colored('{0} ### {1}'.format(rel_path, ', '.join(out_dict['warnings'])), 'yellow'))
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
    in_files = list(input_directory.rglob('*.pdf'))

    results = []
    files = []

    # Rilegge i vecchi file solo se il layout e' stato ricompilato
    if output_file.exists():
        with open(output_file, 'r') as fin:
            in_data = json.loads(fin.read())

        for pdf_file in in_files:
            skip = False
            for old_obj in filter(lambda x : x['filename'] == str(pdf_file), in_data):
                if 'error' not in old_obj and all(Path(f).stat().st_mtime < output_file.stat().st_mtime for f in old_obj['layouts'] + [pdf_file]):
                    results.append(old_obj)
                    skip = True
            if not skip: files.append(pdf_file)
    else:
        files = in_files

    if files:
        with Pool(min(len(files), nthreads)) as pool:
            results.extend(pool.map(read_pdf, files))

    results = check_conguagli(results)

    with open(output_file, 'w') as fout:
        fout.write(json.dumps(results))