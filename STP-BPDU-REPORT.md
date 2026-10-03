# BPDU trap revision: scope, diagnosis and review

## Maintainer feedback

[airjinkela's review](https://github.com/airjinkela/rtl837x-dsa-driver/pull/2#issuecomment-5963940344)
reports that the original trap proposal had already been tried and did not fully
resolve the issue. With `mxl862xx-8021q`, trapped frames go directly to the CPU,
bypass S-VLAN processing and arrive without the tag needed to identify the
source port. The linked experiment is
[`rma_stp` at `d120e278ed7e48f90d0199185a933215d4e8f5af`](https://github.com/airjinkela/rtl837x-dsa-driver/tree/d120e278ed7e48f90d0199185a933215d4e8f5af).
This is a reported hardware result, not a test performed for this revision.

The source confirms why the original proposal was incomplete:

- `rtl8372n_teardown_tag_rtl()` disables native CPU tagging before VLAN tagging
  is installed. A trap that bypasses S-VLAN consequently has neither transport
  tag in that mode.
- OpenWrt's `mxl862xx-8021q` receiver uses the VLAN tag to select a DSA user
  interface. It has no RTL8372N native-trap decoder.
- Linux's `rtl8_4` receiver decodes a native tag, including the source port and
  forwarding reason. A completely untagged BPDU has no equivalent source-port
  field; selecting an arbitrary bridge port would misdeliver it.

## Changes in this revision

| Area | Change |
| --- | --- |
| Native `rtl8_4` | Enable the BPDU trap only after external CPU selection, native CPU tag enable and CPU tag awareness have succeeded |
| Trap destination | Select the external CPU in the **shared RMA/PTP** destination field; the existing native-tag setup selects the physical CPU port |
| BPDU format | Set action TRAP and clear BPDU CKEEP to permit native tag insertion; preserve its storm, VLAN-leak and isolation-leak fields |
| VLAN transport | Restore the saved BPDU action/CKEEP and shared CPU-selection fields before native CPU tagging is disabled; retain the existing VLAN mode with an explicit unresolved-STP warning |
| Return to native | Reapply the native trap after the native tag is ready |
| Error handling | Check new reads/writes, preserve the first enable error, attempt restoration after an enable-write error, and report restoration failure; reject teardown if restoration fails |

The saved fields are the values observed before this revision first enables its
trap. They are **not** a proven safe STP policy for VLAN mode. Other RMA action
entries and the trap priority are unchanged. Because CPU selection is shared,
PTP and other RMA traps must be included in hardware regression checks.

Expected masked values while native trapping is active:

| Register | Mask | Requested value |
| --- | --- | --- |
| BPDU RMA control `0x4ecc` | action/CKEEP `0x34` | `0x10` |
| RMA/PTP trap destination `0x4f34` | CPU mask `0x00030000` | external CPU `0x00020000` |
| CPU tag control `0x6720` | external insert mode `0x00000c00` | all CPU frames `0x00000000` |
| CPU tag control `0x6720` | external tag enable `0x2` | enabled `0x2` |

These are requested encodings from this repository's definitions, not recorded
RTL8372N readbacks. The generated Realtek register header remains an existing
dependency of this repository; this patch makes no licensing-clearance claim
and is not imported into the separate GL-BE9300 candidate.

## What this does not solve

`mxl862xx-8021q` STP remains open. This revision removes the unconditional trap
introduced by the first proposal when changing to VLAN mode; it does not claim
that the restored original policy prevents BPDU flooding. There is no synthetic
source-port assignment, global change to the MaxLinear tagger, or new hybrid
protocol in this patch. Keep the PR as Draft until the native path is tested.

For a complete VLAN-mode solution, evaluate these alternatives:

1. **Native tags on trapped frames only, plus an RTL8372N hybrid tagger.**
   Keep normal data on S-VLAN and configure native tag insertion for traps.
   Capture actual trap bytes first: EtherType/protocol, tag placement, source
   port, reason, C-tag preservation and whether an S-tag is also present. The
   receiver needs strict validation of both formats and correct trap forwarding
   marks; transmit and blocked-port BPDU delivery also need testing. This needs
   a distinct protocol and kernel/package integration, not a change that teaches
   every MaxLinear switch to accept arbitrary Realtek frames.
2. **Forward through the VLAN path to CPU only.** RTLPlayground documents an
   RMA FORWARD + per-VLAN static L2 multicast entry for its embedded CPU. Verify
   that the external CPU is a supported destination and that blocked/listening
   ports still deliver BPDUs, with correct S-VLAN source-port metadata and no
   user-port replication. Its embedded-CPU result does not establish this for
   the external-CPU topology. Entry lifetime, VID/FID scope and flush behavior
   must be defined before implementing that route.

## Build and static review

- `git diff --check`: PASS for the source revision.
- Strict Linux `checkpatch.pl` on the source change: PASS, zero errors, warnings
  or checks.
- Exact-source ARM64 module/API build: PENDING at preparation.
- Complete OpenWrt package/image build: NOT RUN.
- RTL8372N hardware, packet captures and failure injection: NOT RUN.

A module/API build cannot prove tag generation, BPDU delivery or loop safety.

## Requested maintainer tests

Use an isolated bench with a recovery path. Establish correct BPDU delivery
without a physical loop before testing redundant links. Record PASS, FAIL,
BLOCKED or NOT RUN per row, exact source/kernel/image identifiers, chip
revision, CPU-port index and complete logs.

| Check | Evidence required |
| --- | --- |
| Native receive | Inject a known BPDU on each available user port; capture CPU-conduit and DSA-user frames, including native tag bytes, source port and reason; confirm bridge processing |
| Native suppression | Distinguish the injected BPDU from bridge-generated BPDUs using the source MAC/payload; no replica of that injected frame on other user ports |
| STP states | Receive and transmit BPDUs while ports are blocking/listening, learning and forwarding; ordinary data remains blocked as appropriate |
| Control/data regression | LLDP, other enabled RMA/PTP classes, VLAN-tagged data, standalone isolation and ordinary bridge traffic; check for unintended shared-destination changes |
| Mode changes | Native → VLAN → native with interfaces down; compare the saved fields, restored original fields, native tagging and VLAN data. A VLAN data pass is not an STP pass |
| Failure paths | With an agreed mechanism, fail either snapshot read, either enable write, and either restore write; record first error, cleanup error and actual tag/RMA state. Existing mode-change recovery also needs review |
| Reset | Warm reboot and full power cycle; repeat tag/RMA readbacks and per-port BPDU delivery |
| Redundant links | Only after the preceding native-path checks pass, test STP convergence and bounded BPDU/counter rates on two switches |

Please also supply captures from the failing VLAN-mode trap experiment with
native insert mode set to TRAPPING (`1`) and native tagging enabled, if that
configuration can be tested safely. That determines whether the hybrid path is
implementable with the existing hardware controls. Such captures would support
a subsequent patch; the current revision does not enable that experiment.
