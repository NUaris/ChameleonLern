# SPDX-License-Identifier: GPL-3.0-only
import hashlib
import json
from pathlib import Path
import struct
import sys
import tempfile
import unittest
import zipfile

from cryptography.hazmat.primitives import serialization
from cryptography.hazmat.primitives.asymmetric import ec

ROOT = Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'scripts'))
import dfu_cc_pb2 as pb
import package_dfu
import verify_dfu


def record(address, kind, data):
    body = bytes([len(data)]) + address.to_bytes(2, 'big') + bytes([kind]) + data
    return ':' + (body + bytes([-sum(body) & 255])).hex()


class Tests(unittest.TestCase):
    def setUp(self):
        self.folder = tempfile.TemporaryDirectory()
        self.addCleanup(self.folder.cleanup)
        self.root = Path(self.folder.name)
        self.hex = self.root / 'test.hex'
        self.hex.write_text('\n'.join([record(0, 4, b'\x00\x02'),
            record(0x7000, 0, struct.pack('<II', 0x20038000, 0x27009) + b'\x00\xbf\xfe\xe7'),
            record(0, 1, b'')]) + '\n')
        self.output = self.root / 'ultra.zip'
        package_dfu.create_package(self.hex, self.output, ROOT / 'resource/chameleon.pem')

    def mutate(self, change):
        with zipfile.ZipFile(self.output) as archive:
            parts = {name: archive.read(name) for name in archive.namelist()}
        change(parts)
        with zipfile.ZipFile(self.output, 'w') as archive:
            for name, data in parts.items(): archive.writestr(name, data)

    def init_mutation(self, change):
        def modify(parts):
            packet = pb.Packet.FromString(parts['application.dat'])
            change(packet)
            parts['application.dat'] = packet.SerializeToString()
        self.mutate(modify)

    def test_signed_ultra_application(self):
        result = verify_dfu.verify_package(self.output, self.hex)
        self.assertEqual(result['hw_version'], 0)
        self.assertEqual(result['sd_req'], [0x100])
        self.assertEqual(result['application_version'], 1)
        self.assertTrue(result['signed'])
        self.assertEqual(result['zip_sha256'], hashlib.sha256(self.output.read_bytes()).hexdigest())
        self.assertFalse(result['hardware_tested'])

    def test_nordic_reference_fixture(self):
        # Generated independently with Nordic nrfutil 6.1.7, not this packer.
        self.mutate(lambda parts: parts.update({
            'application.dat': (ROOT / 'tests/fixtures/nordic-ultra.dat').read_bytes(),
            'application.bin': (ROOT / 'tests/fixtures/nordic-ultra.bin').read_bytes()}))
        self.assertTrue(verify_dfu.verify_package(self.output, self.hex)['signed'])

    def test_payload_tampering(self):
        self.mutate(lambda parts: parts.update({'application.bin': parts['application.bin'][:-1] + b'\0'}))
        with self.assertRaisesRegex(ValueError, 'SHA256'): verify_dfu.verify_package(self.output)

    def test_signature_tampering(self):
        self.init_mutation(lambda p: setattr(p.signed_command, 'signature', b'\0' * 64))
        with self.assertRaisesRegex(ValueError, 'signature'): verify_dfu.verify_package(self.output)

    def test_lite_rejected(self):
        self.init_mutation(lambda p: setattr(p.signed_command.command.init, 'hw_version', 1))
        with self.assertRaisesRegex(ValueError, 'hardware'): verify_dfu.verify_package(self.output)

    def test_wrong_softdevice_rejected(self):
        def change(packet): packet.signed_command.command.init.sd_req[:] = [0xae]
        self.init_mutation(change)
        with self.assertRaisesRegex(ValueError, 'SoftDevice'): verify_dfu.verify_package(self.output)

    def test_high_version_rejected_to_preserve_official_return(self):
        self.init_mutation(lambda p: setattr(p.signed_command.command.init, 'fw_version', 20261004))
        with self.assertRaisesRegex(ValueError, 'version'): verify_dfu.verify_package(self.output)

    def test_unsigned_and_debug_rejected(self):
        self.init_mutation(lambda p: setattr(p.signed_command.command.init, 'is_debug', True))
        with self.assertRaisesRegex(ValueError, 'non-debug'): verify_dfu.verify_package(self.output)
        def unsigned(parts):
            p = pb.Packet.FromString(parts['application.dat'])
            p.command.CopyFrom(p.signed_command.command); p.ClearField('signed_command')
            parts['application.dat'] = p.SerializeToString()
        self.mutate(unsigned)
        with self.assertRaisesRegex(ValueError, 'signed'): verify_dfu.verify_package(self.output)

    def test_bootloader_or_extra_files_rejected(self):
        def add_bootloader(parts):
            manifest = json.loads(parts['manifest.json']); manifest['manifest']['bootloader'] = {}
            parts['manifest.json'] = json.dumps(manifest)
        self.mutate(add_bootloader)
        with self.assertRaisesRegex(ValueError, 'only application'): verify_dfu.verify_package(self.output)
        self.mutate(lambda parts: parts.update({'extra.pem': b'never include keys'}))
        with self.assertRaisesRegex(ValueError, 'exactly'): verify_dfu.verify_package(self.output)

    def test_foreign_signing_key_rejected(self):
        path = self.root / 'foreign.pem'
        key = ec.generate_private_key(ec.SECP256R1())
        path.write_bytes(key.private_bytes(serialization.Encoding.PEM, serialization.PrivateFormat.PKCS8, serialization.NoEncryption()))
        with self.assertRaisesRegex(ValueError, 'does not match'):
            package_dfu.create_package(self.hex, self.output, path)

    def test_hex_mismatch_and_reserved_regions_rejected(self):
        self.hex.write_text(self.hex.read_text().replace('00bf', '01bf'))
        with self.assertRaises(ValueError): verify_dfu.verify_package(self.output, self.hex)
        self.hex.write_text('\n'.join([record(0, 4, b'\x00\x00'), record(0, 0, b'\0' * 8), record(0, 1, b'')]))
        with self.assertRaisesRegex(ValueError, 'outside'): package_dfu.create_package(self.hex, self.output, ROOT / 'resource/chameleon.pem')


if __name__ == '__main__': unittest.main()
