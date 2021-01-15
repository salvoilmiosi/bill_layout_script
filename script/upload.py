import re
import sys
import requests
import json
from pathlib import Path
from getpass import getpass
from termcolor import colored
import urllib3

login = {'f':'login'}

address = 'https://portale.bollettaetica.com'

in_path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(sys.argv[0]).parent.joinpath('../work/letture')

urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)

session = requests.Session()

while True:
    login['login'] = input('Nome utente: ')
    login['password'] = getpass('Password: ')

    loginr = json.loads(session.post(address + '/login.ws', verify=False, data=login).text)
    print('Login:', loginr['head']['status']['type'])
    if loginr['head']['status']['code'] == 1:
        break

delete_conguagli = input('Salta conguagli? S/n').lower() in ('s','')

def do_upload(f):
    with open(f, 'r') as file:
        data = json.loads(file.read())
        if delete_conguagli:
            for x in data:
                if 'conguaglio' in x:
                    x['values'] = []
        response = session.put(address + '/zelda/fornitura.ws?f=importDatiFattureJSON', json.dumps(data))
        uploadr = json.loads(response.text)
        if (uploadr['head']['status']['code'] == 0):
            print(f)
        else:
            print(f, colored(uploadr['head']['status']['message'],'red'))

if input('Proseguire? S/n ').lower() in ('s',''):
    if in_path.is_dir():
        for f in in_path.rglob('*.json'):
            do_upload(f)
    else:
        do_upload(in_path)