import re
import sys
import requests
import json
from pathlib import Path
from getpass import getpass
from conguagli import skip_conguagli
import urllib3

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

fix_conguagli = input('Fix Conguagli? S/n').lower()

if input('Proseguire? s/N ').lower() != 's': exit(0)

in_dir = Path('W:/letture')
if len(sys.argv) > 1:
    in_dir = Path(sys.argv[1])

in_file = Path(sys.argv[0])

for f in in_dir.rglob('*.json'):
    with open(f, 'r') as file:
        if fix_conguagli == '' or fix_conguagli == 's':
            data = json.dumps(skip_conguagli(json.loads(file.read())))
        else:
            data = file.read()
        uploadr = json.loads(session.put(address + '/zelda/fornitura.ws?f=importDatiFattureJSON', data).text)
        print(f, uploadr['head']['status']['type'])