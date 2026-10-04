#!/usr/bin/env python3
# SPDX-License-Identifier: GPL-3.0-only
"""Manage ChameleonLern learning and card slots over USB CDC (slots 1..8)."""
import argparse
import json
from pathlib import Path
import struct
import sys
import time

MODES = {'off': 0, 'observe': 1, 'auto': 2}
REASONS = ['off', 'empty', 'no_context', 'low_score', 'ambiguous', 'need_feedback', 'manual_hold', 'field_active', 'cooldown', 'ready', 'keep', 'invalid_slot']
ERRORS = {0x60:'invalid parameters', 0x66:'wrong device mode', 0x67:'invalid command', 0x6a:'reader field active / busy', 0x6b:'storage failed', 0x6c:'no usable context; scan first or provide recent reader feedback'}

def frame(command, payload=b''):
    if len(payload) > 512: raise ValueError('payload exceeds 512 bytes')
    header = b'\x11\xef' + struct.pack('>HHH', command, 0, len(payload))
    return header + bytes([(-sum(header)) & 255]) + payload + bytes([(-sum(payload)) & 255])

def slot_value(value):
    number = int(value)
    if not 1 <= number <= 8: raise argparse.ArgumentTypeError('slot must be 1..8')
    return number - 1

def slot_display(value): return None if value == 255 else value + 1

def reason(value): return REASONS[value] if value < len(REASONS) else f'unknown({value})'

def status_decode(data):
    if len(data) != 30 or data[0] != 1 or data[1] > 2: raise ValueError('unsupported status protocol')
    return dict(mode=list(MODES)[data[1]], current_slot=slot_display(data[2]), recommendation=slot_display(data[3]),
                score=data[4], margin=data[5], reason=reason(data[6]), evidence={'ble':bool(data[7]&1),'reader':bool(data[7]&2),'time':bool(data[7]&4)},
                samples=data[8], time_valid=bool(data[9]), hf_field=bool(data[10]), lf_field=bool(data[11]), pending_slot=slot_display(data[12]),
                dirty=bool(data[13]), scanning=bool(data[14]), storage_error=bool(data[15]), utc=struct.unpack_from('>I',data,16)[0],
                switches=struct.unpack_from('>I',data,20)[0], dropped_sessions=struct.unpack_from('>I',data,24)[0],
                eligible_slots=[i+1 for i in range(8) if data[28]&(1<<i)], observations=data[29])

class Client:
    def __init__(self, transport, timeout=35): self.transport=transport; self.timeout=timeout
    def exact(self, count, deadline):
        data=bytearray()
        while len(data)<count:
            if time.monotonic() >= deadline: raise TimeoutError('device response timed out; command may already have been applied')
            chunk=self.transport.read(count-len(data))
            if chunk: data.extend(chunk)
        return bytes(data)
    def request(self, command, payload=b'', success=0x68):
        packet=frame(command,payload)
        if self.transport.write(packet) != len(packet): raise IOError('incomplete serial write')
        deadline=time.monotonic()+self.timeout
        while True:
            if self.exact(1,deadline) != b'\x11': continue
            if self.exact(1,deadline) != b'\xef': continue
            tail=self.exact(7,deadline); header=b'\x11\xef'+tail
            if sum(header)&255: continue
            cmd,status,length=struct.unpack('>HHH',tail[:6])
            if length>512: continue
            body=self.exact(length+1,deadline)
            if sum(body)&255: continue
            if cmd != command: continue
            if status != success: raise RuntimeError(f'command {cmd}: {ERRORS.get(status, f"device status 0x{status:04x}")}')
            return body[:-1]
    def config(self):
        data=self.request(1108)
        if len(data)!=12: raise ValueError('invalid configuration response')
        return bytearray(data)
    def select(self, slot):
        self.request(1003,bytes([slot])); deadline=time.monotonic()+5
        while time.monotonic()<deadline:
            state=status_decode(self.request(1100))
            if state['current_slot']==slot+1 and state['pending_slot'] is None: return
            time.sleep(0.1)
        raise TimeoutError('slot change remains pending; move away from the reader field')

    def request_mode(self, mode):
        data=self.request(1002,success=0)
        if len(data)!=1 or data[0]>1: raise ValueError('invalid device mode response')
        self.request(1001,bytes([mode]),success=0)
        return data[0]

def parser():
    p=argparse.ArgumentParser(description=__doc__)
    p.add_argument('--port',required=True,help='/dev/ttyACM0 or COM3')
    sub=p.add_subparsers(dest='action',required=True)
    for name in ['status','predict','samples','slots','save']: sub.add_parser(name)
    dfu=sub.add_parser('enter-dfu')
    dfu.add_argument('--official',action='store_true',help='send without waiting for ACK: current official firmware resets immediately')
    mode=sub.add_parser('mode'); mode.add_argument('mode',choices=MODES)
    sync=sub.add_parser('sync-time'); sync.add_argument('--timezone',type=int,default=480,help='UTC offset in minutes (default +480)')
    train=sub.add_parser('train'); train.add_argument('slot',type=slot_value); train.add_argument('--source',choices=['environment','reader'],default='environment')
    forget=sub.add_parser('forget'); forget.add_argument('slot',type=lambda x:255 if x=='all' else slot_value(x))
    select=sub.add_parser('select'); select.add_argument('slot',type=slot_value)
    config=sub.add_parser('configure')
    for arg in ['min-score','margin','min-observations','ble-weight','reader-weight','time-weight','scan-period-ms','scan-window-ms']:
        config.add_argument('--'+arg,type=int)
    init=sub.add_parser('init-card'); init.add_argument('slot',type=slot_value); init.add_argument('type',choices=['mini','1k','2k','4k','em410x'])
    mf=sub.add_parser('import-mf1'); mf.add_argument('slot',type=slot_value); mf.add_argument('file',type=Path)
    lf=sub.add_parser('import-em410x'); lf.add_argument('slot',type=slot_value); lf.add_argument('id',help='ten hexadecimal digits')
    return p

