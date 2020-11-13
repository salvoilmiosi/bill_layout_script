import subprocess
import sys
import json
import datetime
import time
import os
from pathlib import Path
from openpyxl import Workbook
from openpyxl.styles import PatternFill, Border, Side

table_values = [
    {'title':'File','value':'filename'},
    {'title':'Data Download','value':'lastmodified','type':'date'},
    {'title':'POD','value':'codice_pod'},
    {'title':'Mese','value':'mese_fattura','type':'month'},
    {'title':'Fornitore','value':'fornitore'},
    {'title':'N. Fatt.','value':'numero_fattura'},
    {'title':'Data Emissione','value':'data_fattura','type':'date'},
    {'title':'Data scadenza','value':'data_scadenza','type':'date'},
    {'title':'Costo Materia Energia','value':'spesa_materia_energia','type':'number'},
    {'title':'Trasporto','value':'trasporto_gestione','type':'number'},
    {'title':'Oneri','value':'oneri','type':'number'},
    {'title':'Accise','value':'accise','type':'number'},
    {'title':'Iva','value':'iva','type':'percentage'},
    {'title':'Imponibile','value':'imponibile','type':'number'},
    {'title':'F1','value':'energia_attiva','index':0,'type':'number'},
    {'title':'F2','value':'energia_attiva','index':1,'type':'number'},
    {'title':'F3','value':'energia_attiva','index':2,'type':'number'},
    {'title':'P1','value':'potenza','index':0,'type':'number'},
    {'title':'P2','value':'potenza','index':1,'type':'number'},
    {'title':'P3','value':'potenza','index':2,'type':'number'},
    {'title':'R1','value':'energia_reattiva','index':0,'type':'number'},
    {'title':'R2','value':'energia_reattiva','index':1,'type':'number'},
    {'title':'R3','value':'energia_reattiva','index':2,'type':'number'},
    {'title':'CTS','value':'cts','type':'number'},
    {'title':'>75%','value':'penale_reattiva_sup75','type':'number'},
    {'title':'<75%','value':'penale_reattiva_inf75','type':'number'},
    {'title':'PEF1','value':'prezzo_energia','index':0,'type':'number'},
    {'title':'PEF2','value':'prezzo_energia','index':1,'type':'number'},
    {'title':'PEF3','value':'prezzo_energia','index':2,'type':'number'},
]

out = []
out_err = []

def read_file(pdf_file):
    rel_path = pdf_file.relative_to(input_directory)
    print(rel_path)

    args = [app_dir.joinpath('../bin/layout_reader'), '-p', pdf_file, '-s', file_layout]
    proc = subprocess.run(args, capture_output=True, text=True)

    try:
        json_output = json.loads(proc.stdout)
        json_values = json_output['values']
    except:
        out_err.append([str(rel_path), proc.stderr])
    else:
        for json_page in json_values:
            row = []
            for obj in table_values:
                if obj['value'] == 'filename':
                    row.append({'value':str(rel_path),'type':'str'})
                elif obj['value'] == 'lastmodified':
                    row.append({'value':datetime.date.fromtimestamp(os.stat(str(path)).st_mtime),'type':'date'})
                else:
                    try:
                        row.append({'value':json_page[obj['value']][obj['index'] if 'index' in obj else 0], 'type':obj['type'] if 'type' in obj else 'str'})
                    except (KeyError, IndexError, ValueError):
                        row.append({'value':'','type':''})

            out.append(row)

if len(sys.argv) < 2:
    print('Argomenti richiesti: input_directory [script_controllo.out] [output.xlsx]')
    sys.exit()

app_dir = Path(sys.argv[0]).parent

input_directory = Path(sys.argv[1])
file_layout = Path(sys.argv[2]) if len(sys.argv) >= 3 else app_dir.joinpath('../layout/controllo.out')
output_file = Path(sys.argv[3]) if len(sys.argv) >= 4 else app_dir.joinpath('out/{0}.xlsx'.format(input_directory.name))

if not input_directory.exists():
    print('La directory {0} non esiste'.format(input_directory))
    sys.exit(1)

if not file_layout.exists():
    print('Il file di layout {0} non esiste'.format(file_layout))
    sys.exit(1)

for path in input_directory.rglob('*.pdf'):
    read_file(path)

wb = Workbook()
ws = wb.active

ws.append([obj['title'] for obj in table_values])

indices = {x['title']:i for i,x in enumerate(table_values)}

out.sort(key = lambda obj: (obj[indices['POD']]['value'], obj[indices['Mese']]['value']))

old_pod = ''
old_date = ''
for i, row in enumerate(out, 2):
    new_pod = row[indices['POD']]['value']
    new_date = row[indices['Mese']]['value']
    for j, c in enumerate(row, 1):
        cell = ws.cell(row=i, column=j)
        if new_pod != old_pod:
            cell.border = Border(top=Side(border_style='thin', color='000000'))
        try:
            if c['type'] == 'str':
                cell.value = c['value']
            elif c['type'] == 'date':
                cell.number_format = 'DD/MM/YY'
                if isinstance(c['value'],datetime.date):
                    cell.value = c['value']
                else:
                    cell.value = datetime.datetime.strptime(c['value'], "%Y-%m-%d")
            elif c['type'] == 'month':
                cell.number_format = 'MM/YYYY'
                cell.value = datetime.datetime.strptime(c['value'], "%Y-%m")
            elif c['type'] == 'number':
                cell.value = float(c['value'])
            elif c['type'] == 'percentage':
                cell.number_format = '0%'
                cell.value = float(c['value'][:-1]) / 100
        except (KeyError, IndexError, ValueError):
            pass
    if new_date == '' or new_date == old_date:
        ws.cell(row=i, column=indices['Mese'] + 1).fill = PatternFill(patternType='solid', fgColor='ffff00')
    old_pod = new_pod
    old_date = new_date

for row in out_err:
    ws.append(row)

ws.freeze_panes = ws['A2']

wb.save(output_file)
