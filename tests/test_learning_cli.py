# SPDX-License-Identifier: GPL-3.0-only
import importlib.util
from pathlib import Path
import struct
import tempfile
import unittest

spec=importlib.util.spec_from_file_location('learning',Path(__file__).resolve().parents[1]/'software/script/chameleon_learning.py')
cli=importlib.util.module_from_spec(spec);spec.loader.exec_module(cli)

def reply(command, payload=b'',status=0x68):
    h=b'\x11\xef'+struct.pack('>HHH',command,status,len(payload))
    return h+bytes([(-sum(h))&255])+payload+bytes([(-sum(payload))&255])

class Serial:
    def __init__(self, respond): self.respond=respond;self.incoming=bytearray();self.sent=[]
    def write(self, packet):
        self.sent.append(packet);self.incoming.extend(self.respond(packet));return len(packet)
    def read(self,count):
        count=min(count,3);out=bytes(self.incoming[:count]);del self.incoming[:count];return out

class FakeDevice:
    def __init__(self): self.calls=[];self.mode=0;self.current=0;self.fail=None
    def request(self,command,payload=b'',success=0x68):
        self.calls.append((command,bytes(payload)))
        if self.fail==command: raise RuntimeError('flash failure')
        if command==1100:
            data=bytearray(30);data[0]=1;data[2]=self.current;data[3]=data[12]=255;return data
        return b''
    def request_mode(self,mode): old=self.mode;self.mode=mode;return old
    def select(self,slot): self.current=slot

class Tests(unittest.TestCase):
    def test_wire_limits(self):
        data=bytes(range(256))*2;packet=cli.frame(1103,data)
        self.assertEqual(len(packet),522);self.assertEqual(sum(packet[:9])&255,0);self.assertEqual(sum(packet[9:])&255,0)
        with self.assertRaises(ValueError):cli.frame(1100,b'x'*513)
    def test_fragmented_corrupt_and_unrelated_reply(self):
        broken=bytearray(reply(1100,b'bad'));broken[-1]^=1
        serial=Serial(lambda p:b'noise'+broken+reply(123,b'old')+reply(1100,b'good'))
        self.assertEqual(cli.Client(serial).request(1100),b'good')
    def test_status_and_timeout(self):
        with self.assertRaisesRegex(RuntimeError,'busy'):cli.Client(Serial(lambda p:reply(1107,status=0x6a))).request(1107)
        with self.assertRaises(TimeoutError):cli.Client(Serial(lambda p:b''),timeout=0.001).request(1100)
    def test_legacy_modes(self):
        def respond(p):return reply(struct.unpack_from('>H',p,2)[0],b'\0' if p[3]==0xea else b'',0)
        # command 1002 = 0x03ea
        serial=Serial(respond);self.assertEqual(cli.Client(serial).request_mode(1),0);self.assertEqual(len(serial.sent),2)
    def test_status_layout(self):
        data=bytearray(30);data[0]=1;data[1]=2;data[3]=255;data[6]=7;data[12]=255;data[28]=5
        result=cli.status_decode(data);self.assertEqual(result['mode'],'auto');self.assertEqual(result['eligible_slots'],[1,3]);self.assertIsNone(result['recommendation'])
        with self.assertRaises(ValueError):cli.status_decode(data[:-1])
    def test_import_validates_before_device_change(self):
        device=FakeDevice()
        with tempfile.TemporaryDirectory() as folder:
            p=Path(folder)/'card.bin';p.write_bytes(b'x'*123)
            args=cli.parser().parse_args(['--port','fake','import-mf1','2',str(p)])
            with self.assertRaises(ValueError):cli.execute(device,args)
            self.assertEqual(device.mode,0);self.assertEqual(device.calls,[])
    def test_full_4k_import_and_partial_failure(self):
        with tempfile.TemporaryDirectory() as folder:
            p=Path(folder)/'card.bin';p.write_bytes(bytes(range(256))*16)
            args=cli.parser().parse_args(['--port','fake','import-mf1','8',str(p)])
            device=FakeDevice();cli.execute(device,args)
            blocks=[payload for command,payload in device.calls if command==1007]
            self.assertEqual(b''.join(x[2:] for x in blocks),p.read_bytes());self.assertTrue(all(len(x)<=512 for x in blocks));self.assertEqual(device.mode,0)
            device=FakeDevice();device.fail=1007
            with self.assertRaisesRegex(RuntimeError,'reader mode'):cli.execute(device,args)
            self.assertEqual(device.mode,1)
    def test_slots_bounds(self):
        self.assertEqual(cli.slot_value('8'),7)
        with self.assertRaises(cli.argparse.ArgumentTypeError):cli.slot_value('9')

if __name__=='__main__':unittest.main()
