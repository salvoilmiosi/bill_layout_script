import subprocess
import sys
import json
from os import path
from pathlib import Path

def read_file(out, app_dir, pdf_file, layout_string, layout_dir):

    args = [app_dir + 'bin/layout_reader', '-p', pdf_file, '-l', layout_dir, '-s', '-']
    proc = subprocess.run(args, input=layout_string, capture_output=True, text=True)

    def write_value(name, index=0):
        try:
            out.write(json_values[name][index])
        except (KeyError, IndexError):
            pass
        out.write(';')

    out.write(str(pdf_file) + ';')
    try:
        json_output = json.loads(proc.stdout)
        json_values = json_output['values']
        write_value('numero_fattura')
        write_value('codice_pod')
        write_value('numero_cliente')
        write_value('ragione_sociale')
        write_value('periodo')
        write_value('totale_fattura')
        write_value('spesa_materia_energia')
        write_value('trasporto_gestione')
        write_value('oneri')
        write_value('energia_attiva', 0)
        write_value('energia_attiva', 1)
        write_value('energia_attiva', 2)
        write_value('energia_reattiva', 0)
        write_value('energia_reattiva', 1)
        write_value('energia_reattiva', 2)
        write_value('potenza')
        write_value('imponibile')
        if 'conguaglio' in json_values: out.write('CONGUAGLIO')
        out.write('\n')
    except:
        out.write(str(pdf_file) + ';Impossibile leggere questo file\n')
    
    out.flush()


if __name__ == '__main__':

    if len(sys.argv) < 4:
        print('Argomenti richiesti: input_direcory controllo.txt output')
        sys.exit()

    app_dir = path.dirname(sys.argv[0])

    input_directory = sys.argv[1]
    file_layout = sys.argv[2]
    output_file = sys.argv[3]

    layout_dir = path.dirname(file_layout)

    if not path.exists(input_directory):
        print('La directory {0} non esiste'.format(input_directory))
        sys.exit(1)
    
    if not path.exists(file_layout):
        print('Il file di layout {0} non esiste'.format(file_layout))
        sys.exit(1)
    
    with open(file_layout, 'r') as content_file:
        layout_string = content_file.read()
    
    out = sys.stdout if output_file == '-' else open(output_file, 'w')

    out.write("Nome file;Numero Fattura;Codice POD;Numero cliente;Ragione Sociale;"
              "Periodo fattura;Totale fattura;Spesa materia energia;Trasporto gestione;"
              "Oneri di sistema;F1;F2;F3;R1;R2;R3;Potenza;Imponibile;\n")

    for path in Path(input_directory).rglob('*.pdf'):
        read_file(out, app_dir, path, layout_string, layout_dir)
    
    out.close()