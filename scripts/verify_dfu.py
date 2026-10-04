#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Verify an application-only Nordic Secure DFU package for production Ultra."""
import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import zipfile

from cryptography.exceptions import InvalidSignature
from cryptography.hazmat.primitives import hashes
from cryptography.hazmat.primitives.asymmetric import ec, utils
from google.protobuf.message import DecodeError

import dfu_cc_pb2 as pb
from verify_firmware import FLASH_START, FLASH_END, RAM_END, parse_hex

ROOT = Path(__file__).resolve().parents[1]
PUBLIC_KEY_FILE = ROOT / 'resource/dfu_public_key.c'
OFFICIAL_PUBLIC_KEY_SHA256 = '24e16a9025ecb0b9608b03da512a9966797b117f8903bcffc437ef2b0985bc7a'
HW_VERSION = 0  # Ultra device type; Lite is 1, historical prototype used 52.
SD_REQ = 0x0100  # S140 7.2.0
APP_VERSION = 1  # Official also uses 1; larger numbers can prevent returning to official.


def public_key(path=PUBLIC_KEY_FILE):
    match = re.search(r'pk\s*\[\s*64\s*\]\s*=\s*\{([^}]+)\}', path.read_text(), re.S)
    if match is None:
        raise ValueError('missing DFU public key')
    raw = bytes(int(x, 16) for x in re.findall(r'0x([0-9a-fA-F]{2})\b', match[1]))
    if hashlib.sha256(raw).hexdigest() != OFFICIAL_PUBLIC_KEY_SHA256:
        raise ValueError('public key does not match the verified official Ultra key')
    numbers = ec.EllipticCurvePublicNumbers(int.from_bytes(raw[:32], 'little'),
                                           int.from_bytes(raw[32:], 'little'), ec.SECP256R1())
    return numbers.public_key()


def image_bytes(hex_path):
    memory = parse_hex(hex_path)
    end = (max(memory) + 4) & ~3
    return bytes(memory.get(i, 0xff) for i in range(FLASH_START, end))


def verify_package(path, hex_path=None):
    with zipfile.ZipFile(path) as archive:
        names = archive.namelist()
        if len(names) != 3 or len(set(names)) != 3:
            raise ValueError('DFU must contain exactly manifest, application BIN and DAT')
        if any(i.file_size > FLASH_END - FLASH_START for i in archive.infolist()):
            raise ValueError('oversized DFU member')
        manifest = json.loads(archive.read('manifest.json'))
        if set(manifest) != {'manifest'} or set(manifest['manifest']) != {'application'}:
            raise ValueError('only application updates are permitted')
        app = manifest['manifest']['application']
        if set(app) != {'bin_file', 'dat_file'}:
            raise ValueError('unexpected application manifest fields')
        if app != {'bin_file': 'application.bin', 'dat_file': 'application.dat'}:
            raise ValueError('unexpected DFU filenames')
        if set(names) != {'manifest.json', 'application.bin', 'application.dat'}:
            raise ValueError('unexpected DFU members')
        binary = archive.read(app['bin_file'])
        dat = archive.read(app['dat_file'])

    if not 8 <= len(binary) <= FLASH_END - FLASH_START or len(binary) % 4:
        raise ValueError('invalid application size/alignment')
    stack, reset = struct.unpack_from('<II', binary)
    if stack != RAM_END or not reset & 1 or not FLASH_START <= reset - 1 < FLASH_START + len(binary):
        raise ValueError('incorrect Ultra application vector table')
    if hex_path is not None and binary != image_bytes(hex_path):
        raise ValueError('DFU payload differs from verified HEX image')
    if not dat or len(dat) > 512:
        raise ValueError('invalid init packet size')
    packet = pb.Packet()
    try:
        packet.ParseFromString(dat)
    except DecodeError as error:
        raise ValueError('invalid Nordic init packet') from error
    if not packet.IsInitialized() or packet.HasField('command') or not packet.HasField('signed_command'):
        raise ValueError('signed init packet required')
    signed = packet.signed_command
    command = signed.command
    init = command.init
    if not command.HasField('op_code') or command.op_code != pb.INIT or not command.HasField('init'):
        raise ValueError('invalid init command')
    required = ['fw_version', 'hw_version', 'type', 'app_size', 'hash']
    if any(not init.HasField(field) for field in required):
        raise ValueError('missing compatibility fields')
    if init.hw_version != HW_VERSION or list(init.sd_req) != [SD_REQ] or init.fw_version != APP_VERSION:
        raise ValueError('incorrect Ultra hardware, SoftDevice or application version')
    if init.type != pb.APPLICATION or init.sd_size or init.bl_size or init.is_debug:
        raise ValueError('only non-debug application updates are permitted')
    if init.app_size != len(binary):
        raise ValueError('signed application size mismatch')
    if init.hash.hash_type != pb.SHA256 or init.hash.hash != hashlib.sha256(binary).digest()[::-1]:
        raise ValueError('application SHA256 mismatch')
    if len(init.boot_validation) != 1 or init.boot_validation[0].type != pb.VALIDATE_GENERATED_CRC or init.boot_validation[0].bytes:
        raise ValueError('expected generated CRC boot validation')
    if signed.signature_type != pb.ECDSA_P256_SHA256 or len(signed.signature) != 64:
        raise ValueError('invalid ECDSA signature format')
    r = int.from_bytes(signed.signature[:32], 'little')
    s = int.from_bytes(signed.signature[32:], 'little')
    try:
        # Nordic signs the InitCommand itself, not the enclosing Command/Packet.
        public_key().verify(utils.encode_dss_signature(r, s), init.SerializeToString(), ec.ECDSA(hashes.SHA256()))
    except (InvalidSignature, ValueError) as error:
        raise ValueError('DFU signature verification failed') from error
    return {'device': 'Chameleon Ultra', 'board_revision': 1, 'hw_version': HW_VERSION,
            'sd_req': [SD_REQ], 'application_version': APP_VERSION, 'application_size': len(binary),
            'signed': True, 'application_only': True, 'hardware_tested': False,
            'signing_identity': 'shared upstream official compatibility key',
            'public_key_sha256': OFFICIAL_PUBLIC_KEY_SHA256,
            'zip_sha256': hashlib.sha256(path.read_bytes()).hexdigest()}


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('zip', type=Path)
    parser.add_argument('--hex', type=Path)
    args = parser.parse_args()
    print(json.dumps(verify_package(args.zip, args.hex), indent=2))


if __name__ == '__main__':
    main()
