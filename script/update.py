from openpyxl import Workbook
from openpyxl import load_workbook
from pathlib import Path
import datetime
import sys

app_dir = Path(sys.argv[0]).parent

dir_out = Path(sys.argv[1]) if len(sys.argv) >= 2 else app_dir.joinpath('out')
dir_schede = Path(sys.argv[2] if len(sys.argv) >= 3 else 'W:/schede/')

for f in dir_out.rglob('*.xlsx'):
    nome_cliente = f.stem

    try:
        wb = load_workbook(f)
        ws = wb.active
    except:
        continue

    old_filename = ''

    for row in tuple(ws.rows)[1:]:
        codice_pod = row[1].value
        mese_fattura = row[2].value
        num_fattura = row[4].value
        if num_fattura == None or codice_pod == None:
            continue
        if not isinstance(mese_fattura, datetime.datetime):
            mese_fattura = datetime.datetime.strptime(mese_fattura, '%Y-%m')

        filename = dir_schede.joinpath('BE {0} {1}.xlsx'.format(nome_cliente, codice_pod[-3:]))
        if not filename.exists():
            filename = dir_schede.joinpath('BE {0} {1}.xlsx'.format(nome_cliente, codice_pod[-4:]))
        if filename != old_filename:
            if old_filename != '':
                wb_scheda.save(old_filename)
            try:
                wb_scheda = load_workbook(filename)
                ws_scheda = wb_scheda['In Dati']
            except FileNotFoundError:
                print(filename, 'Non trovato')
                continue
            old_filename = filename

        found = False
        for row_scheda in tuple(ws_scheda.rows)[2:]:
            mese_scheda = row_scheda[0].value
            if not isinstance(mese_scheda, datetime.datetime):
                mese_scheda = datetime.datetime.strptime(mese_scheda, '%Y-%m')
            if row_scheda[2].value == num_fattura and mese_scheda == mese_fattura:
                found = True
                for cell_out, cell_scheda in zip(row[3:28], row_scheda[1:26]):
                    if isinstance(cell_out.value,str) and cell_out.value[-1:] == '%':
                        cell_out.number_format = '0%'
                        cell_out.value = float(cell_out.value[:-1]) / 100
                    elif isinstance(cell_out.value,datetime.datetime):
                        cell_out.number_format = 'DD/MM/YY'
                    cell_scheda.number_format = cell_out.number_format
                    cell_scheda.value = cell_out.value
        if (found):
            print(nome_cliente, codice_pod, num_fattura, 'Update')
        else:
            print(nome_cliente, codice_pod, num_fattura)
    
    wb_scheda.save(filename)