def execute(client, args):
    action=args.action
    if action=='status': return status_decode(client.request(1100))
    if action=='enter-dfu':
        if args.official:
            packet=frame(1010)
            if client.transport.write(packet)!=len(packet): raise IOError('incomplete DFU request write')
            client.transport.flush()
            return {'dfu_requested':True,'acknowledged':False,'next':'check for the DFU USB port; official firmware resets without an ACK'}
        client.request(1010)
        return {'dfu_requested':True,'acknowledged':True,'next':'wait for the DFU USB port, then install the application ZIP'}
    if action=='save': client.request(1107)
    elif action=='mode':
        config=client.config(); config[0]=MODES[args.mode]; client.request(1101,config); client.request(1107)
    elif action=='sync-time':
        if not -720 <= args.timezone <= 840: raise ValueError('timezone must be -720..840 minutes')
        client.request(1102,struct.pack('>Ih',int(time.time()),args.timezone))
    elif action=='train': client.request(1103,bytes([args.slot,args.source=='reader'])); client.request(1107)
    elif action=='forget': client.request(1104,bytes([args.slot]))
    elif action=='select': client.select(args.slot)
    elif action=='configure':
        config=client.config()
        for index,name in enumerate(['mode','min_score','margin','min_observations','ble_weight','reader_weight','time_weight']):
            value=getattr(args,name,None)
            if value is not None:
                if not 0 <= value <= 255: raise ValueError(f'{name} must be 0..255')
                config[index]=value
        for offset,name in [(8,'scan_period_ms'),(10,'scan_window_ms')]:
            value=getattr(args,name)
            if value is not None:
                if not 0 <= value <= 65535: raise ValueError(f'{name} must be 0..65535')
                struct.pack_into('>H',config,offset,value)
        client.request(1101,config); client.request(1107)
    elif action=='predict':
        data=client.request(1105)
        if len(data)!=14: raise ValueError('invalid prediction response')
        return dict(slot=slot_display(data[0]),score=data[1],margin=data[2],reason=reason(data[3]),evidence=data[4],observations=data[5],scores=list(data[6:]))
    elif action=='samples':
        data=client.request(1106)
        if len(data)%13: raise ValueError('invalid sample response')
        return [dict(slot=data[i]+1,observations=data[i+1],beacons=data[i+2],reader_tokens=data[i+3],field=data[i+4],time_valid=bool(data[i+5]),weekday=data[i+6],minute=struct.unpack_from('>H',data,i+7)[0],order=struct.unpack_from('>I',data,i+9)[0]) for i in range(0,len(data),13)]
    elif action=='slots':
        data=client.request(1202)
        if len(data)!=25: raise ValueError('invalid slots response')
        return dict(current_slot=data[0]+1,slots=[dict(slot=i+1,enabled=bool(data[1+i*3]),hf_type=data[2+i*3],lf_type=data[3+i*3]) for i in range(8)])
    elif action in ['init-card','import-mf1','import-em410x']:
        # Validate inputs before modifying the device. Readers cannot see a partially imported card.
        dump=None; identity=None
        if action=='import-mf1':
            dump=args.file.read_bytes(); types={320:2,1024:3,2048:4,4096:5}
            if len(dump) not in types: raise ValueError('raw MIFARE dump must be 320/1024/2048/4096 bytes')
            tag_type=types[len(dump)]
        elif action=='import-em410x':
            identity=bytes.fromhex(args.id)
            if len(identity)!=5: raise ValueError('EM410x ID must contain five bytes')
            tag_type=1
        else: tag_type={'em410x':1,'mini':2,'1k':3,'2k':4,'4k':5}[args.type]
        # 1001 is a legacy command whose success status is zero.
        original=client.request_mode(1)
        try:
            client.request(1005,bytes([args.slot,tag_type])); client.select(args.slot)
            if dump is not None:
                for start in range(0,len(dump)//16,31):
                    count=min(31,len(dump)//16-start)
                    client.request(1201,bytes([start,count])+dump[start*16:(start+count)*16])
            if identity is not None: client.request(1200,identity)
            client.request(1107)
        except Exception:
            raise RuntimeError('card setup failed; device remains in reader mode to keep partial data off the antenna. Retry the import before using the card.')
        client.request_mode(original)
    return {'ok':True}


def main():
    args=parser().parse_args()
    try:
        import serial
        with serial.Serial(args.port,115200,timeout=0.1,write_timeout=3) as transport:
            transport.dtr=True; transport.reset_input_buffer()
            print(json.dumps(execute(Client(transport),args),ensure_ascii=False,indent=2))
    except (ImportError,OSError,ValueError,RuntimeError,TimeoutError) as error:
        print(f'Error: {error}',file=sys.stderr); return 1
    return 0

if __name__=='__main__': sys.exit(main())
