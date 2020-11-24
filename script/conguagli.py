from pathlib import Path
import datetime
import sys
import json

def skip_conguagli(json_data):
    sorted_data = []
    for x in json_data:
        if 'error' in x:
            continue
        for page in x['values']:
            if all(y in page for y in ('mese_fattura', 'data_fattura', 'codice_pod')):
                sorted_data.append({'filename': x['filename'], 'lastmodified': x['lastmodified'], 'layout': x['layout'], 'values': [page]})
        
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
            print(x['filename'], new_pod, new_mesefatt, new_datafatt, '### Conguaglio')
        else:
            print(x['filename'], new_pod, new_mesefatt, new_datafatt)
            new_data.append(x)

        old_pod = new_pod
        old_mesefatt = new_mesefatt
        old_datafatt = new_datafatt

    return new_data