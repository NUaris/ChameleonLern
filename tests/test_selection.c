/* SPDX-License-Identifier: GPL-3.0-only */
#include "selection_core.h"
#include <assert.h>
#include <stdio.h>
#include <string.h>

static sel_context_t environment(uint32_t key) {
    sel_context_t c = {0}; c.beacon_count = 1; c.beacons[0].key = key; c.beacons[0].rssi = -55;
    return c;
}
int main(void) {
    sel_model_t m; sel_model_init(&m); assert(sel_config_valid(&m.config)); assert(m.config.mode == SEL_OFF);
    sel_context_t c = environment(123);
    assert(!sel_learn(&m, &c, 8)); assert(sel_learn(&m, &c, 2));
    assert(sel_predict(&m, &c, 255).reason == SEL_REASON_SUPPORT);
    assert(sel_learn(&m, &c, 2)); assert(sel_sample_count(&m) == 1);
    sel_prediction_t p = sel_predict(&m, &c, 255); assert(p.slot == 2 && p.score == 100 && p.reason == SEL_REASON_READY);
    assert(sel_predict(&m, &c, 1).slot == SEL_NONE); /* Exclude disabled/missing slots. */
    assert(sel_learn(&m, &c, 3)); assert(sel_learn(&m, &c, 3));
    assert(sel_predict(&m, &c, 255).reason == SEL_REASON_AMBIGUOUS);
    sel_forget(&m, 3); assert(sel_sample_count(&m) == 1);
    sel_context_t wrong = environment(456); assert(sel_predict(&m, &wrong, 255).slot == SEL_NONE);
    wrong = c; wrong.beacons[0].rssi = -95;
    assert(sel_predict(&m, &wrong, 255).slot == SEL_NONE); /* Weak BLE evidence is rejected. */
    sel_context_t time_only = {.time_valid=1, .minute=600, .weekday=1};
    assert(!sel_learn(&m, &time_only, 0)); assert(sel_predict(&m, &time_only, 255).reason == SEL_REASON_NO_CONTEXT);
    sel_forget(&m, SEL_NONE);
    c = (sel_context_t){.field=2, .reader_count=1, .reader={1u<<11}};
    assert(sel_learn(&m, &c, 1)); assert(sel_predict(&m, &c, 255).slot == SEL_NONE); /* Generic REQA is insufficient. */
    c.reader[0] = (4u<<11) | (3u<<3); assert(sel_learn(&m, &c, 1)); assert(sel_learn(&m, &c, 1));
    assert(sel_predict(&m, &c, 255).reason == SEL_REASON_READY);
    c.reader[0] |= 1; assert(sel_predict(&m, &c, 255).score == 85); /* Timing tolerance. */
    uint8_t wire[SEL_MODEL_BYTES], wire2[SEL_MODEL_BYTES]; sel_model_t restored;
    assert(sel_model_encode(&m, wire, sizeof(wire))); sel_model_init(&restored);
    assert(sel_model_decode(&restored, wire, sizeof(wire)));
    assert(sel_model_encode(&restored, wire2, sizeof(wire2))); assert(!memcmp(wire, wire2, sizeof(wire)));
    sel_model_t before = restored;
    for (unsigned i = 0; i < sizeof(wire); i++) {
        wire[i] ^= 1; assert(!sel_model_decode(&restored, wire, sizeof(wire))); wire[i] ^= 1;
        assert(!memcmp(&before, &restored, sizeof(before)));
    }
    assert(!sel_model_decode(&restored, wire, sizeof(wire)-1));
    sel_forget(&m, SEL_NONE);
    for (unsigned i = 0; i < 100; i++) { c = environment(i+1); assert(sel_learn(&m, &c, i%8)); }
    assert(sel_sample_count(&m) == SEL_SAMPLES);
    c = environment(100); assert(sel_predict(&m, &c, 255).slot == 3);
    c.beacon_count = 7; assert(!sel_context_valid(&c));
    c = environment(10); c.beacon_count = 2; c.beacons[1] = c.beacons[0]; assert(!sel_context_valid(&c));
    sel_clock_t clock = {0};
    assert(!sel_clock_sync(&clock, 0, 480, 0)); assert(!sel_clock_sync(&clock, 1704067200, 900, 0));
    assert(sel_clock_sync(&clock, 1704067200, 480, UINT32_MAX-499)); /* 2024-01-01 Mon UTC. */
    sel_clock_update(&clock, 500); assert(clock.utc == 1704067201 && clock.remainder_ms == 0);
    sel_clock_context(&clock, &c); assert(c.weekday == 0 && c.minute == 480);
    assert(sel_clock_sync(&clock, 1704067200, -60, 0)); sel_clock_context(&clock, &c); assert(c.weekday == 6 && c.minute == 1380);
    uint8_t addr[6]={1,2,3,4,5,0x40}, other[6]={6,7,8,9,10,0x40};
    uint8_t ad[]={26,0xff,0x4c,0,2,0x15,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,16,0,1,0,2,0xc5};
    uint32_t id = sel_ble_identity(addr,1,ad,sizeof(ad)); assert(id);
    ad[26]=99; assert(sel_ble_identity(other,1,ad,sizeof(ad)) == id); /* Private address and changing payload ignored. */
    assert(sel_ble_identity(addr,0,NULL,0) != sel_ble_identity(other,0,NULL,0));
    ad[0]=30; assert(!sel_ble_identity(addr,1,ad,sizeof(ad)));
    assert(!sel_ble_identity(addr,1,NULL,0));
    for (unsigned i=0;i<10000;i++) { uint8_t random[31]; for(unsigned j=0;j<31;j++) random[j]=(uint8_t)(i*13+j*71); (void)sel_ble_identity(addr,1,random,i%32); }
    puts("selection: matching, ambiguity, evidence, feedback, capacity, persistence corruption, clock and BLE identity passed");
}
