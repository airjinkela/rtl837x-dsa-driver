#!/usr/bin/env python3
"""Compile actual patched driver functions against a fault-injection shim.

Usage: python3 test_stats.py /path/to/patched/rtl837x-driver /tmp/test-output
These are host-side unit tests, not kernel or hardware integration tests.
"""
from pathlib import Path
import re
import subprocess
import sys

source = (Path(sys.argv[1]) / 'src/rtl8372n.c').read_text(encoding='utf-8')
out = Path(sys.argv[2])
out.mkdir(parents=True, exist_ok=True)

def block(anchor):
    start = source.index(anchor)
    opening = source.index('{', start)
    depth = 1
    for end in range(opening + 1, len(source)):
        depth += (source[end] == '{') - (source[end] == '}')
        if depth == 0:
            return source[start:end + 1]
    raise AssertionError(anchor)

enum = block('enum rtl8372n_mib_counter_index') + ';\n'
chip = block('struct rtl8372n {') + ';\n'
names = ['rtl8372n_get_mib_counter', 'rtl8372n_stats_from_mib',
         'rtl8372n_stats_update', 'rtl8372n_stats_poll', 'rtl8372n_get_stats64',
         'rtl8372n_stats_stop', 'rtl8372n_stats_release', 'rtl8372n_stats_init',
         'rtl8372n_stats_start', 'rtl8372n_teardown', 'rtl8372n_get_mac_stats']
functions = []
for name in names:
    m = re.search(r'^static (?:int|void) ' + name + r'\(', source, re.M)
    assert m, name
    functions.append(block(m.group()))
get_stats = functions[names.index('rtl8372n_get_stats64')]
assert not any(x in get_stats for x in ['mutex_', 'regmap_', 'get_mib_counter', 'schedule_', 'stats_update'])
mac_fields = sorted(set(re.findall(r'mac_stats->(\w+)', functions[-1])))

