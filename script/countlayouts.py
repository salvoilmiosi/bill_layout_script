from multiprocessing.pool import Pool
from pathlib import Path
from collections import Counter
import sys
import os

app_dir = Path(sys.argv[0]).parent
bin_dir = str(app_dir / '../bin')
sys.path.append(bin_dir)
os.environ['PATH'] = bin_dir
import pyreader

controllo = app_dir / '../work/layouts/controllo.out'
input_directory = app_dir / '../work/fatture'

def check_layout(pdf_file):
    try:
        print(pdf_file)
        return pyreader.getlayout(str(pdf_file), str(controllo))
    except:
        return None

if __name__ == '__main__':
    files = list(input_directory.rglob('*.pdf'))

    with Pool(min(len(files), 5)) as pool:
        out = pool.map(check_layout, files)

    print(Counter(out))