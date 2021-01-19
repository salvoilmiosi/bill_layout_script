from multiprocessing.pool import Pool
from collections import Counter
from pathlib import Path
import sys
import os

os.environ['PATH'] += os.pathsep + str(Path(sys.argv[0]).parent.joinpath('../bin'))
import pyreader

app_dir = Path(sys.argv[0]).parent
controllo = app_dir / '../work/layouts/controllo.out'
input_directory = app_dir / '../work/fatture'

def check_layout(pdf_file):
    try:
        print(pdf_file)
        return pyreader.getlayout(pdf_file, controllo)
    except:
        return None

if __name__ == '__main__':
    files = list(input_directory.rglob('*.pdf'))

    with Pool(min(len(files), 5)) as pool:
        out = pool.map(check_layout, files)

    print(Counter(out))