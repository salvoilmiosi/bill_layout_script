from pathlib import Path
from datetime import date
import json
import shutil
import os

clienti_json = Path(__file__).parent / 'clienti.json'
dir_letture = Path(__file__).parent.parent / 'work/letture'

with open(clienti_json, 'r') as file:
    clienti = json.load(file)

root_dir = Path(clienti['dir'])
for k,v in clienti['clients'].items():
    with open(dir_letture / (k + ".json"), 'r') as file:
        in_lettura = json.load(file)

    for f in in_lettura:
        filename = root_dir
        if 'dir' in v:
            filename /= v['dir']
        try:
            file_from = f['filename']
            mese_fattura=date.fromisoformat(f['values'][0]['mese_fattura'][0])
            if mese_fattura.year < 2020:
                continue
            pod=f['values'][0]['codice_pod'][0][-3:]
            if 'pods' in v:
                pattern = v['pods'][pod]['filename']
            else:
                pattern = v['filename']
            filename /= pattern.replace('%pod%', pod).replace('%year%', '{:04}'.format(mese_fattura.year)).replace('%month%', '{:02}'.format(mese_fattura.month))
            if not filename.exists():
                print(file_from, filename)
                os.makedirs(os.path.dirname(filename), exist_ok=True)
                shutil.copy(file_from, filename)
        except (KeyError, ValueError):
            pass