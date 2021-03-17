from multiprocessing import Pool, cpu_count
from pathlib import Path
from datetime import date, datetime
from argparse import ArgumentParser
import json
import os

import pyreader

os.system('color')

parser = ArgumentParser()
parser.add_argument('input_directory')
parser.add_argument('output_file')
parser.add_argument('-s', '--script', default=Path(__file__).resolve().parent.parent / 'layouts/controllo.bls')
parser.add_argument('-f', '--force-read', action='store_true')
parser.add_argument('-c', '--cached', action='store_true')
parser.add_argument('-r', '--recursive', action='store_true')
parser.add_argument('-y', '--filter-year', type=int, default=0)
parser.add_argument('-j', '--nthreads', type=int, default=cpu_count())
args = parser.parse_args()

input_directory = Path(args.input_directory).resolve()
output_file = Path(args.output_file)

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
        date.fromisoformat(obj['values'][0]['mese_fattura'][0]),
        date.fromisoformat(obj['values'][0]['data_fattura'][0])))

    for i in range(1, len(sorted_data)):
        old_values = sorted_data[i-1]['values'][0]
        cur_values = sorted_data[i]['values'][0]

        old_pod = old_values['codice_pod'][0]
        new_pod = cur_values['codice_pod'][0]

        old_mesefatt = date.fromisoformat(old_values['mese_fattura'][0])
        new_mesefatt = date.fromisoformat(cur_values['mese_fattura'][0])

        old_datafatt = date.fromisoformat(old_values['data_fattura'][0])
        new_datafatt = date.fromisoformat(cur_values['data_fattura'][0])

        if old_pod == new_pod and old_mesefatt == new_mesefatt and new_datafatt > old_datafatt:
            cur_values['conguaglio'] = True

    return sorted_data + error_data

required_data = ('fornitore', 'numero_fattura', 'mese_fattura', 'data_fattura', 'codice_pod')

def read_pdf(pdf_file):
    rel_path = pdf_file.relative_to(input_directory)
    ret = {'filename':str(pdf_file)}

    try:
        out_dict = pyreader.readpdf(pdf_file, args.script, cached=args.cached, recursive=args.recursive)

        ret['values'] = [v for v in out_dict['values'] if all(i in v for i in required_data)]
        if ret['values'] != out_dict['values']:
            if 'warnings' not in out_dict: out_dict['warnings'] = []
            out_dict['warnings'].append('Dati Mancanti') 
        ret['layouts'] = [str(Path(x).resolve()) for x in out_dict['layouts']]
        if 'warnings' in out_dict:
            ret['warnings'] = out_dict['warnings']
            print('\033[33m{0} ### {1}\033[0m'.format(rel_path, ', '.join(out_dict['warnings'])))
        else:
            print(rel_path)
    except pyreader.Error as err:
        ret['error'] = str(err)
        print('\033[31m{0} ### {1}\033[0m'.format(rel_path, ret['error']))
    except pyreader.Timeout as err:
        ret['error'] = str(err)
        print('\033[35m{0} ### {1}\033[0m'.format(rel_path, ret['error']))

    return ret

def lastmodified(f):
    return datetime.fromtimestamp(Path(f).stat().st_mtime)

if __name__ == '__main__':
    in_files = [f for f in input_directory.rglob('*.pdf') if lastmodified(f).year >= args.filter_year]

    results = []
    files = []

    # Rilegge i vecchi file solo se il layout e' stato ricompilato
    if not args.force_read and output_file.exists():
        with open(output_file, 'r') as file:
            in_data = json.load(file)

        for pdf_file in in_files:
            skip = False
            for old_obj in filter(lambda x : x['filename'] == str(pdf_file), in_data):
                if 'error' not in old_obj and all(lastmodified(f) < lastmodified(output_file) for f in old_obj['layouts'] + [pdf_file]):
                    results.append(old_obj)
                    skip = True
            if not skip: files.append(pdf_file)
    else:
        files = in_files

    if files:
        with Pool(min(len(files), args.nthreads)) as pool:
            results.extend(pool.map(read_pdf, files))

    results = check_conguagli(results)

    with open(output_file, 'w') as file:
        json.dump(results, file, indent=4)