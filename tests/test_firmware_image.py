# SPDX-License-Identifier: GPL-3.0-only
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest
spec=importlib.util.spec_from_file_location('image',Path(__file__).resolve().parents[1]/'scripts/verify_firmware.py')
image=importlib.util.module_from_spec(spec);spec.loader.exec_module(image)
def record(address,kind,data):
    raw=bytes([len(data)])+address.to_bytes(2,'big')+bytes([kind])+data
    return ':'+(raw+bytes([(-sum(raw))&255])).hex()
class Tests(unittest.TestCase):
    def parse(self, lines):
        with tempfile.TemporaryDirectory() as folder:
            p=Path(folder)/'image.hex';p.write_text('\n'.join(lines));return image.parse_hex(p)
    def valid(self):return [record(0,4,b'\0\2'),record(0x7000,0,struct.pack('<II',image.RAM_END,0x27009)),record(0,1,b'')]
    def test_valid_vector(self):self.assertEqual(len(self.parse(self.valid())),8)
    def test_reserved_flash(self):
        lines=self.valid();lines.insert(2,record(0,4,b'\0\x0c'));lines.insert(3,record(0x7000,0,b'bad'))
        with self.assertRaisesRegex(ValueError,'outside'):self.parse(lines)
    def test_stack_and_corruption(self):
        lines=self.valid();lines[1]=record(0x7000,0,struct.pack('<II',0x20040000,0x27009))
        with self.assertRaisesRegex(ValueError,'stack'):self.parse(lines)
        lines=self.valid();lines[1]=lines[1][:-2]+'ab'
        with self.assertRaisesRegex(ValueError,'checksum'):self.parse(lines)
    def test_incomplete(self):
        with self.assertRaisesRegex(ValueError,'incomplete'):self.parse(self.valid()[:-1])
