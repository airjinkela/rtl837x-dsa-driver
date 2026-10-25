# Cached hardware port statistics

DSA software counters miss traffic forwarded without traversing the CPU.
Report the configured user ports' MIB counters through get_stats64 using
the asynchronous polling/cache design used by Linux rtl8365mb.

One chip-level delayed work samples eight counters for each user port,
approximately once per second. Sleeping MDIO access uses the existing MIB
mutex; publication and get_stats64 use only a short IRQ-safe spinlock.
Failed/inconsistent samples preserve the previous snapshot. Teardown,
remove, shutdown and device-managed cleanup stop the worker.

The initial implementation reports bytes, packets and multicast. It does
not guess error/drop MIB mappings; hardware errors remain inspectable via
ethtool, and zero standard error fields do not prove an error-free link.
Standard bytes retain Ethernet headers and subtract four FCS bytes per
frame, consistent with the existing driver's MAC-octet conversion; this
requires hardware validation. Counters are snapshots, not repeatedly
added deltas, and may lag by the polling interval.

This does not fix bridge, VLAN or PPPoE upper-device accounting and does
not change tagging, forwarding, PHY setup, EEPROM or firmware settings.

## Validation

Run on a Linux host with cc and Python 3:

    python3 tests/test_stats.py . /tmp/rtl837x-stats-test

The harness extracts the actual C functions and tests 49 cases including
read failures, unchanged outputs/cache, 64-bit values, FCS arithmetic,
no double counting, user-port masking and mocked lifecycle handling.
It is not a real IRQ/RCU/workqueue concurrency or hardware test.

The same implementation previously compiled and linked as an external
module against a MediaTek Linux 6.12.108 tree, with an existing local
phylink API compatibility patch applied separately. That compatibility
patch is not included here. Host tests also passed ASan/UBSan previously.
No complete firmware or on-device validation has been performed for
these commits. Please validate counter semantics, polling overhead,
link cycling and driver lifetime on supported hardware before merging.
