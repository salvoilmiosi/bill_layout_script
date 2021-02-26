import requests
import urllib3
import json
from pathlib import Path
from getpass import getpass

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

getfatture = json.loads(session.post(address + '/zelda/fornitura.ws', verify=False, data={'f':'getFatture'}).text)

result = getfatture['body']['getFatture']['fatture']
fornitori = getfatture['body']['getFatture']['fornitori']

input_directory = Path(__file__).parent.parent / 'work/letture'

letture = []
for f in input_directory.rglob('*.json'):
    with open(f, 'r') as fin:
        letture.extend(json.loads(fin.read()))

def find_filename(numero_fattura, id_fornitore):
    for l in letture:
        if 'values' in l:
            for v in l['values']:
                if 'numero_fattura' in v and v['numero_fattura'][0] == numero_fattura and 'fornitore' in v and v['fornitore'][0] == fornitori[id_fornitore]['nome']:
                    return l['filename']
    return None
    
for id_fattura, fattura in result.items():
    if 'id_file_fattura' in fattura and fattura['id_file_fattura'] is not None and len(fattura['id_file_fattura']) != 0: continue
    if fattura['numero_fattura'] is None or fattura['numero_fattura'] == '': continue

    filename = find_filename(fattura['numero_fattura'], fattura['id_fornitore'])
    if filename is None: continue

    basename = Path(filename).name
    with open(filename, 'rb') as fin:
        result = session.put(address + '/file.ws?f=save', fin.read(), headers={'X-File-Name': basename, 'Content-Type':'application/pdf'})
        save = json.loads(result.text)
        id_file = save['body']['save']['file']['id']

        assoc = json.loads(session.post(address + '/zelda/fornitura.ws', verify=False, data={'f':'associaFileFattura','id_fattura':id_fattura,'id_file':id_file}).text)
        print(filename, assoc['head']['status']['type'])
