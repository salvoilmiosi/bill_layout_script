from pathlib import Path
import sys
import json

if len(sys.argv) <= 1:
    print('Specificare file di input')
    exit(0)

file_in = Path(sys.argv[1])
if not file_in.exists():
    print('Il file {0} non esiste'.format(file_in))
    exit(1)

file_out = file_in.parent.parent.joinpath(file_in.name)

with open(file_in, 'r') as file:
    json_bls = json.loads(file.read())

box_types = ('RECTANGLE','PAGE','FILE','DISABLED')
read_modes = ('DEFAULT','LAYOUT','RAW')

with open(file_out, 'w') as file:
    file.write('### Bill Layout Script\n')
    
    for box in json_bls['layout']['boxes']:
        file.write('\n')
        file.write('### Box {}\n'.format(box['name']))
        file.write('### Type {}\n'.format(box_types[box['type']]))
        file.write('### Mode {}\n'.format(read_modes[box['mode']]))
        file.write('### Page {}\n'.format(box['page']))
        file.write('### X {}\n'.format(box['x']))
        file.write('### Y {}\n'.format(box['y']))
        file.write('### W {}\n'.format(box['w']))
        file.write('### H {}\n'.format(box['h']))
        file.write('### Label {}\n'.format(box['goto_label']))
        file.write('### Spacers\n')
        file.write(box['spacers'])
        file.write('\n### Script\n')
        file.write(box['script'])
        file.write('\n### End Box\n')