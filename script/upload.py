import requests
import urllib3
import json
import sys
from pathlib import Path
from getpass import getpass
from datetime import date, datetime
from termcolor import colored

urllib3.disable_warnings(urllib3.exceptions.InsecureRequestWarning)
session = requests.Session()

address = 'https://portale.bollettaetica.com'
in_path = Path(sys.argv[1]) if len(sys.argv) > 1 else Path(__file__).parent.parent / 'work/letture'

def filter_and_upload(f, do_upload = True):
    with open(f, 'r') as file:
        data = json.load(file)
    for x in data:
        if 'conguaglio' in x or (filter_year != 0 and any(datetime.strptime(t['mese_fattura'][0],'%Y-%m').year < filter_year for t in x['values'])):
            x['values'] = []
    if do_upload:
        response = session.put(address + '/zelda/fornitura.ws?f=importDatiFattureJSON', json.dumps(data))
        uploadr = json.loads(response.text)
        if (uploadr['head']['status']['code'] == 0):
            print(f)
        else:
            print(f, colored(uploadr['head']['status']['message'],'red'))

try:
    filter_year = int(input("Filtrare fatture prima dell'anno: "))
except ValueError:
    filter_year = 0

do_upload = input('Proseguire? S/n ').lower() in ('s','')

if do_upload:
    while True:
        login = {'f':'login'}
        login['login'] = input('Nome utente: ')
        login['password'] = getpass('Password: ')

        loginr = json.loads(session.post(address + '/login.ws', verify=False, data=login).text)
        print('Login:', loginr['head']['status']['type'])
        if loginr['head']['status']['code'] == 1:
            break

if in_path.is_dir():
    for f in in_path.rglob('*.json'):
        filter_and_upload(f, do_upload)
else:
    filter_and_upload(in_path, do_upload)