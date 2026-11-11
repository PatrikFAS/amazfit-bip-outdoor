import pathlib
import subprocess
import tempfile
import unittest
import zlib

ROOT = pathlib.Path(__file__).resolve().parents[1]
SCRIPT = ROOT / 'scripts/import-tides.py'
NOTE = 'Point Atkinson CHS datum CD; BC UTC-420; no DST!'

class ImportTidesTests(unittest.TestCase):
    def run_import(self, root, rows, note=NOTE):
        inp, out = root / 'tides.csv', root / 'tides.bipt'
        inp.write_text('timestamp_utc,kind,height_m\n' + rows)
        args = [
            'python3', str(SCRIPT), str(inp), str(out),
            '--station', 'Point Atkinson CHS07795',
            '--latitude', '49.3375', '--longitude', '-123.253583',
            '--utc-offset-minutes', '-420', '--source', 'CHS/IWLS official API',
            '--note', note, '--generated-utc', '1794150000',
            '--expires-utc', '1795478400',
        ]
        return subprocess.run(args, capture_output=True, text=True), out

    def test_valid_file_and_python_to_c_round_trip(self):
        rows = (
            '2026-11-08T01:00:00Z,high,3.25\n'
            '2026-11-08T07:00:00+00:00,low,-0.50\n'
            '2026-11-08T13:00:00Z,high,4.01\n'
        )
        self.assertEqual(len(NOTE), 48)
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            result, output = self.run_import(root, rows)
            self.assertEqual(result.returncode, 0, result.stderr)
            data = output.read_bytes()
            self.assertEqual(data[:5], b'BIPT\x01')
            self.assertEqual(data[5], 3)
            self.assertEqual(len(data), 148 + 3 * 8)
            self.assertEqual(int.from_bytes(data[-4:], 'little'), zlib.crc32(data[:-4]))
            self.assertEqual(data[24:26], (-420).to_bytes(2, 'little', signed=True))
            self.assertEqual(data[26:29], bytes((23, 21, 48)))
            self.assertEqual(data[96:144].decode('ascii'), NOTE)
            executable = root / 'test_outdoor_data'
            subprocess.run([
                'cc', '-std=c99', '-Wall', '-Wextra', '-Werror', '-pedantic',
                '-I' + str(ROOT / 'src'), str(ROOT / 'src/outdoor_data.c'),
                str(ROOT / 'tests/test_outdoor_data.c'), '-lm', '-o', str(executable),
            ], check=True)
            checked = subprocess.run([str(executable), str(output)], capture_output=True, text=True)
            self.assertEqual(checked.returncode, 0, checked.stderr)

    def test_rejects_unsorted_and_local_time(self):
        with tempfile.TemporaryDirectory() as td:
            root = pathlib.Path(td)
            result, _ = self.run_import(root,
                '2026-11-08T07:00:00Z,high,1\n2026-11-08T01:00:00Z,low,0\n')
            self.assertNotEqual(result.returncode, 0)
            self.assertIn('strictly sorted', result.stderr)
            result, _ = self.run_import(root, '2026-11-08T01:00:00,high,1\n')
            self.assertNotEqual(result.returncode, 0)

if __name__ == '__main__':
    unittest.main()
