#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Build an application-only signed DFU ZIP for the official Ultra bootloader."""
import argparse
import hashlib
import json
from pathlib import Path
import tempfile
import zipfile

from cryptography.hazmat.primitives import hashes, serialization
from cryptography.hazmat.primitives.asymmetric import ec, utils

import dfu_cc_pb2 as pb
from verify_dfu import ROOT, HW_VERSION, SD_REQ, APP_VERSION, image_bytes, public_key, verify_package


def create_package(hex_path, output, key_path):
    binary = image_bytes(hex_path)
    key = serialization.load_pem_private_key(key_path.read_bytes(), password=None)
    if not isinstance(key, ec.EllipticCurvePrivateKey) or not isinstance(key.curve, ec.SECP256R1):
        raise ValueError('an ECDSA P-256 compatibility key is required')
    if key.public_key().public_numbers() != public_key().public_numbers():
        raise ValueError('signing key does not match the official Ultra bootloader')
    init = pb.InitCommand(fw_version=APP_VERSION, hw_version=HW_VERSION, sd_req=[SD_REQ],
                          type=pb.APPLICATION, sd_size=0, bl_size=0, app_size=len(binary), is_debug=False)
    init.hash.hash_type = pb.SHA256
    init.hash.hash = hashlib.sha256(binary).digest()[::-1]
    init.boot_validation.add(type=pb.VALIDATE_GENERATED_CRC, bytes=b'')
    signature = key.sign(init.SerializeToString(), ec.ECDSA(hashes.SHA256()))
    r, s = utils.decode_dss_signature(signature)
    packet = pb.Packet()
    packet.signed_command.command.op_code = pb.INIT
    packet.signed_command.command.init.CopyFrom(init)
    packet.signed_command.signature_type = pb.ECDSA_P256_SHA256
    packet.signed_command.signature = r.to_bytes(32, 'little') + s.to_bytes(32, 'little')
    manifest = {'manifest': {'application': {'bin_file': 'application.bin', 'dat_file': 'application.dat'}}}
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(dir=output.parent) as folder:
        temporary = Path(folder) / 'package.zip'
        with zipfile.ZipFile(temporary, 'w', compression=zipfile.ZIP_DEFLATED) as archive:
            archive.writestr('manifest.json', json.dumps(manifest, indent=2) + '\n')
            archive.writestr('application.bin', binary)
            archive.writestr('application.dat', packet.SerializeToString())
        result = verify_package(temporary, hex_path)
        temporary.replace(output)
    return result


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('--hex', type=Path, default=ROOT / 'build/firmware/chameleon-learning.hex')
    parser.add_argument('--output', type=Path, default=ROOT / 'build/firmware/ultra-dfu-app.zip')
    parser.add_argument('--key-file', type=Path, default=ROOT / 'resource/chameleon.pem',
                        help='inherited shared official key; never printed or included in ZIP')
    args = parser.parse_args()
    manifest_path = args.output.parent / 'manifest.json'
    manifest = None
    if manifest_path.is_file():
        manifest = json.loads(manifest_path.read_text())
        if manifest.get('sha256', {}).get(args.hex.name) != hashlib.sha256(args.hex.read_bytes()).hexdigest():
            raise ValueError('build manifest does not match the input HEX')
    result = create_package(args.hex, args.output, args.key_file)
    if manifest is not None:
        manifest['dfu'] = result
        manifest['sha256'][args.output.name] = result['zip_sha256']
        manifest_path.write_text(json.dumps(manifest, indent=2) + '\n')
    print(f'Verified signed Ultra application package: {args.output}')
    print(json.dumps(result, indent=2))


if __name__ == '__main__':
    main()
