# SPDX-License-Identifier: GPL-3.0-only
from pathlib import Path
import re
import unittest

ROOT = Path(__file__).resolve().parents[1]


class Tests(unittest.TestCase):
    def test_production_pins_against_pinned_upstream(self):
        # Independent hardware reference: upstream hw_connect.c, Ultra HW v1,
        # commit 6d92a9ff1a56f93efbcaca10f547eee0a3dd6791.
        expected = {'LED_FIELD': (1,1), 'LED_R': (0,24), 'LED_G': (0,22), 'LED_B': (1,0),
            'LED_1': (0,20), 'LED_2': (0,17), 'LED_3': (0,15), 'LED_4': (0,13),
            'LED_5': (0,12), 'LED_6': (1,9), 'LED_7': (0,8), 'LED_8': (0,6),
            'LF_ANT_DRIVER': (0,31), 'LF_OA_OUT': (0,29), 'LF_MOD': (1,13), 'LF_RSSI_PIN': (0,2),
            'HF_SPI_SELECT': (1,6), 'HF_SPI_MISO': (0,11), 'HF_SPI_MOSI': (1,7), 'HF_SPI_SCK': (1,4),
            'HF_ANT_SEL': (1,10), 'BUTTON_1': (1,2), 'BUTTON_2': (0,26), 'BAT_SENSE': (0,4), 'READER_POWER': (1,15)}
        header = (ROOT / 'firmware/application/app/board_chameleon_ultra.h').read_text()
        actual = {name: (int(port), int(pin)) for name,port,pin in re.findall(
            r'#define\s+(\w+)\s+NRF_GPIO_PIN_MAP\(\s*(\d+)\s*,\s*(\d+)\s*\)', header)}
        self.assertEqual(actual, expected)
        self.assertIn('#include "board_chameleon_ultra.h"', (ROOT / 'firmware/application/app/rfid_main.h').read_text())

    def test_private_records_do_not_overlap_official_storage(self):
        header = (ROOT / 'firmware/application/app/storage_ids.h').read_text()
        ids = {name: int(value,16) for name,value in re.findall(r'#define\s+(CL_\w+_FILE)\s+0x([0-9A-Fa-f]+)', header)}
        official = {0x1000,0x1001,*range(0x1066,0x106a),*range(0x1100,0x1108),*range(0x1200,0x1208)}
        self.assertEqual(len(ids), 4)
        self.assertFalse(set(ids.values()) & official)
        self.assertTrue(all(0 < i < 0xC000 for i in ids.values()))
        self.assertEqual(len(set(ids.values())),4)
        self.assertEqual(ids['CL_TAG_LF_FILE'], ids['CL_TAG_HF_FILE']+1)

    def test_import_commands_do_not_reuse_official_slot_enable_or_nickname(self):
        header = (ROOT / 'firmware/application/app/data_cmd.h').read_text()
        ids = {name:int(value) for name,value in re.findall(r'#define\s+(DATA_CMD_\w+)\s+\((\d+)\)',header)}
        self.assertEqual(ids['DATA_CMD_ENTER_BOOTLOADER'],1010)
        for name in ['DATA_CMD_SET_EM410X_DATA','DATA_CMD_SET_MF1_DATA','DATA_CMD_GET_SLOT_INFO']:
            self.assertNotIn(ids[name], [1006,1007,1008])
        self.assertEqual(len(ids),len(set(ids.values())))


if __name__ == '__main__': unittest.main()
