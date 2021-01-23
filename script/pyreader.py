import subprocess
from subprocess import TimeoutExpired
import json
from pathlib import Path

class Error(Exception):
    pass

class Timeout(TimeoutError):
    pass

def readpdf(pdf_file, controllo):
    try:
        args = [Path(__file__).parent.parent / 'build/reader', pdf_file, controllo]
        proc = subprocess.run(args, capture_output=True, text=True, timeout=5)
        json_out = json.loads(proc.stdout)
        if 'error' in json_out:
            raise Error(json_out['error'])
        else:
            return json_out
    except json.JSONDecodeError:
        raise Timeout('Output non valido')
    except TimeoutExpired:
        raise Timeout('Timeout')