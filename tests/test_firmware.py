"""Compile real firmware functions with deterministic host-side I/O fakes."""
import pathlib
import re
import subprocess
import tempfile
import unittest
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[1]
SOURCE = (ROOT / 'PlatformIO/src/main.cpp').read_text()
SKETCH = 'LayerGlow_v' + re.search(r'// LayerGlow v(\d+\.\d+)', SOURCE).group(1).replace('.', '_')

def function(name):
    match = re.search(r'^(?:bool|void) ' + name + r'\([^\n]*\)\{', SOURCE, re.M)
    if not match:
        raise AssertionError(name)
    start = match.start(); pos = SOURCE.index('{', start); depth = 0
    tokens = re.compile(r'"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'|//[^\n]*|/\*[\s\S]*?\*/|[{}]')
    for token in tokens.finditer(SOURCE, pos):
        if token.group() == '{': depth += 1
        if token.group() == '}':
            depth -= 1
            if depth == 0: return SOURCE[start:token.end()]
    raise AssertionError('Unclosed ' + name)

class FirmwareTests(unittest.TestCase):
    def test_arduino_mirror(self):
        self.assertEqual(SOURCE, (ROOT / f'Arduino/{SKETCH}/{SKETCH}.ino').read_text())
        for name in ('PubSubClient.cpp', 'PubSubClient.h'):
            self.assertEqual((ROOT / 'PlatformIO/src/src' / name).read_bytes(), (ROOT / f'Arduino/{SKETCH}/src' / name).read_bytes())

    def test_source_zip_matches_sketch(self):
        with zipfile.ZipFile(ROOT / f'Software/{SKETCH}.zip') as archive:
            self.assertEqual(SOURCE, archive.read(f'{SKETCH}/{SKETCH}.ino').decode())
            for name in ('PubSubClient.cpp', 'PubSubClient.h', 'PubSubClient-LICENSE.txt'):
                self.assertEqual((ROOT / f'Arduino/{SKETCH}/src' / name).read_bytes(), archive.read(f'{SKETCH}/src/' + name))

    def test_connection_storage_origin_and_ota_regressions(self):
        harness = (ROOT / 'tests/firmware_harness.cpp').read_text()
        names = ('storeString','storeInt','storeByte','storePort','storeBool','removeStored','saveWifiSettings','savePrinterSettings','saveLighting','markPrinterOffline','maintainBambu','maintainNetwork','allowMutation')
        code = '\n'.join(function(name) for name in names)
        structs = SOURCE[SOURCE.index('struct HttpPrinterRequest'):SOURCE.index('void readHttpPrinter(')]
        code += '\n' + structs + '\n' + function('maintainHttpPrinter')
        ota_start = SOURCE.index('  server.on("/api/update",HTTP_POST')
        ota_end = SOURCE.index('\n  });', ota_start) + len('\n  });')
        code += '\nvoid registerOta(){\n' + SOURCE[ota_start:ota_end] + '\n}\n'
        harness = harness.replace('// FIRMWARE_FUNCTIONS', code)
        with tempfile.TemporaryDirectory() as folder:
            cpp = pathlib.Path(folder) / 'test.cpp'; binary = pathlib.Path(folder) / 'test'
            cpp.write_text(harness)
            subprocess.run(['g++','-std=c++17','-Wall','-Wextra','-Werror',str(cpp),'-o',str(binary)], check=True)
            subprocess.run([str(binary)], check=True)

if __name__ == '__main__': unittest.main()
