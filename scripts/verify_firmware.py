#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Reject application images that overwrite SoftDevice, FDS, bootloader or RAM."""
import argparse
from pathlib import Path
import struct

FLASH_START=0x27000
FLASH_END=0xc7000
RAM_START=0x20006000
RAM_END=0x20038000

def parse_hex(path):
    memory={};base=0;ended=False
    for line in path.read_text().splitlines():
        if ended: raise ValueError('data after HEX end marker')
        if not line.startswith(':'): raise ValueError('invalid Intel HEX line')
        record=bytes.fromhex(line[1:])
        if len(record)<5 or len(record)!=record[0]+5 or sum(record)&255: raise ValueError('invalid Intel HEX length/checksum')
        length,address,kind=record[0],int.from_bytes(record[1:3],'big'),record[3]
        data=record[4:-1]
        if kind==0:
            for index,value in enumerate(data):
                location=base+address+index
                if not FLASH_START<=location<FLASH_END: raise ValueError(f'image writes outside application flash at 0x{location:x}')
                if location in memory: raise ValueError('overlapping Intel HEX records')
                memory[location]=value
        elif kind==1:
            if length or address: raise ValueError('invalid HEX end marker')
            ended=True
        elif kind==2:
            if length!=2 or address: raise ValueError('invalid HEX segment address')
            base=int.from_bytes(data,'big')<<4
        elif kind==4:
            if length!=2 or address: raise ValueError('invalid HEX extended address')
            base=int.from_bytes(data,'big')<<16
        elif kind in (3,5):
            if length!=4: raise ValueError('invalid HEX start address')
        else: raise ValueError(f'unsupported HEX record type {kind}')
    if not ended or not memory: raise ValueError('incomplete or empty image')
    if any(FLASH_START+i not in memory for i in range(8)): raise ValueError('missing vector table')
    vector=bytes(memory[FLASH_START+i] for i in range(8))
    stack,reset=struct.unpack('<II',vector)
    if stack!=RAM_END or stack%8: raise ValueError(f'incorrect initial stack: 0x{stack:x}')
    if not reset&1 or not FLASH_START<=reset-1<FLASH_END: raise ValueError('invalid reset vector')
    return memory

def main():
    p=argparse.ArgumentParser(description=__doc__);p.add_argument('hex',type=Path);args=p.parse_args()
    memory=parse_hex(args.hex)
    print(f'Image verified: {len(memory)} bytes, 0x{min(memory):x}..0x{max(memory):x}; reserved regions untouched')
if __name__=='__main__':main()
