import re
import sys
import requests
import json
from pathlib import Path
from getpass import getpass
from conguagli import skip_conguagli
import urllib3

login = {'f':'login'}

address = 'https://portale.bollettaetica.com'

if len(sys.argv) <= 1:
    print('Specificare cartella di input')
    exit(0)

in_path = Path(sys.argv[1])

urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

session = requests.Session()

while True:
    login['login'] = input('Nome utente: ')
    login['password'] = getpass('Password: ')

    loginr = json.loads(session.post(address + '/login.ws', verify=False, data=login).text)
    print('Login:', loginr['head']['status']['type'])
    if loginr['head']['status']['code'] == 1:
        break

fix_conguagli = input('Fix Conguagli? S/n ').lower() in ('s','')

def do_upload(f):
    with open(f, 'r') as file:
        if fix_conguagli:
            data = json.dumps(skip_conguagli(json.loads(file.read())))
        else:
            data = file.read()
        uploadr = json.loads(session.put(address + '/zelda/fornitura.ws?f=importDatiFattureJSON', data).text)
        print(f, uploadr['head']['status']['type'])

if input('Proseguire? S/n ').lower() in ('s',''):
    if in_path.is_dir():
        for f in in_path.rglob('*.json'):
            do_upload(f)
    else:
        do_upload(in_path)