import sys
import json
import datetime
import time
import os
from pathlib import Path
from openpyxl import Workbook
from openpyxl.utils import get_column_letter
from openpyxl.styles import PatternFill, Border, Side

class TableValue:
    def __init__(self, title, value, index=0, type='str', number_format=None, column_width=None):
        self.title = title
        self.value = value
        self.index = index
        self.type = type
        self.number_format = number_format
        self.column_width = column_width

table_values = [
    TableValue('File',                  'filename'),
    TableValue('Ultima Modifica',       'lastmodified',              type='date'),
    TableValue('POD',                   'codice_pod', column_width=16),
    TableValue('Mese',                  'mese_fattura',              type='month'),
    TableValue('Fornitore',             'fornitore'),
    TableValue('N. Fatt.',              'numero_fattura'),
    TableValue('Data Emissione',        'data_fattura',              type='date'),
    TableValue('Data scadenza',         'data_scadenza',             type='date'),
    TableValue('Costo Materia Energia', 'spesa_materia_energia',     type='euro', column_width=11),
    TableValue('Trasporto',             'trasporto_gestione',        type='euro', column_width=11),
    TableValue('Oneri',                 'oneri',                     type='euro', column_width=11),
    TableValue('Accise',                'accise',                    type='euro'),
    TableValue('Iva',                   'iva',                       type='percentage', column_width=6),
    TableValue('Imponibile',            'imponibile',                type='euro', column_width=11),
    TableValue('F1',                    'energia_attiva',   index=0, type='int', column_width=8),
    TableValue('F2',                    'energia_attiva',   index=1, type='int', column_width=8),
    TableValue('F3',                    'energia_attiva',   index=2, type='int', column_width=8),
    TableValue('P1',                    'potenza',          index=0, type='int', column_width=8),
    TableValue('P2',                    'potenza',          index=1, type='int', column_width=8),
    TableValue('P3',                    'potenza',          index=2, type='int', column_width=8),
    TableValue('R1',                    'energia_reattiva', index=0, type='int', column_width=8),
    TableValue('R2',                    'energia_reattiva', index=1, type='int', column_width=8),
    TableValue('R3',                    'energia_reattiva', index=2, type='int', column_width=8),
    TableValue('CTS',                   'cts',                       type='euro'),
    TableValue('>75%',                  'penale_reattiva_sup75',     type='euro', column_width=11),
    TableValue('<75%',                  'penale_reattiva_inf75',     type='euro', column_width=11),
    TableValue('PEF1',                  'prezzo_energia',   index=0, type='number', number_format='0.00000000', column_width=11),
    TableValue('PEF2',                  'prezzo_energia',   index=1, type='number', number_format='0.00000000', column_width=11),
    TableValue('PEF3',                  'prezzo_energia',   index=2, type='number', number_format='0.00000000', column_width=11),
]

out = []
out_err = []

def add_rows(json_data):
    filename = json_data['filename']
    lastmodified = json_data['lastmodified']

    print(filename)

    if 'error' in json_data:
        out_err.append([filename, json_data['error']])
        return
    for json_page in json_data['values']:
        row = []

        for obj in table_values:
            if obj.value == 'filename':
                row.append({'value':filename, 'type':'str'})
            elif obj.value == 'lastmodified':
                row.append({'value':datetime.date.fromtimestamp(lastmodified), 'type':'date'})
            else:
                try:
                    row.append({'value':json_page[obj.value][obj.index], 'type':obj.type, 'number_format':obj.number_format})
                except (KeyError, IndexError, ValueError):
                    row.append({'value':'', 'type':''})

        out.append(row)

if len(sys.argv) < 2:
    print('Argomenti richiesti: input_file [output.xlsx]')
    sys.exit()

app_dir = Path(sys.argv[0]).parent

input_file = Path(sys.argv[1])
output_file = Path(sys.argv[2]) if len(sys.argv) >= 3 else input_file.with_suffix('.xlsx')

if not input_file.exists():
    print('Il file {0} non esiste'.format(input_file))
    sys.exit(1)

with open(input_file, 'r') as fin:
    for r in json.loads(fin.read()):
        add_rows(r)

wb = Workbook()
ws = wb.active

ws.append([obj.title for obj in table_values])
for i, obj in enumerate(table_values, 1):
    if obj.column_width != None:
        ws.column_dimensions[get_column_letter(i)].width = obj.column_width

indices = {x.title:i for i,x in enumerate(table_values)}

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
                cell.number_format = '@'
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
            elif c['type'] == 'euro':
                cell.number_format = '#,##0.00 €'
                cell.value = float(c['value'])
            elif c['type'] == 'int':
                cell.number_format = '0'
                cell.value = float(c['value'])
            elif c['type'] == 'number':
                if 'number_format' in c and c['number_format'] != None:
                    cell.number_format = c['number_format']
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
