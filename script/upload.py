import re
import sys
import requests
import json
from pathlib import Path
from getpass import getpass

path_schede = Path("W:/schede")

session = requests.Session()

login = {'f':'login'}

while True:
    login['login'] = input('Nome utente: ')
    login['password'] = getpass('Password: ')

    loginr = json.loads(session.post('https://portale.bollettaetica.com/login.ws', verify=False, data=login).text)
    print('Login:', loginr['head']['status']['type'])
    if loginr['head']['status']['code'] == 1:
        break

if input('Proseguire? s/N ').lower() != 's': exit(0)

with open(Path(sys.argv[0]).parent.joinpath('forniture.txt')) as file:
    for line in file.readlines():
        match = re.search("^([0-9]+)[ \t]+(.+)$", line)
        id_fornitura = match.group(1)
        scheda = match.group(2)

        files = {'file': open(path_schede.joinpath(scheda), 'rb')}
        values = {'f':'importDatiFatture', 'id_fornitura':id_fornitura}
        uploadr = json.loads(session.post('https://portale.bollettaetica.com/zelda/fornitura.ws', verify=False, data=values, files=files).text)
        print(scheda, uploadr['head']['status']['type'])