shim = r'''
#include <assert.h>
#include <stdbool.h>
#include <stdint.h>
#include <stddef.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <errno.h>
#include <linux/if_link.h>
typedef uint64_t u64;
typedef uint32_t u32;
typedef uint64_t netdev_features_t;
typedef int spinlock_t;
#define RTL8372N_NUM_PORTS 9
#define RTL8372N_VLAN_MAX 4095
#define HZ 100
#define RTL8372N_STATS_INTERVAL HZ
#define ETH_FCS_LEN 4
#define ETH_HLEN 14
#define BIT(n) (1UL << (n))
#define ARRAY_SIZE(a) (sizeof(a) / sizeof((a)[0]))
#define container_of(p,t,m) ((t *)((char *)(p) - offsetof(t,m)))
#define READ_ONCE(x) (x)
#define WRITE_ONCE(x,v) ((x) = (v))
static int mutex_held, spin_held, in_callback, schedules, cancels, action_fail;
static void (*cleanup_action)(void *);
static void *cleanup_data;
struct work_struct { void (*fn)(struct work_struct *); };
struct delayed_work { struct work_struct work; bool pending; };
#define to_delayed_work(p) container_of(p, struct delayed_work, work)
#define INIT_DELAYED_WORK(w,f) do { (w)->work.fn=(f); (w)->pending=false; } while (0)
#define spin_lock_init(p) (*(p)=0)
#define spin_lock_irqsave(p,f) do { (void)(p); (f)=0; assert(!spin_held && !mutex_held); spin_held=1; } while (0)
#define spin_unlock_irqrestore(p,f) do { (void)(p); (void)(f); assert(spin_held); spin_held=0; } while (0)
static void mutex_lock(int *p) { (void)p; assert(!mutex_held && !spin_held && !in_callback); mutex_held=1; }
static void mutex_unlock(int *p) { (void)p; assert(mutex_held); mutex_held=0; }
static void schedule_delayed_work(struct delayed_work *w, unsigned long delay) { (void)delay; assert(!in_callback); w->pending=true; schedules++; }
static void cancel_delayed_work_sync(struct delayed_work *w) { assert(!in_callback); w->pending=false; cancels++; }
static int devm_add_action_or_reset(void *dev, void (*action)(void *), void *data) {
 (void)dev;
 if (action_fail) { action(data); return -ENOMEM; }
 cleanup_action=action; cleanup_data=data; return 0;
}
#define dev_warn_ratelimited(...) ((void)0)
struct rtl837x_priv;
struct rtl837x_mib_counter { unsigned int offset, length; const char *name; };
struct rtl837x_ops { int (*get_mib_counter)(struct rtl837x_priv *, int, const struct rtl837x_mib_counter *, u64 *); };
struct rtl837x_priv { void *chip_data, *dev, *map; int mib_lock; const struct rtl837x_ops *ops; const struct rtl837x_mib_counter *mib_counters; };
struct dsa_port { int index; };
struct dsa_switch { void *priv; unsigned long user_mask; struct dsa_port ports[RTL8372N_NUM_PORTS]; };
#define dsa_switch_for_each_user_port(dp,ds) for (int _i=0; _i<RTL8372N_NUM_PORTS; _i++) if (((ds)->user_mask & BIT(_i)) && (((dp)=&(ds)->ports[_i]),1))
struct rtl8372n_pcs { int unused; };
#define RTL837X_INDIRECT_ACCESS_CTRL_PORT_ID_MASK 0x1e
#define RTL837X_INDIRECT_ACCESS_CTRL_MIB_ID_MASK 0x7e0
#define RTL837X_INDIRECT_ACCESS_CTRL_ACC_CMD_MASK 1
#define RTL837X_INDIRECT_ACCESS_CTRL_ADDR 0
#define RTL837X_INDIRECT_ACCESS_CNT_L_ADDR 1
#define RTL837X_INDIRECT_ACCESS_CNT_H_ADDR 2
#define FIELD_PREP(mask,val) ((mask) & (val))
static int io_step, io_fail;
static int io_next(void) { assert(!in_callback && !spin_held); return io_step++==io_fail ? -EIO : 0; }
static int rtl837x_reg_write(struct rtl837x_priv *p, int reg, u32 val) { (void)p;(void)reg;(void)val;return io_next(); }
static int rtl837x_reg_read(struct rtl837x_priv *p, int reg, u32 *val) {
 (void)p; int ret=io_next(); if (ret) return ret;
 *val=reg==RTL837X_INDIRECT_ACCESS_CNT_L_ADDR ? 0x12345678U : 0x9abcdef0U; return 0;
}
#define regmap_read_poll_timeout(...) io_next()
'''

mock = r'''
static struct rtl8372n chip;
static struct rtl837x_priv priv;
static struct dsa_switch ds;
static struct rtl837x_mib_counter mib[RTL837X_MIB_END];
static u64 counters[RTL8372N_NUM_PORTS][RTL837X_MIB_END];
static int fail_at, calls, fail_port;
static int mib_read(struct rtl837x_priv *p,int port,const struct rtl837x_mib_counter *c,u64 *value) {
 (void)p; assert(mutex_held && !spin_held && !in_callback);
 if (calls++==fail_at || port==fail_port) return -EIO;
 *value=counters[port][c->offset];return 0;
}
static const struct rtl837x_ops ops={.get_mib_counter=mib_read};
static void init_fixture(void) {
 memset(&chip,0,sizeof(chip));memset(&priv,0,sizeof(priv));memset(&ds,0,sizeof(ds));memset(counters,0,sizeof(counters));
 mutex_held=spin_held=in_callback=schedules=cancels=action_fail=0;
 fail_at=fail_port=io_fail=-1;calls=io_step=0;cleanup_action=NULL;cleanup_data=NULL;
 priv.chip_data=&chip;priv.ops=&ops;priv.mib_counters=mib;ds.priv=&priv;
 for (unsigned int i=0;i<RTL837X_MIB_END;i++) mib[i].offset=i;
 for (int i=0;i<RTL8372N_NUM_PORTS;i++) ds.ports[i].index=i;
}
static void fill_port(int p) {
 counters[p][RTL837X_MIB_ifInOctets]=64000;
 counters[p][RTL837X_MIB_ifOutOctets]=128000;
 counters[p][RTL837X_MIB_ifInUcastPkts]=900;
 counters[p][RTL837X_MIB_ifInMulticastPkts]=80;
 counters[p][RTL837X_MIB_ifInBroadcastPkts]=20;
 counters[p][RTL837X_MIB_ifOutUcastPkts]=1800;
 counters[p][RTL837X_MIB_ifOutMulticastPkts]=160;
 counters[p][RTL837X_MIB_ifOutBroadcastPkts]=40;
 counters[p][RTL837X_MIB_RxGoodCnt]=1000;
 counters[p][RTL837X_MIB_TxGoodCnt]=2000;
}
'''

