import requests
import urllib3
import json
from pathlib import Path
from getpass import getpass
from datetime import date, datetime
from termcolor import colored

urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)
session = requests.Session()

login = {'f':'login'}

address = 'https://portale.bollettaetica.com'

while True:
    login['login'] = input('Nome utente: ')
    login['password'] = getpass('Password: ')

    loginr = json.loads(session.post(address + '/login.ws', verify=False, data=login).text)
    print('Login:', loginr['head']['status']['type'])
    if loginr['head']['status']['code'] == 1:
        break

getFattureRes = session.post(address + '/zelda/fornitura.ws', verify=False, data={'f':'getFatture'})
getFatture = json.loads(getFattureRes.text)['body']['getFatture']

input_directory = Path(__file__).parent.parent / 'work/letture'

letture = []
for f in input_directory.rglob('*.json'):
    with open(f, 'r') as fin:
        letture.extend(json.loads(fin.read()))

def getvalue(obj, key):
    return obj[key][0] if key in obj else None

def find_filename(fattura):
    fornitura = getFatture['forniture'][fattura['id_fornitura']]
    gruppo_forniture = getFatture['gruppi_forniture'][fornitura['id_gruppo_forniture']]

    ragione_sociale = getFatture['clienti'][gruppo_forniture['id_cliente']]['ragione_sociale']
    numero_fattura = fattura['numero_fattura']
    nome_fornitore = getFatture['fornitori'][fattura['id_fornitore']]['nome']

    codice_pod = fornitura['pod']
    mese_fattura = date(int(fattura['anno'][0]), int(fattura['mese'][0]), 1)

    print(ragione_sociale, codice_pod, mese_fattura, nome_fornitore, numero_fattura)

    for l in letture:
        if 'values' not in l: continue
        for v in l['values']:
            if getvalue(v, 'fornitore') != nome_fornitore: continue
            if getvalue(v, 'numero_fattura') == numero_fattura \
                or (getvalue(v, 'codice_pod') == codice_pod and datetime.strptime(getvalue(v, 'mese_fattura'), '%Y-%m').date() == mese_fattura):
                return l['filename']
    return None

for id_fattura, fattura in getFatture['fatture'].items():
    if 'id_file_fattura' in fattura and fattura['id_file_fattura'] is not None and len(fattura['id_file_fattura']) != 0: continue

    filename = find_filename(fattura)
    if filename is None: continue

    with open(filename, 'rb') as fin:
        saveres = json.loads(session.put(address + '/file.ws?f=save', fin.read(), headers={'X-File-Name': Path(filename).name, 'Content-Type':'application/pdf'}).text)
        id_file = saveres['body']['save']['file']['id']

        json.loads(session.post(address + '/zelda/fornitura.ws', verify=False, data={'f':'associaFileFattura','id_fattura':id_fattura,'id_file':id_file}).text)
        print(colored(filename, 'green'))