# SPDX-License-Identifier: GPL-3.0-only
import hashlib,json,re,subprocess,unittest
from pathlib import Path
ROOT=Path(__file__).resolve().parents[1]
class Tests(unittest.TestCase):
    def test_upstream_sync_integrity(self):
        reference=json.loads((ROOT/'docs/UPSTREAM_SYNC.json').read_text())
        patches=set(reference['local_patches'])
        tracked=set(subprocess.check_output(['git','-C',str(ROOT),'ls-files','--cached'],text=True).splitlines())
        self.assertTrue(set(reference['files'])<=tracked, 'Upstream files must be committed, including ignored paths')
        self.assertEqual(reference['commit'],'5c99d4a39b424cc67ae82bbcfc8ba5ec8f69bf9c')
        for path,expected in reference['files'].items():
            if path in patches: continue
            data=(ROOT/path).read_bytes()
            actual=hashlib.sha1(b'blob '+str(len(data)).encode()+b'\0'+data).hexdigest()
            self.assertEqual(actual,expected,path)
        for path in ['firmware/application/src/rfid/reader/hf/rc522.c','firmware/application/src/rfid/reader/hf/mf1_toolbox.c','firmware/application/src/rgb_marquee.c','firmware/common/hw_connect.c']:
            self.assertNotIn(path,patches)
    def test_usb_product_descriptor_fits_sdk_buffer(self):
        config=(ROOT/'firmware/application/src/sdk_config.h').read_text()
        capacity=int(re.search(r'^#define APP_USBD_CONFIG_DESC_STRING_SIZE (\d+)',config,re.M)[1])
        names=re.findall(r'^#define DEVICE_NAME_STR\s+"([^"]+)"',(ROOT/'firmware/common/device_info.h').read_text(),re.M)
        self.assertTrue(names)
        for name in names:
            # Hardware is uint8_t and firmware version is uint16_t.
            longest=f'{name}: hw_v255, fw_v65535'
            self.assertLessEqual(len(longest),capacity)
            self.assertLessEqual(2+2*len(longest),254)
        self.assertGreater(2+2*capacity,64, 'Use a dedicated SDK string buffer for the long custom name')
    def test_private_model_id_does_not_overlap_official(self):
        header=(ROOT/'firmware/application/src/selection/storage_ids.h').read_text()
        model=int(re.search(r'#define CL_MODEL_FILE 0x([0-9A-F]+)',header)[1],16)
        official={0x1000,0x1001,*range(0x1066,0x106a),*range(0x1100,0x1108),*range(0x1200,0x1208)}
        self.assertNotIn(model,official);self.assertLess(model,0xC000)
    def test_extension_command_ids_do_not_alias_upstream(self):
        official=(ROOT/'firmware/application/src/data_cmd.h').read_text()
        custom=(ROOT/'firmware/application/src/selection/learning_commands.h').read_text()
        ids=lambda s:{int(n) for n in re.findall(r'^#define\s+DATA_CMD_\w+\s+\((\d+)\)',s,re.M)}
        self.assertEqual(ids(custom),set(range(1100,1109)))
        self.assertFalse(ids(custom)&ids(official))
if __name__=='__main__':unittest.main()
