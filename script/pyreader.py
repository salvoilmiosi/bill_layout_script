import subprocess
from subprocess import TimeoutExpired
import json
from pathlib import Path

class LayoutError(Exception):
    pass

class FatalError(Exception):
    pass

def readpdf(pdf_file, controllo, cached=False, recursive=False):
    try:
        args = [Path(__file__).parent.parent / 'build/reader', pdf_file, controllo]
        if cached: args.append('-c')
        if recursive: args.append('-r')
        proc = subprocess.run(args, capture_output=True, text=True, timeout=5)
        json_out = json.loads(proc.stdout)
        if 'error' in json_out:
            raise LayoutError(json_out['error'])
        else:
            return json_out
    except json.JSONDecodeError:
        raise FatalError(f'Output non valido: {proc.stdout}')
    except TimeoutExpired:
        raise FatalError('Timeout')