tests = r'''
int main(void) {
 struct rtnl_link_stats64 saved, result;
 struct ethtool_eth_mac_stats mac, before;
 struct rtl837x_mib_counter c={.offset=0,.length=2};
 u64 value;
 int count=0;
 init_fixture();
 for (int i=0;i<4;i++) {
  value=0xfedcba9876543210ULL;io_fail=i;io_step=0;
  assert(rtl8372n_get_mib_counter(&priv,4,&c,&value)==-EIO);
  assert(value==0xfedcba9876543210ULL);count++;
 }
 io_fail=-1;io_step=0;
 assert(!rtl8372n_get_mib_counter(&priv,4,&c,&value));
 assert(value==0x123456789abcdef0ULL);count++;
 for (int odd=0;odd<2;odd++) {
  c.length=1;c.offset=(unsigned int)odd;io_fail=2;io_step=0;value=42;
  assert(rtl8372n_get_mib_counter(&priv,4,&c,&value)==-EIO && value==42);count++;
  io_fail=-1;io_step=0;
  assert(!rtl8372n_get_mib_counter(&priv,4,&c,&value));
  assert(value==(odd ? 0x9abcdef0ULL : 0x12345678ULL));count++;
 }
 init_fixture();fill_port(4);
 assert(!rtl8372n_stats_update(&priv,4));
 assert(chip.stats[4].rx_packets==1000 && chip.stats[4].tx_packets==2000);
 assert(chip.stats[4].rx_bytes==60000 && chip.stats[4].tx_bytes==120000);
 assert(chip.stats[4].multicast==80);count++;
 saved=chip.stats[4];
 for (int i=0;i<8;i++) {
  fail_at=i;calls=0;assert(rtl8372n_stats_update(&priv,4)==-EIO);
  assert(!memcmp(&saved,&chip.stats[4],sizeof(saved)));count++;
 }
 fail_at=-1;calls=0;
 assert(!rtl8372n_stats_update(&priv,4));
 assert(!memcmp(&saved,&chip.stats[4],sizeof(saved)));count++;
 counters[4][RTL837X_MIB_ifInOctets]=0;
 assert(rtl8372n_stats_update(&priv,4)==-ERANGE);
 assert(!memcmp(&saved,&chip.stats[4],sizeof(saved)));count++;
 fill_port(4);counters[4][RTL837X_MIB_ifOutOctets]=0;
 assert(rtl8372n_stats_update(&priv,4)==-ERANGE);
 assert(!memcmp(&saved,&chip.stats[4],sizeof(saved)));count++;
 fill_port(4);counters[4][RTL837X_MIB_ifInOctets]=(1ULL<<40)+64000;
 assert(!rtl8372n_stats_update(&priv,4));
 assert(chip.stats[4].rx_bytes==(1ULL<<40)+60000);count++;
 saved=chip.stats[4];calls=0;in_callback=1;
 rtl8372n_get_stats64(&ds,4,&result);in_callback=0;
 assert(!memcmp(&saved,&result,sizeof(saved)) && calls==0);count++;
 memset(&result,0xa5,sizeof(result));saved=result;
 rtl8372n_get_stats64(&ds,-1,&result);rtl8372n_get_stats64(&ds,9,&result);
 assert(!memcmp(&result,&saved,sizeof(saved)));count++;
 init_fixture();
 rtl8372n_stats_stop(&priv);assert(cancels==0);count++;
 ds.user_mask=BIT(4)|BIT(7)|BIT(8);
 assert(!rtl8372n_stats_init(&ds));
 assert(chip.stats_port_mask==ds.user_mask && chip.stats_initialized && !chip.stats_running);count++;
 fill_port(4);fill_port(7);fill_port(8);
 rtl8372n_stats_start(&priv);assert(schedules==1 && chip.stats_running);count++;
 chip.stats_work.pending=false;
 fail_port=7;rtl8372n_stats_poll(&chip.stats_work.work);
 assert(chip.stats[4].rx_packets==1000 && chip.stats[7].rx_packets==0 && chip.stats[8].rx_packets==1000);
 assert(chip.stats[3].rx_packets==0 && schedules==2);count++;
 fail_port=-1;rtl8372n_stats_poll(&chip.stats_work.work);
 assert(chip.stats[7].rx_packets==1000 && chip.stats[4].rx_packets==1000);count++;
 rtl8372n_teardown(&ds);assert(!chip.stats_running && !chip.stats_work.pending && cancels==1);
 calls=0;int n=schedules;rtl8372n_stats_poll(&chip.stats_work.work);
 assert(calls==0 && schedules==n);count++;
 assert(cleanup_action);cleanup_action(cleanup_data);assert(cancels==2);count++;
 init_fixture();action_fail=1;ds.user_mask=BIT(4);
 assert(rtl8372n_stats_init(&ds)==-ENOMEM && !chip.stats_running && cancels==1);count++;
 init_fixture();fill_port(4);
 for (int i=0;i<15;i++) {
  memset(&mac,0xa5,sizeof(mac));before=mac;fail_at=i;calls=0;
  rtl8372n_get_mac_stats(&ds,4,&mac);
  assert(!memcmp(&before,&mac,sizeof(mac)));count++;
 }
 fail_at=-1;calls=0;memset(&mac,0xa5,sizeof(mac));
 rtl8372n_get_mac_stats(&ds,4,&mac);
 assert(mac.FramesReceivedOK==1000 && mac.OctetsReceivedOK==46000);count++;
 memset(&mac,0xa5,sizeof(mac));before=mac;counters[4][RTL837X_MIB_ifInOctets]=0;
 rtl8372n_get_mac_stats(&ds,4,&mac);assert(!memcmp(&before,&mac,sizeof(mac)));count++;
 assert(!mutex_held && !spin_held);
 printf("PASS: %d actual-C-function cases: MIB failures, 64-bit values, FCS conversion, cache retention, no double counting, atomic reads, port mask and lifecycle\n",count);
 return 0;
}
'''

c = shim + enum + chip
c += 'struct ethtool_eth_mac_stats {\n' + ''.join(' u64 ' + field + ';\n' for field in mac_fields) + '};\n'
c += mock + '\n\n'.join(functions) + tests
path = out / 'test_stats.c'
path.write_text(c, encoding='utf-8')
binary = out / 'test_stats'
subprocess.run(['cc', '-std=gnu11', '-O2', '-Wall', '-Wextra', '-Werror', str(path), '-o', str(binary)], check=True)
subprocess.run([str(binary)], check=True)
print('PASS: get_stats64 contains no sleeping hardware/worker calls (static check)')
