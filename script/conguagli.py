from pathlib import Path
import datetime
import sys
import json

def skip_conguagli(json_data, delete_conguagli = False):
    sorted_data = []
    error_data = []
    for x in json_data:
        if 'error' in x:
            error_data.append({'filename': x['filename'], 'error': x['error'], 'values': []})
            continue
        for v in x['values']:
            if all(y in v for y in ('mese_fattura', 'data_fattura', 'codice_pod')):
                sorted_data.append({'filename': x['filename'], 'layout': x['layout'], 'values': [v]})
        
    sorted_data.sort(key = lambda obj : (
        obj['values'][0]['codice_pod'][0],
        datetime.datetime.strptime(obj['values'][0]['mese_fattura'][0], '%Y-%m').date(),
        datetime.datetime.strptime(obj['values'][0]['data_fattura'][0], '%Y-%m-%d').date()))
    new_data = []

    old_pod = None
    old_mesefatt = None
    old_datafatt = None
    for x in sorted_data:
        new_pod = x['values'][0]['codice_pod'][0]
        new_mesefatt = datetime.datetime.strptime(x['values'][0]['mese_fattura'][0], '%Y-%m').date()
        new_datafatt = datetime.datetime.strptime(x['values'][0]['data_fattura'][0], '%Y-%m-%d').date()

        if old_pod == new_pod and old_mesefatt == new_mesefatt and new_datafatt > old_datafatt:
            x['conguaglio'] = True
        
        if not (delete_conguagli and 'conguaglio' in x):
            new_data.append(x)

        old_pod = new_pod
        old_mesefatt = new_mesefatt
        old_datafatt = new_datafatt

    new_data.extend(error_data)

    return new_data

if __name__ == '__main__':
    if len(sys.argv) <= 1:
        print('Specificare file di input')
        exit(0)
    
    in_file = Path(sys.argv[1])
    
    with open(in_file, 'r') as file:
        data = skip_conguagli(json.loads(file.read()))
    
    if len(sys.argv) <= 2:
        out_file = in_file.with_name(in_file.stem + '-nocong').with_suffix(in_file.suffix)
    else:
        out_file = sys.argv[2]
    
    with open(out_file, 'w') as file:
        file.write(json.dumps(data))