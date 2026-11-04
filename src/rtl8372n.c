// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2025 StarField Xu <air_jinkela@163.com>
 */
#include <linux/version.h>
#include <linux/regmap.h>
#include <linux/if_bridge.h>
#include <linux/if_vlan.h>
#include <linux/of_net.h>
#include <linux/spinlock.h>
#include <linux/workqueue.h>

#include "rtl837x.h"
#include "rtl837x_l2.h"
#include "rtl837x_vlan.h"
#include "rtl837x_serdes.h"

#define RTL837X_NUM_PORTS 9
#define RTL837X_LUT_MAX 4160

#define RTL8372N_SUPPORTED_BRIDGE_FLAGS \
	    (BR_LEARNING | BR_FLOOD | BR_MCAST_FLOOD | BR_BCAST_FLOOD)

/*
 * Chip identification. The model is a 16-bit number followed by two packed
 * 5-bit characters, e.g. "8372" + 'N'.
 */
#define RTL837X_CHIP_ID_REG                  0x4
#define   RTL837X_CHIP_ID_MODEL_MASK         GENMASK(31, 16)
#define   RTL837X_CHIP_ID_CHAR1_MASK         GENMASK(15, 11)
#define   RTL837X_CHIP_ID_CHAR2_MASK         GENMASK(10, 6)

/* Ports whose link status is polled by the switch itself */
#define RTL837X_PORT_POLLING_REG             0x6334
#define   RTL837X_PORT_POLLING_PORT_BIT(_p)  BIT(_p) /* port 0~8 */

/* Selects whether a MAC polls an internal PHY or an internal SerDes */
#define RTL837X_SMI_MAC_TYPE_REG             0x6330
#define   RTL837X_SMI_MAC_TYPE_MASK(_p)      (0x3 << ((_p) * 2))
#define   RTL837X_SMI_MAC_TYPE_SERDES        0
#define   RTL837X_SMI_MAC_TYPE_PORT          1

/*
 * MAC force mode, one register per port. With the force link bits set the
 * MAC uses the forced speed/duplex/link of this register instead of the
 * state polled from its PHY or SerDes.
 */
#define RTL837X_MAC_FORCE_MODE_REG(_p)       (0x6344 + ((_p) << 2))
#define   RTL837X_MAC_FORCE_MODE_FORCE_LINK_MASK GENMASK(1, 0)

/* Global SMI settings */
#define RTL837X_SMI_GLB_CTRL_REG                    0x632C
#define   RTL837X_SMI_GLB_CTRL_POLLING_MASK         GENMASK(20, 12)
#define   RTL837X_SMI_GLB_CTRL_POLLING_PORT_BIT(_p)  (BIT(_p) << 12)
#define RTL837X_SMI_CTRL_REG                         0x6454
#define   RTL837X_SMI_CTRL_SMI2_MDC_EN_MASK          BIT(14)
#define   RTL837X_SMI_CTRL_SMI1_MDC_EN_MASK          BIT(13)
#define   RTL837X_SMI_CTRL_SMI0_MDC_EN_MASK          BIT(12)

/* Internal PHY configuration */
#define RTL837X_PHY_MDI_REVERSE_REG                       0xA90
#define   RTL837X_PHY_MDI_REVERSE_P3_MASK                 BIT(3)
#define   RTL837X_PHY_MDI_REVERSE_P2_MASK                 BIT(2)
#define   RTL837X_PHY_MDI_REVERSE_P1_MASK                 BIT(1)
#define   RTL837X_PHY_MDI_REVERSE_P0_MASK                 BIT(0)
#define RTL837X_PHY_TX_POLARITY_REG                       0xA94
#define   RTL837X_PHY_TX_POLARITY_P3_MASK                 GENMASK(15, 12)
#define   RTL837X_PHY_TX_POLARITY_P2_MASK                 GENMASK(11, 8)
#define   RTL837X_PHY_TX_POLARITY_P1_MASK                 GENMASK(7, 4)
#define   RTL837X_PHY_TX_POLARITY_P0_MASK                 GENMASK(3, 0)

/* MDIO address the switch uses to reach the PHY of each external port */
#define RTL837X_SMI_PORT_MDIO_ADDR_REG(_p)                (0x644C + (((_p) / 6) << 2))
#define   RTL837X_SMI_PORT_MDIO_ADDR_MASK(_p)             (GENMASK(4, 0) << (((_p) % 6) * 5))

/*
 * MIB counters are read indirectly: pick a counter id and port, then read
 * the low and high words.
 */
#define RTL837X_MIB_CTRL_REG                     0xF60
#define   RTL837X_MIB_CTRL_ID_MASK               GENMASK(10, 5)
#define   RTL837X_MIB_CTRL_PORT_IDX_MASK         GENMASK(4, 1)
#define   RTL837X_MIB_CTRL_BUSY_MASK             BIT(0)
#define RTL837X_MIB_COUNTER_LO_REG               0xF64
#define RTL837X_MIB_COUNTER_HI_REG               0xF68

/* Per-port MAC control and aggregated link status registers */
#define RTL837X_MAC_PORT_CTRL_REG(_p)                   (0x122C + (((_p) << 8)))
#define   RTL837X_MAC_PORT_CTRL_BKPRES_EN_MASK          BIT(0)

/* Maximum frame length per port, two 14-bit fields set to the same value */
#define RTL837X_MAC_MAX_LEN_REG(_p)                     (0x1250 + (((_p) << 8)))
#define   RTL837X_MAC_MAX_LEN_LO_MASK                   GENMASK(13, 0)
#define   RTL837X_MAC_MAX_LEN_HI_MASK                   GENMASK(27, 14)
#define RTL837X_MAC_MAX_LEN_MAX                         0x3FFF

#define RTL837X_MAC_LINK_STS_REG                        0x63E8
#define RTL837X_MAC_LINK_SPD_STS_REG(_p)                (0x63F0 + (((_p) >> 3) << 2))
#define   RTL837X_MAC_LINK_SPD_STS_MASK(_p)             (GENMASK(3, 0) << (((_p) & 0x7) << 2))
#define RTL837X_MAC_LINK_DUP_STS_REG                    0x63F8
#define RTL837X_MAC_TX_PAUSE_STS_REG                    0x63FC
#define RTL837X_MAC_RX_PAUSE_STS_REG                    0x6400

/* Energy Efficient Ethernet control and status */
#define RTL837X_EEE_CTRL_REG(_p)                        (0x125C + (((_p) << 8)))
#define   RTL837X_EEE_CTRL_RX_STS_MASK                  BIT(3)
#define   RTL837X_EEE_CTRL_TX_STS_MASK                  BIT(2)
#define   RTL837X_EEE_CTRL_TX_EN_MASK                   BIT(1)
#define   RTL837X_EEE_CTRL_RX_EN_MASK                   BIT(0)

/* L2 forwarding and learning control */
#define RTL837X_MAC_L2_GLOBAL_CTRL_REG                  0x5FD4
#define   RTL837X_MAC_L2_GLOBAL_CTRL_FWD_INVLD_MAC_EN_MASK   BIT(20)
#define   RTL837X_MAC_L2_GLOBAL_CTRL_FWD_UNKN_OPCODE_EN_MASK BIT(19)
#define RTL837X_MAC_L2_PORT_CTRL_REG(_p)                (0x1238 + (((_p) << 8)))
#define   RTL837X_MAC_L2_PORT_CTRL_CLOCK_SWITCH_MASK    BIT(8)
#define   RTL837X_MAC_L2_PORT_CTRL_RX_CHK_CRC_EN_MASK   BIT(4)

/*
 * CPU port tagging. The switch inserts a Realtek CPU tag on frames sent to
 * the CPU port, and only accepts tagged frames back when CPU_CTRL_EN is set.
 */
#define RTL837X_CPU_CTRL_REG                            0x6720
#define   RTL837X_CPU_CTRL_INSERTMODE_MASK              GENMASK(11, 10)
#define   RTL837X_CPU_CTRL_EN_MASK                      BIT(1)
#define RTL837X_EXT_CPU_CTRL_REG                        0x6724
#define   RTL837X_EXT_CPU_CTRL_PORT_IDX_MASK            GENMASK(3, 0)
/* Ports that expect the extended CPU tag on ingress */
#define RTL837X_CPU_TAG_AWARE_CTRL_REG                  0x603C

/* VLAN global control */
#define RTL837X_VLAN_CTRL_REG                                   0x4E14
#define   RTL837X_VLAN_CTRL_TABLE_RST_MASK                       BIT(3)
#define   RTL837X_VLAN_CTRL_CVLAN_FILTER_MASK                    BIT(2)

/*
 * VLAN stacking (SVLAN). The switch maps the customer VID of an ingress
 * frame to a service VID, and marks that service VID on the CPU tag.
 */
#define RTL837X_VS_GLB_CTRL_REG                                 0x6044
#define RTL837X_VS_UPLINK_PORT_REG                              0x57C0
#define RTL837X_VS_CTRL_REG                                     0x57C4
#define   RTL837X_VS_CTRL_SPRISEL_MASK                           GENMASK(4, 3)
#define   RTL837X_VS_CTRL_UIFSEG_MASK                            BIT(2)
#define   RTL837X_VS_CTRL_UNTAG_MASK                             GENMASK(1, 0)
#define RTL837X_VS_PORT_DFLT_SVID_REG(_p)	\
	(0x57CC + (((_p) >> 1) << 2))
#define   RTL837X_VS_PORT_DFLT_SVID_MASK(_p)	\
	(GENMASK(11, 0) << (((_p) & 0x1) * 12))
#define RTL837X_SVLAN_TRAP_CTRL_REG                             0x4EC8
/* Which CPU type the SVLAN trap is reported to: bit0 internal, bit1 external */
#define   RTL837X_SVLAN_TRAP_CTRL_CPU_TYPE_MASK                  GENMASK(17, 16)
/* Per-C2SVID entry mapping a customer VID to a service VID */
#define RTL837X_VLAN_C2S_ENTRY_REG(_i)                          (0x57E0 + (((_i) << 3)))

/* Spanning tree port state, indexed by filtering id (FID) */
#define RTL837X_MSTI_CTRL_REG(_fid)       (0x5310 + (((_fid) << 2)))
#define RTL837X_MSTI_CTRL_PORT_STATE(port, state) \
	((state) << ((port) * 2))
#define RTL837X_MSTI_CTRL_PORT_STATE_MASK(port) \
	RTL837X_MSTI_CTRL_PORT_STATE((port), GENMASK(1, 0))

/* Per-port forwarding mask; a clear bit blocks frames to that port */
#define RTL837X_PORT_ISOLATION_REG(_p)                    (0x50C0 + (((_p) << 2)))
#define   RTL837X_PORT_ISOLATION_PORT_MASK                 GENMASK(9, 0)

/* Flood port masks: bit n set = frames of that kind are flooded to port n */
#define RTL837X_FLOOD_UNKNOWN_UC_REG                      0x5360
#define RTL837X_FLOOD_UNKNOWN_L2MC_REG                    0x5364
#define RTL837X_FLOOD_UNKNOWN_IP4MC_REG                   0x5368
#define RTL837X_FLOOD_UNKNOWN_IP6MC_REG                   0x536C
#define RTL837X_FLOOD_BCAST_REG                           0x5370

/* Flow control: force link and pause state instead of using autoneg */
#define RTL837X_FC_PORT_ACT_CTRL_REG(_p)                      (0x7124 + (((_p) << 2)))
#define   RTL837X_FC_PORT_ACT_CTRL_ACT_MASK                   BIT(12)
#define   RTL837X_FC_PORT_ACT_CTRL_ALLOW_PAGE_CNT_MASK        GENMASK(11, 0)

/* Reconciliation sublayer: optionally suppress link fault propagation */
#define RTL837X_RS_LAYER_CONFIG_REG                          0xB7C
#define   RTL837X_RS_LAYER_CONFIG_RS_LINK_FAULT_INDI_OFF_MASK BIT(5)

/* TODO: this should check whether the port is in SerDes or port mode */
static inline bool rtl8372n_is_serdes_port(int port)
{
	return port == 3 || port == 8;
}

#define PORT_TO_SERDES_IDX(_p) ((_p) == 3 ? 0 : 1)

enum rtl8372n_mib_counter_index {
	RTL837X_MIB_ifInOctets,
	RTL837X_MIB_ifOutOctets,
	RTL837X_MIB_ifInUcastPkts,
	RTL837X_MIB_ifInMulticastPkts,
	RTL837X_MIB_ifInBroadcastPkts,
	RTL837X_MIB_ifOutUcastPkts,
	RTL837X_MIB_ifOutMulticastPkts,
	RTL837X_MIB_ifOutBroadcastPkts,

	RTL837X_MIB_ifOutDiscards,
	RTL837X_MIB_dot1dTpPortInDiscards,
	RTL837X_MIB_dot3StatsSingleCollisionFrames,
	RTL837X_MIB_dot3StatsMultipleCollisionFrames,
	RTL837X_MIB_dot3StatsDeferredTransmissions,
	RTL837X_MIB_dot3StatsLateCollisions,
	RTL837X_MIB_dot3StatsExcessiveCollisions,
	RTL837X_MIB_dot3StatsSymbolErrors,
	RTL837X_MIB_dot3ControlInUnknownOpcodes,
	RTL837X_MIB_dot3InPauseFrames,
	RTL837X_MIB_dot3OutPauseFrames,
	RTL837X_MIB_etherStatsDropEvents,
	RTL837X_MIB_tx_etherStatsBroadcastPkts,
	RTL837X_MIB_tx_etherStatsMulticastPkts,
	RTL837X_MIB_tx_etherStatsCRCAlignErrors,
	RTL837X_MIB_rx_etherStatsCRCAlignErrors,
	RTL837X_MIB_tx_etherStatsUndersizePkts,
	RTL837X_MIB_rx_etherStatsUndersizePkts,
	RTL837X_MIB_tx_etherStatsOversizePkts,
	RTL837X_MIB_rx_etherStatsOversizePkts,
	RTL837X_MIB_tx_etherStatsFragments,
	RTL837X_MIB_rx_etherStatsFragments,
	RTL837X_MIB_tx_etherStatsJabbers,
	RTL837X_MIB_rx_etherStatsJabbers,
	RTL837X_MIB_tx_etherStatsCollisions,
	RTL837X_MIB_tx_etherStatsPkts64Octets,
	RTL837X_MIB_rx_etherStatsPkts64Octets,
	RTL837X_MIB_tx_etherStatsPkts65to127Octets,
	RTL837X_MIB_rx_etherStatsPkts65to127Octets,
	RTL837X_MIB_tx_etherStatsPkts128to255Octets,
	RTL837X_MIB_rx_etherStatsPkts128to255Octets,
	RTL837X_MIB_tx_etherStatsPkts256to511Octets,
	RTL837X_MIB_rx_etherStatsPkts256to511Octets,
	RTL837X_MIB_tx_etherStatsPkts512to1023Octets,
	RTL837X_MIB_rx_etherStatsPkts512to1023Octets,
	RTL837X_MIB_tx_etherStatsPkts1024to1518Octets,
	RTL837X_MIB_rx_etherStatsPkts1024to1518Octets,

	RTL837X_MIB_rx_etherStatsUndersizedropPkts,
	RTL837X_MIB_tx_etherStatsPkts1519toMaxOctets,
	RTL837X_MIB_rx_etherStatsPkts1519toMaxOctets,
	RTL837X_MIB_tx_etherStatsPktsOverMaxOctets,
	RTL837X_MIB_rx_etherStatsPktsOverMaxOctets,
	RTL837X_MIB_tx_etherStatsPktsFlexibleOctetsSET1,
	RTL837X_MIB_rx_etherStatsPktsFlexibleOctetsSET1,
	RTL837X_MIB_tx_etherStatsPktsFlexibleOctetsCRCSET1,
	RTL837X_MIB_rx_etherStatsPktsFlexibleOctetsCRCSET1,
	RTL837X_MIB_tx_etherStatsPktsFlexibleOctetsSET0,
	RTL837X_MIB_rx_etherStatsPktsFlexibleOctetsSET0,
	RTL837X_MIB_tx_etherStatsPktsFlexibleOctetsCRSET0C,
	RTL837X_MIB_rx_etherStatsPktsFlexibleOctetsCRSET0C,
	RTL837X_MIB_lengthFieldError,
	RTL837X_MIB_falseCarrieimes,
	RTL837X_MIB_underSizeOctets,
	RTL837X_MIB_framingErrors,

	RTL837X_MIB_rxMacDiscards,
	RTL837X_MIB_rxMacIPGShortDropRT,

	RTL837X_MIB_dot1dTpLearnedEntryDiscards,
	RTL837X_MIB_egrQueue7DropPktRT,
	RTL837X_MIB_egrQueue6DropPktRT,
	RTL837X_MIB_egrQueue5DropPktRT,
	RTL837X_MIB_egrQueue4DropPktRT,
	RTL837X_MIB_egrQueue3DropPktRT,
	RTL837X_MIB_egrQueue2DropPktRT,
	RTL837X_MIB_egrQueue1DropPktRT,
	RTL837X_MIB_egrQueue0DropPktRT,
	RTL837X_MIB_egrQueue7OutPktRT,
	RTL837X_MIB_egrQueue6OutPktRT,
	RTL837X_MIB_egrQueue5OutPktRT,
	RTL837X_MIB_egrQueue4OutPktRT,
	RTL837X_MIB_egrQueue3OutPktRT,
	RTL837X_MIB_egrQueue2OutPktRT,
	RTL837X_MIB_egrQueue1OutPktRT,
	RTL837X_MIB_egrQueue0OutPktRT,

	RTL837X_MIB_TxGoodCnt,
	RTL837X_MIB_RxGoodCnt,

	RTL837X_MIB_RxErrorCnt,
	RTL837X_MIB_TxErrorCnt,

	RTL837X_MIB_TxGoodCnt_phy,
	RTL837X_MIB_RxGoodCnt_phy,

	RTL837X_MIB_RxErrorCnt_phy,
	RTL837X_MIB_TxErrorCnt_phy,
	RTL837X_MIB_END,
};

#define RTL837X_MAKE_MIB_COUNTER(_offset, _length, _name) \
		[RTL837X_MIB_ ## _name] = { _offset, _length, #_name }

static const struct rtl837x_mib_counter rtl8372n_mib_counters[] = {
	RTL837X_MAKE_MIB_COUNTER(0,  2, ifInOctets),
	RTL837X_MAKE_MIB_COUNTER(2,  2, ifOutOctets),
	RTL837X_MAKE_MIB_COUNTER(4,  2, ifInUcastPkts),
	RTL837X_MAKE_MIB_COUNTER(6,  2, ifInMulticastPkts),
	RTL837X_MAKE_MIB_COUNTER(8,  2, ifInBroadcastPkts),
	RTL837X_MAKE_MIB_COUNTER(10, 2, ifOutUcastPkts),
	RTL837X_MAKE_MIB_COUNTER(12, 2, ifOutMulticastPkts),
	RTL837X_MAKE_MIB_COUNTER(14, 2, ifOutBroadcastPkts),

	RTL837X_MAKE_MIB_COUNTER(16, 1, ifOutDiscards),
	RTL837X_MAKE_MIB_COUNTER(17, 1, dot1dTpPortInDiscards),
	RTL837X_MAKE_MIB_COUNTER(18, 1, dot3StatsSingleCollisionFrames),
	RTL837X_MAKE_MIB_COUNTER(19, 1, dot3StatsMultipleCollisionFrames),
	RTL837X_MAKE_MIB_COUNTER(20, 1, dot3StatsDeferredTransmissions),
	RTL837X_MAKE_MIB_COUNTER(21, 1, dot3StatsLateCollisions),
	RTL837X_MAKE_MIB_COUNTER(22, 1, dot3StatsExcessiveCollisions),
	RTL837X_MAKE_MIB_COUNTER(23, 1, dot3StatsSymbolErrors),
	RTL837X_MAKE_MIB_COUNTER(24, 1, dot3ControlInUnknownOpcodes),
	RTL837X_MAKE_MIB_COUNTER(25, 1, dot3InPauseFrames),
	RTL837X_MAKE_MIB_COUNTER(26, 1, dot3OutPauseFrames),
	RTL837X_MAKE_MIB_COUNTER(27, 1, etherStatsDropEvents),
	RTL837X_MAKE_MIB_COUNTER(28, 1, tx_etherStatsBroadcastPkts),
	RTL837X_MAKE_MIB_COUNTER(29, 1, tx_etherStatsMulticastPkts),
	RTL837X_MAKE_MIB_COUNTER(30, 1, tx_etherStatsCRCAlignErrors),
	RTL837X_MAKE_MIB_COUNTER(31, 1, rx_etherStatsCRCAlignErrors),
	RTL837X_MAKE_MIB_COUNTER(32, 1, tx_etherStatsUndersizePkts),
	RTL837X_MAKE_MIB_COUNTER(33, 1, rx_etherStatsUndersizePkts),
	RTL837X_MAKE_MIB_COUNTER(34, 1, tx_etherStatsOversizePkts),
	RTL837X_MAKE_MIB_COUNTER(35, 1, rx_etherStatsOversizePkts),
	RTL837X_MAKE_MIB_COUNTER(36, 1, tx_etherStatsFragments),
	RTL837X_MAKE_MIB_COUNTER(37, 1, rx_etherStatsFragments),
	RTL837X_MAKE_MIB_COUNTER(38, 1, tx_etherStatsJabbers),
	RTL837X_MAKE_MIB_COUNTER(39, 1, rx_etherStatsJabbers),
	RTL837X_MAKE_MIB_COUNTER(40, 1, tx_etherStatsCollisions),
	RTL837X_MAKE_MIB_COUNTER(41, 1, tx_etherStatsPkts64Octets),
	RTL837X_MAKE_MIB_COUNTER(42, 1, rx_etherStatsPkts64Octets),
	RTL837X_MAKE_MIB_COUNTER(43, 1, tx_etherStatsPkts65to127Octets),
	RTL837X_MAKE_MIB_COUNTER(44, 1, rx_etherStatsPkts65to127Octets),
	RTL837X_MAKE_MIB_COUNTER(45, 1, tx_etherStatsPkts128to255Octets),
	RTL837X_MAKE_MIB_COUNTER(46, 1, rx_etherStatsPkts128to255Octets),
	RTL837X_MAKE_MIB_COUNTER(47, 1, tx_etherStatsPkts256to511Octets),
	RTL837X_MAKE_MIB_COUNTER(48, 1, rx_etherStatsPkts256to511Octets),
	RTL837X_MAKE_MIB_COUNTER(49, 1, tx_etherStatsPkts512to1023Octets),
	RTL837X_MAKE_MIB_COUNTER(50, 1, rx_etherStatsPkts512to1023Octets),
	RTL837X_MAKE_MIB_COUNTER(51, 1, tx_etherStatsPkts1024to1518Octets),
	RTL837X_MAKE_MIB_COUNTER(52, 1, rx_etherStatsPkts1024to1518Octets),

	RTL837X_MAKE_MIB_COUNTER(54, 1, rx_etherStatsUndersizedropPkts),
	RTL837X_MAKE_MIB_COUNTER(55, 1, tx_etherStatsPkts1519toMaxOctets),
	RTL837X_MAKE_MIB_COUNTER(56, 1, rx_etherStatsPkts1519toMaxOctets),
	RTL837X_MAKE_MIB_COUNTER(57, 1, tx_etherStatsPktsOverMaxOctets),
	RTL837X_MAKE_MIB_COUNTER(58, 1, rx_etherStatsPktsOverMaxOctets),
	RTL837X_MAKE_MIB_COUNTER(59, 1, tx_etherStatsPktsFlexibleOctetsSET1),
	RTL837X_MAKE_MIB_COUNTER(60, 1, rx_etherStatsPktsFlexibleOctetsSET1),
	RTL837X_MAKE_MIB_COUNTER(61, 1, tx_etherStatsPktsFlexibleOctetsCRCSET1),
	RTL837X_MAKE_MIB_COUNTER(62, 1, rx_etherStatsPktsFlexibleOctetsCRCSET1),
	RTL837X_MAKE_MIB_COUNTER(63, 1, tx_etherStatsPktsFlexibleOctetsSET0),
	RTL837X_MAKE_MIB_COUNTER(64, 1, rx_etherStatsPktsFlexibleOctetsSET0),
	RTL837X_MAKE_MIB_COUNTER(65, 1, tx_etherStatsPktsFlexibleOctetsCRSET0C),
	RTL837X_MAKE_MIB_COUNTER(66, 1, rx_etherStatsPktsFlexibleOctetsCRSET0C),
	RTL837X_MAKE_MIB_COUNTER(67, 1, lengthFieldError),
	RTL837X_MAKE_MIB_COUNTER(68, 1, falseCarrieimes),
	RTL837X_MAKE_MIB_COUNTER(69, 1, underSizeOctets),
	RTL837X_MAKE_MIB_COUNTER(70, 1, framingErrors),

	RTL837X_MAKE_MIB_COUNTER(72, 1, rxMacDiscards),
	RTL837X_MAKE_MIB_COUNTER(73, 1, rxMacIPGShortDropRT),

	RTL837X_MAKE_MIB_COUNTER(75, 1, dot1dTpLearnedEntryDiscards),
	RTL837X_MAKE_MIB_COUNTER(76, 1, egrQueue7DropPktRT),
	RTL837X_MAKE_MIB_COUNTER(77, 1, egrQueue6DropPktRT),
	RTL837X_MAKE_MIB_COUNTER(78, 1, egrQueue5DropPktRT),
	RTL837X_MAKE_MIB_COUNTER(79, 1, egrQueue4DropPktRT),
	RTL837X_MAKE_MIB_COUNTER(80, 1, egrQueue3DropPktRT),
	RTL837X_MAKE_MIB_COUNTER(81, 1, egrQueue2DropPktRT),
	RTL837X_MAKE_MIB_COUNTER(82, 1, egrQueue1DropPktRT),
	RTL837X_MAKE_MIB_COUNTER(83, 1, egrQueue0DropPktRT),
	RTL837X_MAKE_MIB_COUNTER(84, 1, egrQueue7OutPktRT),
	RTL837X_MAKE_MIB_COUNTER(85, 1, egrQueue6OutPktRT),
	RTL837X_MAKE_MIB_COUNTER(86, 1, egrQueue5OutPktRT),
	RTL837X_MAKE_MIB_COUNTER(87, 1, egrQueue4OutPktRT),
	RTL837X_MAKE_MIB_COUNTER(88, 1, egrQueue3OutPktRT),
	RTL837X_MAKE_MIB_COUNTER(89, 1, egrQueue2OutPktRT),
	RTL837X_MAKE_MIB_COUNTER(90, 1, egrQueue1OutPktRT),
	RTL837X_MAKE_MIB_COUNTER(91, 1, egrQueue0OutPktRT),

	RTL837X_MAKE_MIB_COUNTER(92, 2, TxGoodCnt),
	RTL837X_MAKE_MIB_COUNTER(94, 2, RxGoodCnt),

	RTL837X_MAKE_MIB_COUNTER(96, 1, RxErrorCnt),
	RTL837X_MAKE_MIB_COUNTER(97, 1, TxErrorCnt),

	RTL837X_MAKE_MIB_COUNTER(98, 2, TxGoodCnt_phy),
	RTL837X_MAKE_MIB_COUNTER(100, 2, RxGoodCnt_phy),

	RTL837X_MAKE_MIB_COUNTER(102, 1, RxErrorCnt_phy),
	RTL837X_MAKE_MIB_COUNTER(103, 1, TxErrorCnt_phy)
};

static_assert(ARRAY_SIZE(rtl8372n_mib_counters) == RTL837X_MIB_END);

enum rtl837x_port_feature {
	RTL837X_PORT_FEATURE_NONEXIST = 0,
	RTL837X_PORT_FEATURE_PORT = BIT(0),
	RTL837X_PORT_FEATURE_SERDES = BIT(1),
	RTL837X_PORT_FEATURE_QSXGMII = BIT(2),
	RTL837X_PORT_FEATURE_INTERNAL = BIT(2),
};

struct rtl837x_chip_info {
	const char *name;
	u32 chip_model_id;
	u32 chip_model_char1;
	unsigned int port_feature[RTL837X_NUM_PORTS];
	bool is_support;
};

/* get_stats64 runs in atomic context, so the MIB counters are polled */
#define RTL8372N_STATS_INTERVAL_JIFFIES	(3 * HZ)

struct rtl8372n_port {
	struct realtek_priv *priv;
	struct phylink_pcs pcs;
	bool have_pcs;
	int index;
	struct delayed_work mib_work;
	spinlock_t stats_lock;		/* protects stats */
	struct rtnl_link_stats64 stats;
};

struct rtl8372n {
	netdev_features_t csum_feature_backup;
	bool pvid_enabled[RTL837X_NUM_PORTS];
	bool dsa_tag_8021q_vid[4096];
	struct rtl8372n_port ports[RTL837X_NUM_PORTS];
	const struct rtl837x_chip_info *chip_info;
};

static const struct rtl837x_chip_info rtl837x_chip_infos[] = {
	{
		.name = "RTL8372N",
		.chip_model_id = 0x8372,
		.chip_model_char1 = 14, // 'N' = 78 = 64('@') + 14
		.port_feature = {
			[3] = RTL837X_PORT_FEATURE_PORT | RTL837X_PORT_FEATURE_SERDES,
			[4] = RTL837X_PORT_FEATURE_PORT | RTL837X_PORT_FEATURE_INTERNAL,
			[5] = RTL837X_PORT_FEATURE_PORT | RTL837X_PORT_FEATURE_INTERNAL,
			[6] = RTL837X_PORT_FEATURE_PORT | RTL837X_PORT_FEATURE_INTERNAL,
			[7] = RTL837X_PORT_FEATURE_PORT | RTL837X_PORT_FEATURE_INTERNAL,
			[8] = RTL837X_PORT_FEATURE_PORT | RTL837X_PORT_FEATURE_SERDES,
		},
		.is_support = true,
	},
	{
		.name = "RTL8372",
		.chip_model_id = 0x8372,
		.chip_model_char1 = 0,
		.port_feature = {
			[3] = RTL837X_PORT_FEATURE_PORT | RTL837X_PORT_FEATURE_SERDES,
			[4] = RTL837X_PORT_FEATURE_PORT | RTL837X_PORT_FEATURE_INTERNAL,
			[5] = RTL837X_PORT_FEATURE_PORT | RTL837X_PORT_FEATURE_INTERNAL,
			[6] = RTL837X_PORT_FEATURE_PORT | RTL837X_PORT_FEATURE_INTERNAL,
			[7] = RTL837X_PORT_FEATURE_PORT | RTL837X_PORT_FEATURE_INTERNAL,
			[8] = RTL837X_PORT_FEATURE_PORT | RTL837X_PORT_FEATURE_SERDES,
		},
		.is_support = true,
	},
	{
		.name = "RTL8373N",
		.chip_model_id = 0x8373,
		.chip_model_char1 = 14,
		.port_feature = {
			[0] = RTL837X_PORT_FEATURE_PORT,
			[1] = RTL837X_PORT_FEATURE_PORT,
			[2] = RTL837X_PORT_FEATURE_PORT,
			[3] = RTL837X_PORT_FEATURE_PORT | RTL837X_PORT_FEATURE_QSXGMII,
			[4] = RTL837X_PORT_FEATURE_PORT | RTL837X_PORT_FEATURE_INTERNAL,
			[5] = RTL837X_PORT_FEATURE_PORT | RTL837X_PORT_FEATURE_INTERNAL,
			[6] = RTL837X_PORT_FEATURE_PORT | RTL837X_PORT_FEATURE_INTERNAL,
			[7] = RTL837X_PORT_FEATURE_PORT | RTL837X_PORT_FEATURE_INTERNAL,
			[8] = RTL837X_PORT_FEATURE_PORT | RTL837X_PORT_FEATURE_SERDES,
		},
		.is_support = false,
	},
	{
		.name = "RTL8373",
		.chip_model_id = 0x8373,
		.chip_model_char1 = 0,
		.port_feature = {
			[0] = RTL837X_PORT_FEATURE_PORT,
			[1] = RTL837X_PORT_FEATURE_PORT,
			[2] = RTL837X_PORT_FEATURE_PORT,
			[3] = RTL837X_PORT_FEATURE_PORT | RTL837X_PORT_FEATURE_QSXGMII,
			[4] = RTL837X_PORT_FEATURE_PORT | RTL837X_PORT_FEATURE_INTERNAL,
			[5] = RTL837X_PORT_FEATURE_PORT | RTL837X_PORT_FEATURE_INTERNAL,
			[6] = RTL837X_PORT_FEATURE_PORT | RTL837X_PORT_FEATURE_INTERNAL,
			[7] = RTL837X_PORT_FEATURE_PORT | RTL837X_PORT_FEATURE_INTERNAL,
			[8] = RTL837X_PORT_FEATURE_PORT | RTL837X_PORT_FEATURE_SERDES,
		},
		.is_support = false,
	},
	{
		.name = "RTL8366U",
		.chip_model_id = 0x8366,
		.chip_model_char1 = 21,  // 'U' = 85 = 64('@') + 21
		.port_feature = {
			[3] = RTL837X_PORT_FEATURE_PORT | RTL837X_PORT_FEATURE_SERDES,
			[4] = RTL837X_PORT_FEATURE_PORT,
			[5] = RTL837X_PORT_FEATURE_PORT,
			[6] = RTL837X_PORT_FEATURE_PORT,
			[7] = RTL837X_PORT_FEATURE_PORT,
			[8] = RTL837X_PORT_FEATURE_PORT | RTL837X_PORT_FEATURE_SERDES,
		},
		.is_support = false,
	},
};

static bool is_8021q_tag(enum dsa_tag_protocol tag_proto)
{
	switch (tag_proto) {
	case DSA_TAG_PROTO_MXL862_8021Q:
		return true;
	default:
		return false;
	}
}

static int rtl837x_get_chip_info(struct realtek_priv *priv,
				 const struct rtl837x_chip_info **chip_info)
{
	u32 model_id, model_char1, val;
	int ret;

	ret = regmap_read(priv->map, RTL837X_CHIP_ID_REG, &val);
	if (ret)
		return ret;

	model_id = FIELD_GET(RTL837X_CHIP_ID_MODEL_MASK, val);
	model_char1 = FIELD_GET(RTL837X_CHIP_ID_CHAR1_MASK, val);

	for (int i = 0; i < ARRAY_SIZE(rtl837x_chip_infos); i++) {
		if (rtl837x_chip_infos[i].chip_model_id == model_id &&
		    rtl837x_chip_infos[i].chip_model_char1 == model_char1) {
			*chip_info = &rtl837x_chip_infos[i];
			return 0;
		}
	}

	dev_err(priv->dev, "unrecognized switch (MODEL_NAME_REG=0x%08x)\n", val);
	return -ENODEV;
}

static int rtl8372n_detect(struct realtek_priv *priv)
{
	const struct rtl837x_chip_info *chip_info;
	struct rtl8372n *chip_data = priv->chip_data;
	struct device *dev = priv->dev;
	int ret;

	ret = rtl837x_get_chip_info(priv, &chip_info);
	if (ret)
		return ret;

	dev_info(dev, "found an %s switch\n", chip_info->name);
	if (!chip_info->is_support) {
		dev_err(priv->dev, "Chip %s is not support now\n", chip_info->name);
		return -ENODEV;
	}

	chip_data->chip_info = chip_info;
	priv->num_ports = RTL837X_NUM_PORTS;
	priv->mib_counters = rtl8372n_mib_counters;
	priv->num_mib_counters = ARRAY_SIZE(rtl8372n_mib_counters);

	return 0;
}

static int rtl8372n_get_vlan_4k(struct realtek_priv *priv, u32 vid,
				struct rtl837x_vlan_4k *vlan4k)
{
	struct rtl837x_vlan_data vlan;
	int ret;

	ret = rtl837x_vlan_get(priv, vid, &vlan);
	if (ret) {
		dev_dbg(priv->dev, "[%s]: failed: ret: %d\n", __func__, ret);
		return ret;
	}

	vlan4k->vid = vid;
	vlan4k->member = vlan.mbr;
	vlan4k->untag = vlan.untag;
	vlan4k->fid = vlan.fid;

	return 0;
}

static int rtl8372n_set_vlan_4k(struct realtek_priv *priv,
				const struct rtl837x_vlan_4k *vlan4k)
{
	struct rtl837x_vlan_data vlan = {0};
	int ret;

	vlan.mbr = vlan4k->member;
	vlan.untag = vlan4k->untag;
	vlan.fid = vlan4k->fid;
	vlan.ivl_en = 1;

	ret = rtl837x_vlan_set(priv, vlan4k->vid, &vlan);
	if (ret) {
		dev_dbg(priv->dev, "rtl837x_vlan_set: failed: ret: %d\n", ret);
		return ret;
	}
	return 0;
}

static int rtl8372n_vlan_update(struct realtek_priv *priv, int vid, u32 member,
				u32 untag, u32 fid)
{
	struct rtl837x_vlan_data vlan_data;
	int ret;

	ret = rtl837x_vlan_get(priv, vid, &vlan_data);

	if (ret)
		return ret;

	vlan_data.mbr |= member;
	vlan_data.untag &= ~member;
	vlan_data.untag |= untag;
	vlan_data.fid = fid;
	vlan_data.ivl_en = 1;

	return rtl837x_vlan_set(priv, vid, &vlan_data);
}

static int rtl8372n_set_pvid(struct realtek_priv *priv, int port,
			     u16 vid)
{
	enum rtl837x_frame_ingress prev_accepted_frame;
	struct rtl8372n *chip_data = priv->chip_data;
	bool prev_pvid_enabled = chip_data->pvid_enabled[port];
	struct dsa_switch *ds = priv->ds;
	u32 configured_ports = 0;
	struct dsa_port *dp;
	bool pvid_enabled;
	u16 prev_pvid = 0;
	int ret;

	dev_dbg(priv->dev, "[%s]: port (%d), vid (%d)\n", __func__, port, vid);
	pvid_enabled = !!vid;

	ret = rtl837x_vlan_port_get_pvid(priv, port, &prev_pvid);
	if (ret)
		return ret;

	ret = rtl837x_vlan_port_get_framefilter(priv, port, &prev_accepted_frame);
	if (ret)
		return ret;

	ret = rtl837x_vlan_port_set_pvid(priv, port, vid);
	if (ret)
		return ret;

	chip_data->pvid_enabled[port] = pvid_enabled;

	/* When a port have its pvid, we can accept tagged/untagged frame came in */
	ret = rtl837x_vlan_port_set_framefilter(priv, port, RTL837X_FRAME_TYPE_ANY_FRAME);
	if (ret)
		goto undo_pvid;

	/* When a port have its pvid, untagged frame will marked the port pvid and forward */
	dsa_switch_for_each_available_port(dp, ds) {
		ret = rtl837x_vlan_port_set_egress_vlan_keep(priv, port, dp->index, false);
		if (ret)
			goto undo_vlan_keep;

		configured_ports |= BIT(dp->index);
	}

	return 0;

undo_vlan_keep:
	dsa_switch_for_each_port(dp, ds) {
		if (configured_ports & BIT(dp->index))
			(void)rtl837x_vlan_port_set_egress_vlan_keep(priv, port, dp->index,
						       !prev_pvid_enabled);
	}

	if (prev_accepted_frame != RTL837X_FRAME_TYPE_ANY_FRAME)
		(void)rtl837x_vlan_port_set_framefilter(priv, port, prev_accepted_frame);

undo_pvid:
	(void)rtl837x_vlan_port_set_pvid(priv, port, prev_pvid);
	chip_data->pvid_enabled[port] = prev_pvid_enabled;
	return ret;
}

static int rtl8372n_clean_pvid(struct realtek_priv *priv, int port, u16 vid)
{
	enum rtl837x_frame_ingress accepted_frame, prev_accepted_frame;
	struct rtl8372n *chip_data = priv->chip_data;
	struct dsa_switch *ds = priv->ds;
	u32 configured_ports = 0;
	struct dsa_port *dp;
	u16 prev_pvid;
	int ret;

	ret = rtl837x_vlan_port_get_pvid(priv, port, &prev_pvid);
	if (ret)
		return ret;

	if (prev_pvid != vid)
		return 0;

	ret = rtl837x_vlan_port_get_framefilter(priv, port,
						&prev_accepted_frame);
	if (ret) {
		dev_err(priv->dev, "Failed to get current framefilter\n");
		return ret;
	}

	/* When a port doesn't have pvid, and it enable vlan filter,
	 * Only accept tagged frame came in
	 */
	if (dsa_port_is_vlan_filtering(dsa_to_port(ds, port))) {
		accepted_frame = RTL837X_FRAME_TYPE_TAGGED_ONLY;

		ret = rtl837x_vlan_port_set_framefilter(priv, port,
							accepted_frame);
		if (ret) {
			dev_err(priv->dev, "Failed to set port frame filter\n");
			return ret;
		}
	} else {
		/* skip undo_set_framefilter */
		accepted_frame = prev_accepted_frame;
	}

	ret = rtl837x_vlan_port_set_pvid(priv, port, 0);
	if (ret) {
		dev_err(priv->dev, "Failed to set port PVID to 0\n");
		goto undo_set_framefilter;
	}

	chip_data->pvid_enabled[port] = false;

	/* When a port doesn't have pvid, the untagged frame must be
	 * forwarded as-is and must not carry a VID 0.
	 */
	dsa_switch_for_each_available_port(dp, ds) {
		ret = rtl837x_vlan_port_set_egress_vlan_keep(priv, port, dp->index, true);
		if (ret)
			goto undo_vlan_keep;

		configured_ports |= BIT(dp->index);
	}

	return 0;

undo_vlan_keep:
	dsa_switch_for_each_port(dp, ds) {
		if (configured_ports & BIT(dp->index))
			(void)rtl837x_vlan_port_set_egress_vlan_keep(priv, port, dp->index,
						       false);
	}

	(void)rtl837x_vlan_port_set_pvid(priv, port, prev_pvid);

undo_set_framefilter:
	if (prev_accepted_frame != accepted_frame)
		(void)rtl837x_vlan_port_set_framefilter(priv, port,
							  prev_accepted_frame);

	return ret;
}

static int rtl8372n_get_mib_counter(struct realtek_priv *priv,
				    int port,
		const struct rtl837x_mib_counter *mib,
		u64 *mibvalue)
{
	u32 val_h, val_l, val, tmp;
	int mib_id = (mib->offset) / 2;
	int ret;

	tmp = (FIELD_PREP(RTL837X_MIB_CTRL_PORT_IDX_MASK, port) |
					FIELD_PREP(RTL837X_MIB_CTRL_ID_MASK, mib_id) |
					FIELD_PREP(RTL837X_MIB_CTRL_BUSY_MASK, 1));

	ret = rtl837x_reg_write(priv, RTL837X_MIB_CTRL_REG, tmp);
	if (ret)
		return ret;

	ret = regmap_read_poll_timeout(priv->map, RTL837X_MIB_CTRL_REG, tmp,
				       ((tmp & RTL837X_MIB_CTRL_BUSY_MASK) == 0),
			  0, 1000
			);
	if (ret)
		return ret;

	if (mib->length > 1) {
		ret = rtl837x_reg_read(priv, RTL837X_MIB_COUNTER_LO_REG, &val_l);
		if (ret)
			return ret;
		ret = rtl837x_reg_read(priv, RTL837X_MIB_COUNTER_HI_REG, &val_h);
		if (ret)
			return ret;
		*mibvalue = ((u64)val_l << 32) | val_h;
		return 0;
	}

	if (mib->offset % 2)
		ret = rtl837x_reg_read(priv, RTL837X_MIB_COUNTER_HI_REG, &val);
	else
		ret = rtl837x_reg_read(priv, RTL837X_MIB_COUNTER_LO_REG, &val);
	*mibvalue = val;
	return ret;
}

static enum dsa_tag_protocol rtl8372n_get_tag_protocol(struct dsa_switch *ds,
						       int port,
							enum dsa_tag_protocol mp)
{
	struct realtek_priv *priv = ds->priv;
	struct device *dev = priv->dev;

	dev_dbg(dev, "get_DSA_PROTO port:%d\n", port);

	return priv->tag_proto;
}

static int rtl8372n_mdio_phy_read_c22(struct mii_bus *bus, int addr, int regnum)
{
	struct realtek_priv *priv = bus->priv;
	u16 val;
	int ret;

	ret = priv->ops->phy_read_c22(priv, addr, regnum, &val);

	if (ret)
		return ret;

	return val;
}

static int rtl8372n_mdio_phy_write_c22(struct mii_bus *bus, int addr, int regnum, u16 val)
{
	struct realtek_priv *priv = bus->priv;

	return priv->ops->phy_write_c22(priv, addr, regnum, val);
}

static int rtl8372n_mdio_phy_read_c45(struct mii_bus *bus, int port, int devad, int regnum)
{
	struct realtek_priv *priv = bus->priv;
	u16 val;
	int ret;

	ret = priv->ops->phy_read_c45(priv, port, devad, regnum, &val);
	if (ret)
		return ret;

	return val;
}

static int rtl8372n_mdio_phy_write_c45(struct mii_bus *bus, int port, int devad,
				       int regnum, u16 val)
{
	struct realtek_priv *priv = bus->priv;

	return priv->ops->phy_write_c45(priv, port, devad, regnum, val);
}

static struct device_node *of_get_phy_node(struct device_node *dn)
{
	struct device_node *phy_dn;

	phy_dn = of_parse_phandle(dn, "phy-handle", 0);
	if (!phy_dn)
		phy_dn = of_parse_phandle(dn, "phy", 0);

	return phy_dn;
}

static int rtl8372n_setup_mdio(struct realtek_priv *priv)
{
	struct device_node *phy_dn, *mnp, *phy_parent_mnp;
	const struct rtl837x_chip_info *chip_info;
	struct rtl8372n *chip_data = priv->chip_data;
	struct device_node *np = priv->dev->of_node;
	struct dsa_switch *ds = priv->ds;
	struct mii_bus *bus;
	struct dsa_port *dp;
	struct device *dev = priv->dev;
	static int idx;
	u32 phy_addr;
	int ret = 0;

	chip_info = chip_data->chip_info;

	mnp = of_get_child_by_name(np, "mdio");

	if (!mnp || !of_device_is_available(mnp)) {
		dev_err(priv->dev, "not found mdio node in device node");
		ret = -ENODEV;
		goto out;
	}

	bus = devm_mdiobus_alloc(dev);
	if (!bus) {
		ret = -ENOMEM;
		goto out;
	}

#if KERNEL_VERSION(6, 12, 44) <= LINUX_VERSION_CODE
	if (!mnp)
		ds->user_mii_bus = bus;
#endif

	bus->priv = priv;
	bus->name = KBUILD_MODNAME "-mdio";
	snprintf(bus->id, MII_BUS_ID_SIZE, KBUILD_MODNAME "-%d", idx++);
	bus->read = rtl8372n_mdio_phy_read_c22;
	bus->write = rtl8372n_mdio_phy_write_c22;
	bus->read_c45 = rtl8372n_mdio_phy_read_c45;
	bus->write_c45 = rtl8372n_mdio_phy_write_c45;
	bus->parent = dev;
	bus->phy_mask = ~ds->phys_mii_mask;

	ret = devm_of_mdiobus_register(dev, bus, mnp);
	if (ret) {
		dev_err(dev, "failed to register MDIO bus: %d\n", ret);
		goto out;
	}

	dsa_switch_for_each_available_port(dp, ds) {
		phy_dn = of_get_phy_node(dp->dn);
		if (phy_dn)
			phy_parent_mnp = of_get_parent(phy_dn);
		else
			phy_parent_mnp = NULL;
		if (phy_dn)
			ret = of_property_read_u32(phy_dn, "reg", &phy_addr);
		if (ret ||
		    !phy_dn || !phy_parent_mnp ||
			  phy_parent_mnp != mnp ||
			  chip_info->port_feature[dp->index] & RTL837X_PORT_FEATURE_INTERNAL) {
			/* Check the Internal port is connected to the correct phy handle */
			if (!ret && phy_dn && phy_parent_mnp &&
			    chip_info->port_feature[dp->index] & RTL837X_PORT_FEATURE_INTERNAL &&
				  (phy_addr != dp->index || phy_parent_mnp != mnp)) {
				if (phy_parent_mnp)
					of_node_put(phy_parent_mnp);
				if (phy_dn)
					of_node_put(phy_dn);
				dev_err(priv->dev,
					"Internal port(%d) connect to a invalid phy(%d)\n",
					dp->index, phy_addr);
				ret = -ENODEV;
				goto out;
			}

			dev_dbg(priv->dev,
				"[%s] use port index(%d) as phy addr\n",
				__func__, dp->index);
			phy_addr = dp->index;
		}
		if (phy_parent_mnp)
			of_node_put(phy_parent_mnp);
		if (phy_dn)
			of_node_put(phy_dn);

		dev_dbg(priv->dev, "[%s] set port(%d) phy addr 0x%x\n", __func__,
			dp->index, phy_addr);

		/* set port mdio address */
		ret = regmap_update_bits(priv->map,
					 RTL837X_SMI_PORT_MDIO_ADDR_REG(dp->index),
			    RTL837X_SMI_PORT_MDIO_ADDR_MASK(dp->index),
			    phy_addr << __ffs(RTL837X_SMI_PORT_MDIO_ADDR_MASK(dp->index))
			);
		if (ret)
			goto out;
	}

out:
	if (mnp)
		of_node_put(mnp);
	return ret;
}

static unsigned int rtl8372n_sds_pcs_inband_caps(struct phylink_pcs *pcs,
						 phy_interface_t interface)
{
	struct rtl8372n_port *port_priv = container_of(pcs, struct rtl8372n_port, pcs);
	int port = port_priv->index;

	switch (port) {
	case 3:
	case 8:
		switch (interface) {
		case PHY_INTERFACE_MODE_1000BASEX:
		case PHY_INTERFACE_MODE_2500BASEX:
		case PHY_INTERFACE_MODE_SGMII:
			return LINK_INBAND_ENABLE | LINK_INBAND_DISABLE;
		case PHY_INTERFACE_MODE_10GBASER:
			return LINK_INBAND_DISABLE;
		case PHY_INTERFACE_MODE_USXGMII:
			return LINK_INBAND_ENABLE;
		default:
			return 0;
		}
	default:
		return 0;
	}
}

#if KERNEL_VERSION(6, 18, 0) <= LINUX_VERSION_CODE
static void rtl8372n_sds_pcs_get_state(struct phylink_pcs *pcs, unsigned int neg_mode,
				       struct phylink_link_state *state)
#else
static void rtl8372n_sds_pcs_get_state(struct phylink_pcs *pcs,
				       struct phylink_link_state *state)
#endif
{
	struct rtl8372n_port *port_priv = container_of(pcs, struct rtl8372n_port, pcs);
	struct realtek_priv *priv = port_priv->priv;
	int port = port_priv->index;
	u32 tmp;
	int ret;

	ret = rtl837x_reg_bits_read(priv, RTL837X_MAC_LINK_STS_REG, BIT(port), &tmp);
	if (ret < 0)
		return;
	state->link = tmp & 1;
	state->an_complete = tmp & 1;

	ret = rtl837x_reg_bits_read(priv, RTL837X_MAC_LINK_DUP_STS_REG, BIT(port), &tmp);
	if (ret < 0)
		return;
	state->duplex = tmp & 1;

	state->pause &= ~(MLO_PAUSE_RX | MLO_PAUSE_TX);

	ret = rtl837x_reg_bits_read(priv, RTL837X_MAC_RX_PAUSE_STS_REG, BIT(port), &tmp);
	if (ret < 0)
		return;
	if (tmp & 1)
		state->pause |= MLO_PAUSE_RX;

	ret = rtl837x_reg_bits_read(priv, RTL837X_MAC_TX_PAUSE_STS_REG, BIT(port), &tmp);
	if (ret < 0)
		return;
	if (tmp & 1)
		state->pause |= MLO_PAUSE_TX;

	ret = rtl837x_reg_read(priv, RTL837X_MAC_LINK_SPD_STS_REG(port), &tmp);
	if (ret)
		return;
	tmp = (tmp & RTL837X_MAC_LINK_SPD_STS_MASK(port)) >>
	      __ffs(RTL837X_MAC_LINK_SPD_STS_MASK(port));
	switch (tmp) {
	case 0:
		state->speed = SPEED_10;
		break;
	case 1:
		state->speed = SPEED_100;
		break;
	case 2:
		state->speed = SPEED_1000;
		break;
	case 4:
		state->speed = SPEED_10000;
		break;
	case 5:
		state->speed = SPEED_2500;
		break;
	case 6:
		state->speed = SPEED_5000;
		break;
	default:
		state->speed = SPEED_UNKNOWN;
		break;
	}

	if (!state->link)
		state->speed = SPEED_UNKNOWN;
}

/*
 * NOTE: this performs a full SerDes reconfiguration (off/on + patch +
 * reset) on every call, including the lightweight "advertisement only"
 * path from phylink_change_inband_advert(). It therefore causes one
 * link flap when the user changes pause/advertising at runtime.
 * This is intentional until the SerDes behaviour is fully characterised.
 */
static int rtl8372n_sds_pcs_config(struct phylink_pcs *pcs, unsigned int neg_mode,
				   phy_interface_t interface,
			     const unsigned long *advertising,
			     bool permit_pause_to_mac)
{
	struct rtl8372n_port *port_priv = container_of(pcs, struct rtl8372n_port, pcs);
	struct realtek_priv *priv = port_priv->priv;
	u8 sds_idx;
	int port = port_priv->index;
	int ret;

	sds_idx = PORT_TO_SERDES_IDX(port);

	dev_dbg(priv->dev, "[%s]PCS config serdes(%d) mode(%s) neg_mode(0x%x)\n", __func__,
		sds_idx,
			  phy_modes(interface), neg_mode);

	ret = rtl837x_serdes_off(priv, sds_idx);
	if (ret)
		return ret;

	ret = rtl837x_serdes_set_serdes_mode(priv, sds_idx, interface);
	if (ret)
		return ret;

	// Apply patch
	ret = rtl837x_serdes_an_patch(priv, sds_idx, interface);
	if (ret)
		return ret;
	ret = rtl837x_serdes_mac_patch(priv, sds_idx);
	if (ret)
		return ret;

	msleep(250);

	ret = rtl837x_serdes_pause_frame_config(priv, sds_idx,
						interface, advertising);
	if (ret)
		return ret;
	ret = rtl837x_serdes_an_config(priv, sds_idx, interface, neg_mode);
	if (ret)
		return ret;

	ret = rtl837x_serdes_on(priv, sds_idx);
	if (ret)
		return ret;

	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x1F, 0x00, 0xffff, 0xB);
	if (ret)
		return ret;
	msleep(50);
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x1F, 0x00, 0xffff, 0x0);
	if (ret)
		return ret;
	msleep(50);

	switch (interface) {
	case PHY_INTERFACE_MODE_USXGMII:
	case PHY_INTERFACE_MODE_10GBASER:
		dev_dbg(priv->dev, "[%s]Reset Serdes RX R\n", __func__);
		rtl837x_serdes_reset_10G(priv, PORT_TO_SERDES_IDX(port));
		break;
	default:
		dev_dbg(priv->dev, "[%s]Reset Serdes RX X\n", __func__);
		rtl837x_serdes_reset_XSG(priv, PORT_TO_SERDES_IDX(port));
		break;
	}

	return 0;
}

static void rtl8372n_sds_pcs_link_up(struct phylink_pcs *pcs, unsigned int neg_mode,
				     phy_interface_t interface, int speed, int duplex)
{
	struct rtl8372n_port *port_priv = container_of(pcs, struct rtl8372n_port, pcs);
	struct realtek_priv *priv = port_priv->priv;
	int port = port_priv->index;

	dev_dbg(priv->dev, "[%s]PCS link up serdes (%d) mode (%s)\n", __func__,
		PORT_TO_SERDES_IDX(port),
			  phy_modes(interface));
}

// TODO? OR Just Empty Func?
static void rtl8372n_sds_pcs_an_restart(struct phylink_pcs *pcs)
{}

static const struct phylink_pcs_ops rtl8372n_sds_pcs_ops = {
	.pcs_inband_caps = rtl8372n_sds_pcs_inband_caps,
	.pcs_get_state = rtl8372n_sds_pcs_get_state,
	.pcs_config = rtl8372n_sds_pcs_config,
	.pcs_link_up = rtl8372n_sds_pcs_link_up,
	.pcs_an_restart = rtl8372n_sds_pcs_an_restart,
};

static void rtl8372n_phylink_get_caps(struct dsa_switch *ds, int port,
				      struct phylink_config *config)
{
	if (port == 3 || port == 8) {
		/* TODO: check whether 100BASEX and USXGMII are really wired
		 * up on the SerDes ports
		 */
		__set_bit(PHY_INTERFACE_MODE_100BASEX,
			  config->supported_interfaces);
		__set_bit(PHY_INTERFACE_MODE_SGMII,
			  config->supported_interfaces);
		__set_bit(PHY_INTERFACE_MODE_1000BASEX,
			  config->supported_interfaces);
		__set_bit(PHY_INTERFACE_MODE_2500BASEX,
			  config->supported_interfaces);
		__set_bit(PHY_INTERFACE_MODE_10GBASER,
			  config->supported_interfaces);
		__set_bit(PHY_INTERFACE_MODE_USXGMII,
			  config->supported_interfaces);

		config->mac_capabilities = MAC_10000FD | MAC_2500FD | MAC_1000 |
					   MAC_100 | MAC_10 |
					   MAC_SYM_PAUSE | MAC_ASYM_PAUSE;
	} else {
		__set_bit(PHY_INTERFACE_MODE_INTERNAL, config->supported_interfaces);
		config->mac_capabilities = MAC_2500FD | MAC_1000 | MAC_100 | MAC_10 |
				    MAC_SYM_PAUSE | MAC_ASYM_PAUSE;
	}
}

static struct phylink_pcs *rtl8372n_phylink_mac_select_pcs(struct phylink_config *config,
							   phy_interface_t interface)
{
	struct rtl8372n *chip_data;
	struct realtek_priv *priv;
	struct dsa_port *dp = dsa_phylink_to_port(config);

	priv = dp->ds->priv;
	chip_data = priv->chip_data;

	if (chip_data->ports[dp->index].have_pcs)
		return &chip_data->ports[dp->index].pcs;
	return NULL;
}

// TODO: config for internal phy?
static void rtl8372n_phylink_mac_config(struct phylink_config *config, unsigned int mode,
					const struct phylink_link_state *state)
{
	struct realtek_priv *priv;
	struct dsa_port *dp = dsa_phylink_to_port(config);

	priv = dp->ds->priv;

	if (!rtl8372n_is_serdes_port(dp->index))
		return;
	dev_dbg(priv->dev,
		"[%s]: port:%d interface:%s speed:%s duplex:%s advertising:%*pb pause:0x%x\n",
		__func__, dp->index, phy_modes(state->interface),
		phy_speed_to_str(state->speed),
		phy_duplex_to_str(state->duplex),
		__ETHTOOL_LINK_MODE_MASK_NBITS, state->advertising,
		state->pause);
}

static void rtl8372n_phylink_mac_link_down(struct phylink_config *config,
					   unsigned int mode,
					   phy_interface_t interface)
{
	struct realtek_priv *priv;
	struct dsa_port *dp = dsa_phylink_to_port(config);

	priv = dp->ds->priv;

	if (!rtl8372n_is_serdes_port(dp->index))
		return;

	dev_dbg(priv->dev, "[%s]: port:%d interface:%s\n", __func__,
		dp->index, phy_modes(interface));
}

static void rtl8372n_phylink_mac_link_up(struct phylink_config *config,
					 struct phy_device *phy, unsigned int mode,
			phy_interface_t interface, int speed, int duplex,
			bool tx_pause, bool rx_pause)
{
	struct realtek_priv *priv;
	struct dsa_port *dp = dsa_phylink_to_port(config);

	priv = dp->ds->priv;

	if (!rtl8372n_is_serdes_port(dp->index))
		return;
	dev_dbg(priv->dev,
		"[%s]: port:%d mode:%s speed:%s duplex:%s tx_pause:%d rx_pause:%d\n", __func__,
			   dp->index, phy_modes(interface), phy_speed_to_str(speed),
			   phy_duplex_to_str(duplex), tx_pause, rx_pause
			);
}

/* TODO: config port based on port_feature */
static const struct phylink_mac_ops rtl8372n_phylink_mac_ops = {
	.mac_select_pcs	= rtl8372n_phylink_mac_select_pcs,
	.mac_config	= rtl8372n_phylink_mac_config,
	.mac_link_down	= rtl8372n_phylink_mac_link_down,
	.mac_link_up	= rtl8372n_phylink_mac_link_up,
};

/*
 * Deprecated: this is a hardcoded LED/GPIO matrix initialization sequence
 * and is scheduled for removal. Use the "leds" device tree node instead of
 * the "extra-init" property to describe the LED configuration.
 */
static int of_extra_init(struct dsa_switch *ds)
{
	struct realtek_priv *priv = ds->priv;
	struct device_node *np = ds->dev->of_node;
	const __be32 *list;
	int size, data_len;
	u32 reg, mask, val;

	list = of_get_property(np, "extra-init", &size);
	if (!list || !size)
		return 0;

	data_len = size / (3 * sizeof(__be32));
	for (int i = 0; i < data_len; i++) {
		reg = be32_to_cpu(*list);
		list++;
		mask = be32_to_cpu(*list);
		list++;
		val = be32_to_cpu(*list);
		list++;
		dev_dbg(ds->dev, "[%s] reg:0x%04X mask:0x%08X val:0x%08X\n",
			__func__, reg, mask, val);
		rtl837x_reg_bits_write(priv, reg, mask, val);
	}
	return 0;
}

static int rtl8372n_set_tag_rtl(struct dsa_switch *ds)
{
	struct net_device *master_dev = NULL;
	struct dsa_port *dp, *cpu_dp = NULL;
	struct rtl8372n *chip_data;
	struct realtek_priv *priv = ds->priv;
	int ret;

	chip_data = priv->chip_data;

	// Only support one CPU port
	dsa_switch_for_each_cpu_port(dp, ds) {
		cpu_dp = dp;
		break;
	}

	if (!cpu_dp)
		return -ENODEV;

#if KERNEL_VERSION(6, 12, 44) >= LINUX_VERSION_CODE
	master_dev = cpu_dp->master;
#else
	master_dev = cpu_dp->conduit;
#endif

	if (!master_dev) {
		dev_err(priv->dev, "Cannot get master netdev from cpu port\n");
		return -ENODEV;
	}

	// Set external CPU DSA tag insert mode
	/*
	 *	CPU_INSERT_TO_ALL = 0,
	 *	CPU_INSERT_TO_TRAPPING,
	 *	CPU_INSERT_TO_NONE,
	 *	CPU_INSERT_END
	 */
	ret = rtl837x_reg_bits_write(priv, RTL837X_CPU_CTRL_REG,
				     RTL837X_CPU_CTRL_INSERTMODE_MASK, 0
			);
	if (ret)
		return ret;

	// Set external CPU port
	ret = rtl837x_reg_bits_write(priv, RTL837X_EXT_CPU_CTRL_REG,
				     RTL837X_EXT_CPU_CTRL_PORT_IDX_MASK, cpu_dp->index
			);
	if (ret)
		return ret;

	// Enable CPU tag
	ret = rtl837x_reg_bits_write(priv, RTL837X_CPU_CTRL_REG,
				     RTL837X_CPU_CTRL_EN_MASK, 1
			);
	if (ret)
		return ret;

	// Add cpu port to RTL8_4 TAG aware port
	ret = rtl837x_reg_bits_write(priv, RTL837X_CPU_TAG_AWARE_CTRL_REG, BIT(cpu_dp->index), 1);
	if (ret)
		return ret;

	chip_data->csum_feature_backup = master_dev->wanted_features &
					 (NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM);
	chip_data->csum_feature_backup |= master_dev->wanted_features &
					  NETIF_F_HW_CSUM;

	master_dev->wanted_features &= ~(NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM);
	master_dev->wanted_features &= ~NETIF_F_HW_CSUM;
	netdev_update_features(master_dev);

	return 0;
}

static int rtl8372n_teardown_tag_rtl(struct dsa_switch *ds)
{
	struct net_device *master_dev = NULL;
	struct dsa_port *dp, *cpu_dp = NULL;
	struct rtl8372n *chip_data;
	struct realtek_priv *priv = ds->priv;

	chip_data = priv->chip_data;

	// Only support one CPU port
	dsa_switch_for_each_cpu_port(dp, ds) {
		cpu_dp = dp;
		break;
	}

	if (!cpu_dp)
		return -ENODEV;

#if KERNEL_VERSION(6, 12, 44) >= LINUX_VERSION_CODE
	master_dev = cpu_dp->master;
#else
	master_dev = cpu_dp->conduit;
#endif

	if (!master_dev) {
		dev_err(priv->dev, "Cannot get master netdev from cpu port\n");
		return -ENODEV;
	}

	// Set external CPU DSA tag insert mode
	/*
	 *	CPU_INSERT_TO_ALL = 0,
	 *	CPU_INSERT_TO_TRAPPING,
	 *	CPU_INSERT_TO_NONE,
	 *	CPU_INSERT_END
	 */
	rtl837x_reg_bits_write(priv, RTL837X_CPU_CTRL_REG,
			       RTL837X_CPU_CTRL_INSERTMODE_MASK, 2
			);

	// Disable CPU tag
	rtl837x_reg_bits_write(priv, RTL837X_CPU_CTRL_REG,
			       RTL837X_CPU_CTRL_EN_MASK, 0
			);

	// Remove cpu port from RTL8_4 TAG aware port
	dsa_switch_for_each_cpu_port(dp, ds) {
		rtl837x_reg_bits_write(priv, RTL837X_CPU_TAG_AWARE_CTRL_REG, BIT(dp->index), 0);
	}

	master_dev->wanted_features |= chip_data->csum_feature_backup;
	netdev_update_features(master_dev);

	(void)rtl837x_lut_l2_flush_all(priv);
	return 0;
}

static int rtl8372n_set_tag_8021q(struct dsa_switch *ds)
{
	struct realtek_priv *priv = ds->priv;
	int ret;

	//  bit0: internal cpu; bit1: external cpu
	ret = rtl837x_reg_bits_write(priv, RTL837X_SVLAN_TRAP_CTRL_REG,
				     RTL837X_SVLAN_TRAP_CTRL_CPU_TYPE_MASK, BIT(1)
			);
	if (ret)
		return ret;

	// Set S-VLAN upstream priority reference setting.
	/*
	 *	REF_INTERNAL_PRI = 0,
	 *	REF_CTAG_PRI,
	 *	REF_SVLAN_PRI,
	 *	REF_PB_PRI,
	 *	REF_PRI_END
	 */
	ret = rtl837x_reg_bits_write(priv, RTL837X_VS_CTRL_REG,
				     RTL837X_VS_CTRL_SPRISEL_MASK, 1
			);
	if (ret)
		return ret;

	// Drop packets received from the CPU port without an S-tag
	/*
	 *	UNTAG_DROP = 0,
	 *	UNTAG_TRAP,
	 *	UNTAG_ASSIGN,
	 *	UNTAG_END
	 */
	ret = rtl837x_reg_bits_write(priv, RTL837X_VS_CTRL_REG,
				     RTL837X_VS_CTRL_UNTAG_MASK, 0
		);
	if (ret)
		return ret;

	// Set custom TPID
	ret = rtl837x_reg_write(priv, RTL837X_VS_GLB_CTRL_REG, ETH_P_8021Q);
	if (ret)
		return ret;

	// Set cpu port as service port
	ret = rtl837x_reg_write(priv, RTL837X_VS_UPLINK_PORT_REG, dsa_cpu_ports(ds));
	if (ret)
		return ret;

	for (int idx = 0; idx <= RTL837x_C2SIDXMAX;  idx++) {
		ret = rtl837x_reg_write(priv, RTL837X_VLAN_C2S_ENTRY_REG(idx) + 4, 0);
		if (ret)
			return ret;

		ret = rtl837x_reg_write(priv, RTL837X_VLAN_C2S_ENTRY_REG(idx), 0);
		if (ret)
			return ret;
	}

	// Set Port Ingress Tag Action
	/*
	 *	UNASSIGN_PBSVID = 0,
	 *	UNASSIGN_TRAP,
	 *	UNASSIGN_END
	 */
	ret = rtl837x_reg_bits_write(priv, RTL837X_VS_CTRL_REG,
				     RTL837X_VS_CTRL_UIFSEG_MASK, 0
		);
	if (ret)
		return ret;

	ret = dsa_tag_8021q_register(ds, htons(ETH_P_8021Q));
	if (ret)
		return ret;

	return 0;
}

static int rtl8372n_teardown_tag_8021q(struct dsa_switch *ds)
{
	struct rtl837x_vlan_data vlan_data;
	struct rtl8372n *chip_data;
	struct realtek_priv *priv = ds->priv;
	struct dsa_port *dp = NULL;

	chip_data = priv->chip_data;

	if (ds->tag_8021q_ctx)
		dsa_tag_8021q_unregister(ds);

	// Clean service port
	rtl837x_reg_write(priv, RTL837X_VS_UPLINK_PORT_REG, 0);

	vlan_data.mbr = 0;
	vlan_data.untag = 0;
	vlan_data.fid = 0;
	vlan_data.ivl_en = 1;

	// Remove s-tag vlan entries
	for (int i = 0; i < 4096; i++) {
		if (chip_data->dsa_tag_8021q_vid[i]) {
			chip_data->dsa_tag_8021q_vid[i] = false;
			rtl837x_vlan_set(priv, i, &vlan_data);
		}
	}

	dsa_switch_for_each_user_port(dp, ds) {
		rtl837x_reg_bits_write(priv, RTL837X_VS_PORT_DFLT_SVID_REG(dp->index),
				       RTL837X_VS_PORT_DFLT_SVID_MASK(dp->index), 0
				);
	}

	(void)rtl837x_lut_l2_flush_all(priv);
	return 0;
}

static int rtl8372n_tag_8021q_vlan_add(struct dsa_switch *ds, int port,
				       u16 vid, u16 flags)
{
	struct rtl8372n *chip_data;
	struct realtek_priv *priv = ds->priv;
	u32 member = 0, untag = 0;
	int ret = 0;

	chip_data = priv->chip_data;

	dev_dbg(priv->dev, "[%s]: port: %d, vid:%u, flags: %u\n", __func__, port, vid, flags);

	// Set Port SVID
	if (flags & BRIDGE_VLAN_INFO_PVID)
		ret = rtl837x_reg_bits_write(priv, RTL837X_VS_PORT_DFLT_SVID_REG(port),
					     RTL837X_VS_PORT_DFLT_SVID_MASK(port), vid
				);
	if (ret)
		return ret;

	member |= BIT(port);
	if (dsa_is_user_port(ds, port))
		untag |= BIT(port);

	ret = rtl8372n_vlan_update(priv, vid, member, untag, 0);
	if (ret)
		goto fail_rollback;

	chip_data->dsa_tag_8021q_vid[vid] = true;
	return 0;

fail_rollback:
	dev_err(priv->dev, "failed to add 8021q tag for port(%d) vid: %u\n", port, vid);

	if (flags & BRIDGE_VLAN_INFO_PVID)
		rtl837x_reg_bits_write(priv, RTL837X_VS_PORT_DFLT_SVID_REG(port),
				       RTL837X_VS_PORT_DFLT_SVID_MASK(port), 0
				);
	return ret;
}

static int rtl8372n_tag_8021q_vlan_del(struct dsa_switch *ds, int port,
				       u16 vid)
{
	struct rtl837x_vlan_data vlan_data;
	struct rtl8372n *chip_data;
	struct realtek_priv *priv = ds->priv;
	int ret;

	chip_data = priv->chip_data;

	dev_dbg(priv->dev, "[%s]: port: %d, vid: %u\n", __func__, port, vid);

	ret = rtl837x_vlan_get(priv, vid, &vlan_data);
	if (ret)
		return ret;

	vlan_data.mbr &= ~BIT(port);
	vlan_data.untag &= ~BIT(port);
	vlan_data.ivl_en = 1;

	if (!vlan_data.mbr) {
		vlan_data.untag = 0;
		vlan_data.fid = 0;
	}

	ret = rtl837x_vlan_set(priv, vid, &vlan_data);
	if (ret) {
		dev_err(priv->dev,
			"failed to remove SVLAN %u\n",
			vid);
		return ret;
	}

	chip_data->dsa_tag_8021q_vid[vid] = false;

	// Clean Port SVID
	if (dsa_is_user_port(ds, port))
		ret = rtl837x_reg_bits_write(priv, RTL837X_VS_PORT_DFLT_SVID_REG(port),
					     RTL837X_VS_PORT_DFLT_SVID_MASK(port), 0
				);
	if (ret) {
		dev_err(priv->dev,
			"failed to clean port(%d) SVID %u\n",
			port, vid);
		return ret;
	}

	return 0;
}

static int rtl8372n_change_tag_protocol(struct dsa_switch *ds,
					enum dsa_tag_protocol proto)
{
	struct realtek_priv *priv = ds->priv;
	int ret;

	dev_dbg(priv->dev, "[%s]: proto: %d\n", __func__, proto);

	switch (proto) {
	case DSA_TAG_PROTO_MXL862_8021Q:
		ret = rtl8372n_teardown_tag_rtl(ds);
		if (ret)
			return ret;
		ret = rtl8372n_set_tag_8021q(ds);
		if (ret)
			return ret;
		break;
	case DSA_TAG_PROTO_RTL8_4:
		ret = rtl8372n_teardown_tag_8021q(ds);
		if (ret)
			return ret;
		ret = rtl8372n_set_tag_rtl(ds);
		if (ret)
			return ret;
		break;
	default:
		return -EPROTONOSUPPORT;
	}
	priv->tag_proto = proto;

	return 0;
}

static int rtl8372n_port_set_isolation(struct realtek_priv *priv, int port,
				       u32 mask)
{
	return rtl837x_reg_write(priv, RTL837X_PORT_ISOLATION_REG(port), mask);
}

static int rtl8372n_port_add_isolation(struct realtek_priv *priv, int port,
				       u32 mask)
{
	return rtl837x_reg_bits_write(priv, RTL837X_PORT_ISOLATION_REG(port),
				  mask, 0xffffffff);
}

static int rtl8372n_port_remove_isolation(struct realtek_priv *priv, int port,
					  u32 mask)
{
	return rtl837x_reg_bits_write(priv, RTL837X_PORT_ISOLATION_REG(port),
				  mask, 0);
}

static void rtl8372n_get_strings(struct dsa_switch *ds, int port, u32 stringset,
				 uint8_t *data)
{
	const struct rtl837x_mib_counter *mib;
	struct realtek_priv *priv = ds->priv;
	int i;

	/* Only ETH_SS_STATS has strings (see get_sset_count) */
	if (stringset != ETH_SS_STATS || port >= priv->num_ports)
		return;

	for (i = 0; i < priv->num_mib_counters; i++) {
		mib = &priv->mib_counters[i];
		strscpy_pad(data + i * ETH_GSTRING_LEN,
			    mib->name, ETH_GSTRING_LEN);
	}
}

static void rtl8372n_get_ethtool_stats(struct dsa_switch *ds, int port, uint64_t *data)
{
	const struct rtl837x_mib_counter *mib;
	struct realtek_priv *priv = ds->priv;
	int ret;

	if (port >= priv->num_ports)
		return;

	mutex_lock(&priv->mib_lock);
	for (int i = 0; i < priv->num_mib_counters; i++) {
		u64 mibvalue = 0;

		mib = &priv->mib_counters[i];
		ret = priv->ops->get_mib_counter(priv, port, mib, &mibvalue);
		if (ret) {
			dev_err(priv->dev, "Error reading MIB counter %s\n",
				mib->name);
		}
		data[i] = mibvalue;
	}
	mutex_unlock(&priv->mib_lock);
}

static int rtl8372n_get_sset_count(struct dsa_switch *ds, int port, int sset)
{
	struct realtek_priv *priv = ds->priv;

	/* We only support SS_STATS */
	if (sset != ETH_SS_STATS)
		return 0;
	if (port >= priv->num_ports)
		return -EINVAL;

	return priv->num_mib_counters;
}

static void rtl8372n_get_phy_stats(struct dsa_switch *ds, int port,
				   struct ethtool_eth_phy_stats *phy_stats)
{
	const struct rtl837x_mib_counter *mib;
	struct realtek_priv *priv = ds->priv;

	mib = &priv->mib_counters[RTL837X_MIB_dot3StatsSymbolErrors];

	mutex_lock(&priv->mib_lock);
	priv->ops->get_mib_counter(priv, port, mib,
				   &phy_stats->SymbolErrorDuringCarrier);
	mutex_unlock(&priv->mib_lock);
}

static void rtl8372n_get_mac_stats(struct dsa_switch *ds, int port,
				   struct ethtool_eth_mac_stats *mac_stats)
{
	u64 cnt[RTL837X_MIB_END] = {
		[RTL837X_MIB_ifOutOctets] = 1,
		[RTL837X_MIB_ifOutMulticastPkts] = 1,
		[RTL837X_MIB_ifOutBroadcastPkts] = 1,
		[RTL837X_MIB_falseCarrieimes] = 1,
		[RTL837X_MIB_ifInOctets] = 1,
		[RTL837X_MIB_ifInMulticastPkts] = 1,
		[RTL837X_MIB_ifInBroadcastPkts] = 1,
		[RTL837X_MIB_dot3StatsSingleCollisionFrames] = 1,
		[RTL837X_MIB_dot3StatsMultipleCollisionFrames] = 1,
		[RTL837X_MIB_framingErrors] = 1,
		[RTL837X_MIB_dot3StatsDeferredTransmissions] = 1,
		[RTL837X_MIB_dot3StatsLateCollisions] = 1,
		[RTL837X_MIB_dot3StatsExcessiveCollisions] = 1,
		[RTL837X_MIB_TxGoodCnt] = 1,
		[RTL837X_MIB_RxGoodCnt] = 1,
	};

	struct realtek_priv *priv = ds->priv;
	int ret;
	int i;

	mutex_lock(&priv->mib_lock);
	for (i = 0; i < RTL837X_MIB_END; i++) {
		const struct rtl837x_mib_counter *mib = &priv->mib_counters[i];

		/* Only fetch required MIB counters (marked = 1 above) */
		if (!cnt[i])
			continue;

		ret = priv->ops->get_mib_counter(priv, port, mib, &cnt[i]);
		if (ret)
			break;
	}
	mutex_unlock(&priv->mib_lock);

	mac_stats->FramesTransmittedOK = cnt[RTL837X_MIB_TxGoodCnt];
	mac_stats->SingleCollisionFrames =
		cnt[RTL837X_MIB_dot3StatsSingleCollisionFrames];
	mac_stats->MultipleCollisionFrames =
		cnt[RTL837X_MIB_dot3StatsMultipleCollisionFrames];
	mac_stats->FramesReceivedOK = cnt[RTL837X_MIB_RxGoodCnt];
	mac_stats->FrameCheckSequenceErrors =
		cnt[RTL837X_MIB_framingErrors];
	mac_stats->OctetsTransmittedOK = cnt[RTL837X_MIB_ifOutOctets] -
					 18 * mac_stats->FramesTransmittedOK;
	mac_stats->FramesWithDeferredXmissions =
		cnt[RTL837X_MIB_dot3StatsDeferredTransmissions];
	mac_stats->LateCollisions = cnt[RTL837X_MIB_dot3StatsLateCollisions];
	mac_stats->FramesAbortedDueToXSColls =
		cnt[RTL837X_MIB_dot3StatsExcessiveCollisions];
	mac_stats->CarrierSenseErrors = cnt[RTL837X_MIB_falseCarrieimes];
	mac_stats->OctetsReceivedOK = cnt[RTL837X_MIB_ifInOctets] -
				      18 * mac_stats->FramesReceivedOK;
	mac_stats->MulticastFramesXmittedOK =
		cnt[RTL837X_MIB_ifOutMulticastPkts];
	mac_stats->BroadcastFramesXmittedOK =
		cnt[RTL837X_MIB_ifOutBroadcastPkts];
	mac_stats->MulticastFramesReceivedOK =
		cnt[RTL837X_MIB_ifInMulticastPkts];
	mac_stats->BroadcastFramesReceivedOK =
		cnt[RTL837X_MIB_ifInBroadcastPkts];
}

static void rtl8372n_get_ctrl_stats(struct dsa_switch *ds, int port,
				    struct ethtool_eth_ctrl_stats *ctrl_stats)
{
	const struct rtl837x_mib_counter *mib;
	struct realtek_priv *priv = ds->priv;

	mib = &priv->mib_counters[RTL837X_MIB_dot3ControlInUnknownOpcodes];

	mutex_lock(&priv->mib_lock);
	priv->ops->get_mib_counter(priv, port, mib,
			  &ctrl_stats->UnsupportedOpcodesReceived);
	mutex_unlock(&priv->mib_lock);
}

static int rtl8372n_port_vlan_filtering(struct dsa_switch *ds, int port,
					bool vlan_filtering, struct netlink_ext_ack *extack)
{
	enum rtl837x_frame_ingress prev_accepted_frame, accepted_frame;
	enum rtl837x_vlan_egress_mode mode;
	struct rtl8372n *chip_data;
	struct realtek_priv *priv = ds->priv;
	u32 configured_ports = 0;
	struct dsa_port *dp;
	int ret;

	chip_data = priv->chip_data;

	dev_dbg(priv->dev, "[%s]: port (%d), filtering: %s\n", __func__,
		port, !!vlan_filtering ? "true" : "false");

	ret = rtl837x_vlan_port_get_framefilter(priv, port, &prev_accepted_frame);
	if (ret) {
		NL_SET_ERR_MSG_MOD(extack,
				   "Failed to get current framefilter");
		return ret;
	}

	/* While filtering, only accepts untagged frames if PVID is enabled */
	accepted_frame = RTL837X_FRAME_TYPE_ANY_FRAME;
	if (vlan_filtering)
		accepted_frame = chip_data->pvid_enabled[port] ?
				    RTL837X_FRAME_TYPE_ANY_FRAME : RTL837X_FRAME_TYPE_TAGGED_ONLY;

	/* When vlan filter is enable/disabled in a bridge, this function is
	 * called for all member ports. We need to enable/disable ingress
	 * VLAN membership check.
	 */
	ret = rtl837x_vlan_port_set_ingress_filtering(priv, port, vlan_filtering);
	if (ret)
		return ret;

	/* However, we also enable/disable egress filtering because the switch
	 * still consider the egress interface VLAN membership to forward the
	 * traffic. We enable/disable that check disabling/enabling transparent
	 * VLAN between the ingress port and all other available ports.
	 */
	dsa_switch_for_each_available_port(dp, ds) {
		/* port isolation will still keep traffic inside the bridge */
		ret = rtl837x_vlan_port_set_transparent(priv, port, dp->index,
							!vlan_filtering);
		if (ret)
			goto undo_transparent;

		configured_ports |= BIT(dp->index);
	}

	if (accepted_frame != prev_accepted_frame) {
		ret = rtl837x_vlan_port_set_framefilter(priv, port,
							accepted_frame);
		if (ret) {
			NL_SET_ERR_MSG_MOD(extack,
					   "Failed to set port framefilter");
			goto undo_transparent;
		}
	}

	/* When VLAN filtering is disabled, preserve frames exactly as received.
	 * Otherwise, the VLAN egress pipeline may still alter tag state
	 * according to VLAN membership and untag configuration.
	 */
	if (vlan_filtering)
		mode = RTL837X_VLAN_EGRESS_MODE_ORIGINAL;
	else
		mode = RTL837X_VLAN_EGRESS_MODE_REAL_KEEP;

	ret = rtl837x_vlan_port_set_vlan_egress_mode(priv, port, mode);
	if (ret)
		goto undo_set_framefilter;

	return ret;

undo_set_framefilter:
	if (prev_accepted_frame != accepted_frame)
		rtl837x_vlan_port_set_framefilter(priv, port,
						  prev_accepted_frame);
undo_transparent:
	/* The DSA core guarantees this callback is only invoked on an actual
	 * state transition, ensuring the previous hardware state was the
	 * opposite (!vlan_filtering). It is also called during setup but, in
	 * that case, any failure here aborts the entire switch initialization.
	 *
	 * VLAN_INGRESS and VLAN_EGRESS_TRANSPARENT states are directly derived
	 * from vlan_filtering. That way, we can simply undo it without
	 * checking the current HW state as we do with VLAN_EGRESS_MODE.
	 */
	dsa_switch_for_each_port(dp, ds) {
		if (configured_ports & BIT(dp->index))
			rtl837x_vlan_port_set_transparent(priv, port, dp->index,
							  vlan_filtering);
	}

	rtl837x_vlan_port_set_ingress_filtering(priv, port, !vlan_filtering);

	return ret;
}

/*
 * The well-known bridge group address 01:80:C2:00:00:00 is treated by the
 * hardware as an ordinary L2 multicast address, so BPDUs received on one port
 * are flooded to every other member port of the bridge instead of being
 * terminated locally.
 *
 * Steer them to the CPU port only, per VLAN, by installing an L2 multicast
 * entry whose member port mask holds nothing but the CPU port.
 *
 * An L2 multicast entry is used instead of an RMA rule because the RMA actions
 * (forward, trap to CPU, drop, forward excluding CPU) cannot express "deliver
 * to the CPU and to no other port", whereas the member port mask of an L2
 * multicast entry can.
 */
static int rtl8372n_set_bpdu_l2mc_cpu_trap(struct realtek_priv *priv, u32 vid,
					   u32 cpu_port_mask, bool add)
{
	struct rtl837x_lut_entry entry = {0};
	const u8 bpdu[] = { 0x01, 0x80, 0xC2, 0x00, 0x00, 0x00 };
	int ret;

	entry.type = RTL837X_LUT_TYPE_L2_MC;
	memcpy(entry.mc.key.mac_addr, bpdu, ETH_ALEN);
	entry.mc.key.ivl = true;
	entry.mc.key.vid_fid = vid;

	if (add) {
		entry.mc.mbr = (u16)cpu_port_mask;
		return rtl837x_lut_set(priv, &entry);
	}

	ret = rtl837x_lut_query(priv, RTL837X_LUT_METHOD_MAC, &entry);
	if (ret == -ENOENT)
		return 0;
	if (ret)
		return ret;

	return rtl837x_lut_del(priv, entry.addr);
}

static int rtl8372n_vlan_add(struct dsa_switch *ds, int port,
			     const struct switchdev_obj_port_vlan *vlan,
			    struct netlink_ext_ack *extack)
{
	struct rtl837x_vlan_data vlan_data_bak;
	struct realtek_priv *priv = ds->priv;
	bool untagged = !!(vlan->flags & BRIDGE_VLAN_INFO_UNTAGGED);
	u32 member = 0, untag = 0;
	bool pvid = !!(vlan->flags & BRIDGE_VLAN_INFO_PVID);
	u16 vid = vlan->vid;
	int ret;

	if (vid == 0) {
		NL_SET_ERR_MSG_MOD(extack, "VLAN 0 is reserved by this driver");
		return -EOPNOTSUPP;
	}

	if (vid_is_dsa_8021q(vid) && is_8021q_tag(priv->tag_proto)) {
		NL_SET_ERR_MSG_MOD(extack, "Range 3072-4095 reserved for dsa_8021q operation");
		return -EINVAL;
	}

	member |= BIT(port);

	if (untagged)
		untag |= BIT(port);

	dev_dbg(priv->dev, "[%s] add VLAN %d on port %d, %s, %s\n", __func__,
		vlan->vid, port, untagged ? "untagged" : "tagged",
		pvid ? "PVID" : "no PVID");

	ret = rtl837x_vlan_get(priv, vid, &vlan_data_bak);
	if (ret)
		return ret;

	ret = rtl8372n_vlan_update(priv, vid, member, untag, 0);
	if (ret) {
		dev_err(priv->dev, "failed to set up VLAN %04x\n",
			vid);
		goto fail_rollback;
	}

	/* Add bpdu trap rule for vlan */
	ret = rtl8372n_set_bpdu_l2mc_cpu_trap(priv, vid,
					      dsa_cpu_ports(priv->ds), true);
	if (ret)
		dev_warn(priv->dev,
			 "failed to install BPDU trap for VLAN %04x: %d\n",
			 vid, ret);

	if (pvid) {
		ret = rtl8372n_set_pvid(priv, port, vid);
		if (ret) {
			dev_err(priv->dev, "failed to set PVID on port %d to VLAN %04x\n",
				port, vid);
			goto fail_rollback;
		}
	} else {
		ret = rtl8372n_clean_pvid(priv, port, vid);
		if (ret) {
			dev_err(priv->dev, "failed to clean PVID on port %d to VLAN %04x\n",
				port, vid);
			goto fail_rollback;
		}
	}

	return 0;

fail_rollback:
	{
		int r = rtl837x_vlan_set(priv, vid, &vlan_data_bak);

		if (r)
			dev_err(priv->dev,
				"failed to restore VLAN %04x: %d\n",
				vid, r);
	}
	return ret;
}

static int rtl8372n_vlan_del(struct dsa_switch *ds, int port,
			     const struct switchdev_obj_port_vlan *vlan)
{
	struct rtl837x_vlan_data vlan_data;
	struct realtek_priv *priv = ds->priv;
	int ret;

	dev_dbg(priv->dev, "[%s]: port (%d) vid (%d)\n", __func__,
		port, vlan->vid);

	ret = rtl8372n_clean_pvid(priv, port, vlan->vid);
	if (ret) {
		dev_err(priv->dev, "[%s] failed to clean PVID on port %d to VLAN %04x\n", __func__,
			port, vlan->vid);
		return ret;
	}

	ret = rtl837x_vlan_get(priv, vlan->vid, &vlan_data);
	if (ret)
		return ret;

	vlan_data.mbr &= ~BIT(port);
	vlan_data.untag &= ~BIT(port);
	vlan_data.ivl_en = 1;

	if (!vlan_data.mbr) {
		vlan_data.untag = 0;
		vlan_data.fid = 0;
	}

	ret = rtl837x_vlan_set(priv, vlan->vid, &vlan_data);
	if (ret) {
		dev_err(priv->dev,
			"failed to remove VLAN %04x\n",
			vlan->vid);
		return ret;
	}

	/* The VLAN is really gone now, drop the BPDU trap rule with it */
	if (!vlan_data.mbr) {
		ret = rtl8372n_set_bpdu_l2mc_cpu_trap(priv, vlan->vid, 0, false);
		if (ret)
			dev_warn(priv->dev,
				 "failed to remove BPDU trap for VLAN %04x: %d\n",
				 vlan->vid, ret);
	}

	return 0;
}

static int rtl8372n_port_set_ucast_flood(struct realtek_priv *priv, int port,
					 bool enable)
{
	return rtl837x_reg_bits_write(priv, RTL837X_FLOOD_UNKNOWN_UC_REG,
				      BIT(port), enable);
}

/* Unknown L2 multicast as well as unknown IPv4 and IPv6 multicast */
static int rtl8372n_port_set_mcast_flood(struct realtek_priv *priv, int port,
					 bool enable)
{
	static const u32 regs[] = {
		RTL837X_FLOOD_UNKNOWN_L2MC_REG,
		RTL837X_FLOOD_UNKNOWN_IP4MC_REG,
		RTL837X_FLOOD_UNKNOWN_IP6MC_REG,
	};
	int ret;
	int i;

	for (i = 0; i < ARRAY_SIZE(regs); i++) {
		ret = rtl837x_reg_bits_write(priv, regs[i], BIT(port), enable);
		if (ret)
			return ret;
	}

	return 0;
}

static int rtl8372n_port_set_bcast_flood(struct realtek_priv *priv, int port,
					 bool enable)
{
	return rtl837x_reg_bits_write(priv, RTL837X_FLOOD_BCAST_REG,
				      BIT(port), enable);
}

static int rtl8372n_port_change_mtu(struct dsa_switch *ds, int port,
				    int new_mtu)
{
	struct realtek_priv *priv = ds->priv;
	u32 len = new_mtu + VLAN_ETH_HLEN + ETH_FCS_LEN;

	if (len > RTL837X_MAC_MAX_LEN_MAX)
		return -EINVAL;

	return regmap_update_bits(priv->map, RTL837X_MAC_MAX_LEN_REG(port),
				  RTL837X_MAC_MAX_LEN_LO_MASK |
				  RTL837X_MAC_MAX_LEN_HI_MASK,
				  FIELD_PREP(RTL837X_MAC_MAX_LEN_LO_MASK, len) |
				  FIELD_PREP(RTL837X_MAC_MAX_LEN_HI_MASK, len));
}

static int rtl8372n_port_max_mtu(struct dsa_switch *ds, int port)
{
	return RTL837X_MAC_MAX_LEN_MAX - VLAN_ETH_HLEN - ETH_FCS_LEN;
}

static int rtl8372n_port_pre_bridge_flags(struct dsa_switch *ds, int port,
					  struct switchdev_brport_flags flags,
					   struct netlink_ext_ack *extack)
{
	struct realtek_priv *priv = ds->priv;

	dev_dbg(priv->dev, "[%s] port:%d flags:%lx supported:%lx\n", __func__,
		port, flags.mask, RTL8372N_SUPPORTED_BRIDGE_FLAGS);

	if (flags.mask & ~RTL8372N_SUPPORTED_BRIDGE_FLAGS)
		return -EINVAL;

	return 0;
}

static int rtl8372n_port_bridge_flags(struct dsa_switch *ds, int port,
				      struct switchdev_brport_flags flags,
			      struct netlink_ext_ack *extack)
{
	struct realtek_priv *priv = ds->priv;
	int ret;

	if (flags.mask & BR_LEARNING) {
		ret = rtl837x_lut_port_set_learning(priv, port,
						    !!(flags.val & BR_LEARNING));
		if (ret)
			return ret;
	}

	if (flags.mask & BR_FLOOD) {
		ret = rtl8372n_port_set_ucast_flood(priv, port,
						    !!(flags.val & BR_FLOOD));
		if (ret)
			return ret;
	}

	if (flags.mask & BR_MCAST_FLOOD) {
		ret = rtl8372n_port_set_mcast_flood(priv, port,
						    !!(flags.val & BR_MCAST_FLOOD));
		if (ret)
			return ret;
	}

	if (flags.mask & BR_BCAST_FLOOD) {
		ret = rtl8372n_port_set_bcast_flood(priv, port,
						    !!(flags.val & BR_BCAST_FLOOD));
		if (ret)
			return ret;
	}

	return 0;
}

static int
rtl8372n_port_bridge_join(struct dsa_switch *ds, int port,
			  struct dsa_bridge bridge,
			   bool *tx_fwd_offload,
			   struct netlink_ext_ack *extack)
{
	struct realtek_priv *priv = ds->priv;
	struct dsa_port *dp;
	u32 port_bitmap = 0;
	int ret;

	/* Loop over all other ports than the current one */
	dsa_switch_for_each_user_port(dp, ds) {
		/* Not on this bridge */
		if (!dsa_port_offloads_bridge(dp, &bridge))
			continue;

		/* Current port handled last */
		if (dp->index == port)
			continue;

		/* Join this port to each other port on the bridge */
		ret = rtl8372n_port_add_isolation(priv, dp->index, BIT(port));
		if (ret) {
			dev_err(priv->dev,
				"failed to join port %d to port %d: %d\n",
				dp->index, port, ret);
			goto fail_rollback;
		}

		port_bitmap |= BIT(dp->index);
	}
	dev_dbg(priv->dev, "[%s]: port(%d) isolate(0x%04x)\n", __func__,
		port, port_bitmap);

	/* Set the bits for the ports we can access */
	ret = rtl8372n_port_add_isolation(priv, port, port_bitmap);
	if (ret) {
		dev_err(priv->dev, "failed to join port %d: %d\n", port, ret);
		goto fail_rollback;
	}

	ret = rtl837x_lut_port_set_learning(priv, port, true);
	if (ret)
		goto fail_rollback;

	return 0;

fail_rollback:
	{
		int r;
		/* Drop the access rights of this port */
		r = rtl8372n_port_remove_isolation(priv, port, port_bitmap);
		if (r)
			dev_err(priv->dev,
				"failed to drop access rights of port %d: %d\n",
				port, r);

		/* And drop the access rights other ports gained towards us */
		dsa_switch_for_each_user_port(dp, ds) {
			if (!(port_bitmap & BIT(dp->index)))
				continue;

			r = rtl8372n_port_remove_isolation(priv, dp->index, BIT(port));
			if (r)
				dev_err(priv->dev,
					"failed to drop port %d access to port %d: %d\n",
					dp->index, port, r);
		}
	}
	return ret;
}

static void
rtl8372n_port_bridge_leave(struct dsa_switch *ds, int port,
			   struct dsa_bridge bridge)
{
	struct realtek_priv *priv = ds->priv;
	struct dsa_port *dp;
	u32 port_bitmap = 0;
	int ret;

	dev_dbg(priv->dev, "[%s]: %d\n", __func__,
		port);

	/* Loop over all other ports than this one */
	dsa_switch_for_each_user_port(dp, ds) {
		/* Not on this bridge */
		if (!dsa_port_offloads_bridge(dp, &bridge))
			continue;

		/* Current port handled last */
		if (dp->index == port)
			continue;

		/* Remove this port from any other port on the bridge */
		ret = rtl8372n_port_remove_isolation(priv, dp->index,
						     BIT(port));
		if (ret) {
			dev_err(priv->dev,
				"failed to leave port %d from port %d: %d\n",
				dp->index, port, ret);
		}

		port_bitmap |= BIT(dp->index);
	}
	/* Clear the bits for the ports we can not access, leave ourselves */
	ret = rtl8372n_port_remove_isolation(priv, port, port_bitmap);
	if (ret)
		dev_err(priv->dev, "failed to remove port(%d) isolation err: %d\n", port, ret);

	ret = rtl837x_lut_port_set_learning(priv, port, false);
	if (ret)
		dev_err(priv->dev,
			"failed to disable learning on port %d: %pe\n",
			port, ERR_PTR(ret));
}

static int rtl8372n_set_mac_eee(struct dsa_switch *ds, int port, struct ethtool_keee *eee)
{
	struct realtek_priv *priv = ds->priv;

	dev_dbg(priv->dev, "[%s]: port(%d) eee_enable:(%s)\n", __func__,
		port, eee->eee_enabled ? "true" : "false");

	if (eee->eee_enabled)
		return rtl837x_reg_bits_write(priv, RTL837X_EEE_CTRL_REG(port),
				  RTL837X_EEE_CTRL_TX_EN_MASK | RTL837X_EEE_CTRL_RX_EN_MASK,
				  0xffffffff
				);
	else
		return rtl837x_reg_bits_write(priv, RTL837X_EEE_CTRL_REG(port),
				  RTL837X_EEE_CTRL_TX_EN_MASK | RTL837X_EEE_CTRL_RX_EN_MASK,
				  0
				);
	return 0;
}

static int
rtl8372n_port_fdb_static_add(struct realtek_priv *priv, int port,
			     const unsigned char *addr, u16 vid)
{
	struct rtl837x_lut_entry entry = {0};
	int ret;

	memcpy(entry.uc.key.mac_addr, addr, ETH_ALEN);
	entry.type = RTL837X_LUT_TYPE_L2_UC;
	entry.uc.key.vid_fid = vid;
	entry.uc.key.ivl = true;

	// age should greater than 0, so that the static entry will not be flushed by hardware
	entry.uc.age = 6;
	entry.uc.port = port;
	entry.uc.is_static = true;

	ret = rtl837x_lut_set(priv, &entry);
	if (ret == -ENOENT) {
		dev_dbg(priv->dev,
			"[%s]:addfailed mac:%pM port:%d vid:%04d\n",
			__func__, addr, port, vid);
		return -ENOSPC;
	}
	dev_dbg(priv->dev, "[%s]:addsucceed mac:%pM port:%d vid:%04d addr: %04d\n",
		__func__, addr, port, vid, entry.addr);
	return ret;
}

static int
rtl8372n_port_fdb_static_del(struct realtek_priv *priv,
			     const unsigned char *addr, u16 vid)
{
	struct rtl837x_lut_entry entry = {0};
	int ret;

	memcpy(entry.uc.key.mac_addr, addr, ETH_ALEN);
	entry.type = RTL837X_LUT_TYPE_L2_UC;
	entry.uc.key.vid_fid = vid;
	entry.uc.key.ivl = true;

	// query the entry in lut table
	ret = rtl837x_lut_query(priv, RTL837X_LUT_METHOD_MAC, &entry);
	if (ret == -ENOENT) {
		/* Not found in the LUT table, just return success */
		dev_dbg(priv->dev, "[%s]:notfound mac:%pM vid:%04d\n",
			__func__, addr, vid);
		return 0;
	}
	/* Any other error leaves entry.addr unset: do not delete index 0 */
	if (ret)
		return ret;

	dev_dbg(priv->dev, "[%s]:deleted mac:%pM vid:%04d addr: %04d\n",
		__func__, addr, vid, entry.addr);
	return rtl837x_lut_del(priv, entry.addr);
}

static int
rtl8372n_port_fdb_add(struct dsa_switch *ds, int port,
		      const unsigned char *addr, u16 vid,
		    struct dsa_db db)
{
	struct realtek_priv *priv = ds->priv;
	int ret;

	if (db.type != DSA_DB_PORT && db.type != DSA_DB_BRIDGE)
		return -EOPNOTSUPP;

	dev_dbg(priv->dev, "[%s]:type: %s port:%d vid:%04d\n", __func__,
		db.type == DSA_DB_PORT ? "DSA_DB_PORT" : "DSA_DB_BRIDGE",
					port, vid);

	if (db.type == DSA_DB_BRIDGE && is_8021q_tag(priv->tag_proto)) {
		/*
		 * When the DSA tag protocol is DSA_TAG_PROTO_MXL862_8021Q, the L2 VLAN
		 * learned on the CPU port is the 802.1q DSA TAG rather than the port's own
		 * VLAN ID.
		 * We need add the correct fdb entry
		 */
		if (vid == 0) {
			struct dsa_port *dp;

			dsa_switch_for_each_user_port(dp, ds) {
				/* Add static fdb entry */
				vid = dsa_tag_8021q_standalone_vid(dp);
				ret = rtl8372n_port_fdb_static_add(priv, port,
								   addr, vid);
				if (ret)
					return ret;
			}
		} else {
			ret = rtl8372n_port_fdb_static_add(priv, port, addr,
							   vid);
			if (ret)
				return ret;
		}
	} else {
		return rtl8372n_port_fdb_static_add(priv, port, addr, vid);
	}
	return 0;
}

static int
rtl8372n_port_fdb_del(struct dsa_switch *ds, int port,
		      const unsigned char *addr, u16 vid,
		    struct dsa_db db)
{
	struct realtek_priv *priv = ds->priv;
	int ret;

	if (db.type != DSA_DB_PORT && db.type != DSA_DB_BRIDGE)
		return -EOPNOTSUPP;

	if (dsa_fdb_present_in_other_db(ds, port, addr, vid, db))
		return 0;

	if (db.type == DSA_DB_BRIDGE && is_8021q_tag(priv->tag_proto)) {
		if (vid == 0) {
			struct dsa_port *dp;

			dsa_switch_for_each_user_port(dp, ds) {
				/* Del static fdb entry */
				vid = dsa_tag_8021q_standalone_vid(dp);
				ret = rtl8372n_port_fdb_static_del(priv, addr,
								   vid);
				if (ret)
					return ret;
			}
		} else {
			return rtl8372n_port_fdb_static_del(priv, addr, vid);
		}
	} else {
		return rtl8372n_port_fdb_static_del(priv, addr, vid);
	}
	return 0;
}

static int
rtl8372n_port_fdb_dump(struct dsa_switch *ds, int port,
		       dsa_fdb_dump_cb_t *cb, void *data)
{
	struct rtl837x_lut_entry entry = {0};
	struct realtek_priv *priv = ds->priv;
	int ret;

	for (int i = 0; i < RTL837X_LUT_MAX; i++) {
		entry.addr = i;
		entry.uc.port = port;
		ret = rtl837x_lut_query(priv, RTL837X_LUT_METHOD_NEXT_L2UCSPA, &entry);
		/* No (further) entry: the dump is complete */
		if (ret == -ENOENT)
			return 0;
		if (ret)
			return ret;
		if (entry.addr < i)
			break;
		/* We need to hide the dsa_8021q VLANs from the user. */
		if (vid_is_dsa_8021q(entry.uc.key.vid_fid) && is_8021q_tag(priv->tag_proto))
			entry.uc.key.vid_fid = 0;
		ret = cb(entry.uc.key.mac_addr, entry.uc.key.vid_fid, entry.uc.is_static,
			 data);
		/* Pass errors on: -EMSGSIZE makes DSA continue in a new message */
		if (ret)
			return ret;
		i = entry.addr;
	}
	return 0;
}

static int
rtl8372n_port_mdb_add(struct dsa_switch *ds, int port,
		      const struct switchdev_obj_port_mdb *mdb,
		    struct dsa_db db)
{
	struct rtl837x_lut_entry entry = {0};
	struct realtek_priv *priv = ds->priv;
	const u8 *addr = mdb->addr;
	u16 vid = mdb->vid;
	int ret;

	memcpy(entry.mc.key.mac_addr, addr, ETH_ALEN);
	entry.type = RTL837X_LUT_TYPE_L2_MC;
	entry.mc.key.vid_fid = vid;
	entry.mc.key.ivl = true;
	ret = rtl837x_lut_query(priv, RTL837X_LUT_METHOD_MAC, &entry);
	if (ret != -ENOENT && ret != 0)
		return ret;

	if (ret == 0) {
		dev_dbg(priv->dev,
			"[%s]:found mac:%pM port:%d vid:%04d addr:%04d\n",
			__func__, addr, port, vid, entry.addr);
		if (entry.type != RTL837X_LUT_TYPE_L2_MC) {
			dev_err(priv->dev,
				"[%s]: got an unexpected entry from LUT table\n",
				__func__);
			return -EINVAL;
		}
	}

	entry.mc.mbr |= BIT(port);
	ret = rtl837x_lut_set(priv, &entry);
	if (ret == -ENOENT) {
		dev_dbg(priv->dev,
			"[%s]:addfailed mac:%pM port:%d vid:%04d\n",
			__func__, addr, port, vid);
		return -ENOSPC;
	}
	dev_dbg(priv->dev, "[%s]:addsucceed mac:%pM port:%d vid:%04d addr: %04d\n",
		__func__, addr, port, vid, entry.addr);
	return ret;
}

static int
rtl8372n_port_mdb_del(struct dsa_switch *ds, int port,
		      const struct switchdev_obj_port_mdb *mdb,
		    struct dsa_db db)
{
	struct rtl837x_lut_entry entry = {0};
	struct realtek_priv *priv = ds->priv;
	const u8 *addr = mdb->addr;
	u16 vid = mdb->vid;
	int ret;

	dev_dbg(priv->dev, "[%s]:mac:%pM port:%d vid:%04d\n",
		__func__, addr, port, vid);

	memcpy(entry.mc.key.mac_addr, addr, ETH_ALEN);
	entry.type = RTL837X_LUT_TYPE_L2_MC;
	entry.mc.key.vid_fid = vid;
	entry.mc.key.ivl = true;
	ret = rtl837x_lut_query(priv, RTL837X_LUT_METHOD_MAC, &entry);
	if (ret == -ENOENT)
		return 0;

	if (ret == 0) {
		dev_dbg(priv->dev,
			"[%s]:found mac:%pM port:%d vid:%04d addr:%04d\n",
			__func__, addr, port, vid, entry.addr);
		if (entry.type != RTL837X_LUT_TYPE_L2_MC) {
			dev_err(priv->dev,
				"[%s]: got an unexpected entry from LUT table\n",
				__func__);
			return -EINVAL;
		}
	} else {
		return ret;
	}

	entry.mc.mbr &= ~BIT(port);
	if (!entry.mc.mbr)
		return rtl837x_lut_del(priv, entry.addr);

	ret = rtl837x_lut_set(priv, &entry);
	if (ret == -ENOENT) {
		dev_dbg(priv->dev,
			"[%s]:delfailed mac:%pM port:%d vid:%04d\n",
			__func__, addr, port, vid);
		return -ENOSPC;
	}
	dev_dbg(priv->dev, "[%s]:delsucceed mac:%pM port:%d vid:%04d addr: %04d\n",
		__func__, addr, port, vid, entry.addr);
	return ret;
}

static void rtl8372n_port_fast_age(struct dsa_switch *ds, int port)
{
	struct realtek_priv *priv = ds->priv;

	(void)rtl837x_lut_l2_flush(priv, RTL837X_l2_FLUSH_MODE_PORT,
			    port, 0, 0);
}

enum RTL837X_MSTP_STATE {
	MSTP_DISABLE = 0,
	MSTP_BLOCKING,
	MSTP_LEARNING,
	MSTP_FORWARDING,
	MSTP_END
};

static void rtl8372n_port_stp_state_set(struct dsa_switch *ds, int port, u8 state)
{
	struct realtek_priv *priv = ds->priv;
	u32 val;

	dev_dbg(priv->dev, "[%s]: port(%d) stp_status(%d)\n", __func__,
		port, state);

	switch (state) {
	case BR_STATE_DISABLED:
		val = MSTP_DISABLE;
		break;
	case BR_STATE_BLOCKING:
	case BR_STATE_LISTENING:
		val = MSTP_BLOCKING;
		break;
	case BR_STATE_LEARNING:
		val = MSTP_LEARNING;
		break;
	case BR_STATE_FORWARDING:
		val = MSTP_FORWARDING;
		break;
	default:
		dev_err(priv->dev, "[%s]: unknown bridge state requested\n", __func__);
		return;
	}

	/* Set the same status for the port on all the FIDs */
	for (int i = 0; i < RTL837x_FIDMAX; i++) {
		rtl837x_reg_bits_write(priv, RTL837X_MSTI_CTRL_REG(i),
				       RTL837X_MSTI_CTRL_PORT_STATE_MASK(port), val
				);
	}
}

static int rtl8372n_of_setup_phy_and_serdes(struct realtek_priv *priv)
{
	struct device_node *np = priv->dev->of_node;
	int ret;

	if (of_property_read_bool(np, "sds0-rx-swap"))
		ret = rtl837x_serdes_pnswap_rx(priv, 0, true);
	else
		ret = rtl837x_serdes_pnswap_rx(priv, 0, false);
	if (ret)
		return ret;

	if (of_property_read_bool(np, "sds0-tx-swap"))
		ret = rtl837x_serdes_pnswap_tx(priv, 0, true);
	else
		ret = rtl837x_serdes_pnswap_tx(priv, 0, false);
	if (ret)
		return ret;

	if (of_property_read_bool(np, "sds1-rx-swap"))
		ret = rtl837x_serdes_pnswap_rx(priv, 1, true);
	else
		ret = rtl837x_serdes_pnswap_rx(priv, 1, false);
	if (ret)
		return ret;

	if (of_property_read_bool(np, "sds1-tx-swap"))
		ret = rtl837x_serdes_pnswap_tx(priv, 1, true);
	else
		ret = rtl837x_serdes_pnswap_tx(priv, 1, false);
	if (ret)
		return ret;

	/*
	 * What The Fuck Is This???????
	 * I can't understand this register design.
	 *
	 * We have two PCB routing
	 *
	 * RJ45 port pin mapping:
	 *  white Orange ->  0+
	 *        Orange ->  0-
	 *  white Green  ->  1+
	 *        Blue   ->  2+
	 *  white Blue   ->  2-
	 *        Green  ->  1-
	 *  white Brown  ->  3+
	 *        Brown  ->  3-
	 * Chip side:
	 *    A+ A- B+ B- C+ C- D+ D-
	 *
	 * Case 1: No phy-mdi-reverse and no phy-tx-polarity-swap (normal connection)
	 *   A+ → 0+   A- → 0-   (Pair A → RJ45 Pair 0)
	 *   B+ → 1+   B- → 1-   (Pair B → RJ45 Pair 1)
	 *   C+ → 2+   C- → 2-   (Pair C → RJ45 Pair 2)
	 *   D+ → 3+   D- → 3-   (Pair D → RJ45 Pair 3)
	 * Case 2: With phy-mdi-reverse and phy-tx-polarity-swap
	 *   A+ → 3+   A- → 3-   (Pair A → RJ45 Pair 3)
	 *   B+ → 2+   B- → 2-   (Pair B → RJ45 Pair 2)
	 *   C+ → 1-   C- → 1+   (Pair C → RJ45 Pair 1, with polarity REVERSED)
	 *   D+ → 0-   D- → 0+   (Pair D → RJ45 Pair 0, with polarity REVERSED)
	 */

	if (of_property_read_bool(np, "phy-mdi-reverse"))
		ret = rtl837x_reg_bits_write(priv, RTL837X_PHY_MDI_REVERSE_REG,
					     RTL837X_PHY_MDI_REVERSE_P0_MASK |
					     RTL837X_PHY_MDI_REVERSE_P1_MASK |
					     RTL837X_PHY_MDI_REVERSE_P2_MASK |
					     RTL837X_PHY_MDI_REVERSE_P3_MASK,
					     0xC);
	if (ret)
		return ret;

	if (of_property_read_bool(np, "phy-tx-polarity-swap"))
		ret = rtl837x_reg_bits_write(priv, RTL837X_PHY_TX_POLARITY_REG,
					     RTL837X_PHY_TX_POLARITY_P0_MASK |
					     RTL837X_PHY_TX_POLARITY_P1_MASK |
					     RTL837X_PHY_TX_POLARITY_P2_MASK |
					     RTL837X_PHY_TX_POLARITY_P3_MASK,
					     0x596A);
	if (ret)
		return ret;
	return 0;
}

static int rtl8372n_setup_port(struct realtek_priv *priv, int port)
{
	const struct rtl837x_chip_info *chip_info;
	struct rtl8372n_port *port_priv;
	struct rtl8372n *chip_data;
	struct ethtool_keee eee = {0};
	struct device_node *dn;
	phy_interface_t mode;
	struct dsa_port *dp = dsa_to_port(priv->ds, port);
	u32 port_type = RTL837X_SMI_MAC_TYPE_PORT;
	int ret;

	chip_data = priv->chip_data;
	port_priv = &chip_data->ports[port];
	chip_info = chip_data->chip_info;
	dn = dp->dn;

	port_priv->index = port;
	port_priv->priv = priv;
	port_priv->have_pcs = false;

	if (chip_info->port_feature[port] == RTL837X_PORT_FEATURE_NONEXIST &&
	    !dsa_port_is_unused(dp)) {
		dev_err(priv->dev, "port(%d) is not available on chip %s\n",
			port, chip_info->name);
		return -ENODEV;
	}

	if (chip_info->port_feature[port] == RTL837X_PORT_FEATURE_NONEXIST)
		return 0;

	if (of_get_phy_mode(dn, &mode)) {
		dev_warn(priv->dev,
			 "Failed to get port(%d) phy-mode, Use PHY_INTERFACE_MODE_INTERNAL by default",
		      port
			);
		mode = PHY_INTERFACE_MODE_INTERNAL;
	}

	if (mode != PHY_INTERFACE_MODE_INTERNAL) {
		if (chip_info->port_feature[port] & RTL837X_PORT_FEATURE_SERDES)
			port_type = RTL837X_SMI_MAC_TYPE_SERDES;
		if (chip_info->port_feature[port] & RTL837X_PORT_FEATURE_QSXGMII)
			port_type = RTL837X_SMI_MAC_TYPE_PORT;
	}

	ret = regmap_update_bits(priv->map, RTL837X_SMI_MAC_TYPE_REG,
				 RTL837X_SMI_MAC_TYPE_MASK(port),
				    port_type << __ffs(RTL837X_SMI_MAC_TYPE_MASK(port))
				);
	if (ret)
		return ret;

	if (!dsa_port_is_unused(dp)) {
		if (port_type == RTL837X_SMI_MAC_TYPE_PORT) {
			// TODO?: may be we can add poll phy status?
			port_priv->have_pcs = false;

			/*
			 * Configure ports to poll the phy internal status register.
			 *
			 * Will this have any effect on the RTL8372N/RTL8372? I tried
			 * disabling these bits and checked the status of the MAC
			 * and MIB registers; it did not affect the normal
			 * behavior/updates of these registers.
			 */
			ret = regmap_set_bits(priv->map, RTL837X_PORT_POLLING_REG,
					      RTL837X_PORT_POLLING_PORT_BIT(port)
						);
			if (ret)
				return ret;
		} else if (port_type == RTL837X_SMI_MAC_TYPE_SERDES) {
			port_priv->have_pcs = true;
			port_priv->pcs.ops = &rtl8372n_sds_pcs_ops;
#if  KERNEL_VERSION(6, 18, 0) >= LINUX_VERSION_CODE
			port_priv->pcs.neg_mode = true;
#endif
			port_priv->pcs.poll = true;
		}

		/* Use the polled (PHY/Serdes). */
		ret = regmap_set_bits(priv->map, RTL837X_SMI_GLB_CTRL_REG,
				      RTL837X_SMI_GLB_CTRL_POLLING_PORT_BIT(port)
			);
		if (ret)
			return ret;
	}

	/* Clean the force link bit */
	ret = regmap_clear_bits(priv->map, RTL837X_MAC_FORCE_MODE_REG(port),
				RTL837X_MAC_FORCE_MODE_FORCE_LINK_MASK);
	if (ret)
		return ret;

	ret = regmap_write(priv->map, RTL837X_FC_PORT_ACT_CTRL_REG(port), 0x1050);
	if (ret)
		return ret;

	ret = regmap_set_bits(priv->map, RTL837X_MAC_L2_PORT_CTRL_REG(port),
			      RTL837X_MAC_L2_PORT_CTRL_RX_CHK_CRC_EN_MASK |
		   RTL837X_MAC_L2_PORT_CTRL_CLOCK_SWITCH_MASK
		);
	if (ret)
		return ret;

	/* Disable learning */
	ret = rtl837x_lut_port_set_learning(priv, port, false);
	if (ret)
		return ret;

	rtl8372n_port_stp_state_set(priv->ds, port, BR_STATE_DISABLED);
	/* Start with all port completely isolated */
	ret = rtl8372n_port_set_isolation(priv, port, 0);
	if (ret)
		return ret;

	/* Disable port EEE feature by default */
	eee.eee_enabled = false;
	ret = rtl8372n_set_mac_eee(priv->ds, port, &eee);
	if (ret)
		return ret;

	/* Enable backpressure */
	ret = regmap_set_bits(priv->map, RTL837X_MAC_PORT_CTRL_REG(port),
			      RTL837X_MAC_PORT_CTRL_BKPRES_EN_MASK
			);
	if (ret)
		return ret;

	return 0;
}

static void rtl8372n_stats_update(struct realtek_priv *priv, int port)
{
	static const int idx[] = {
		RTL837X_MIB_ifInOctets,
		RTL837X_MIB_ifOutOctets,
		RTL837X_MIB_ifInUcastPkts,
		RTL837X_MIB_ifInMulticastPkts,
		RTL837X_MIB_ifInBroadcastPkts,
		RTL837X_MIB_ifOutUcastPkts,
		RTL837X_MIB_ifOutMulticastPkts,
		RTL837X_MIB_ifOutBroadcastPkts,
		RTL837X_MIB_ifOutDiscards,
		RTL837X_MIB_etherStatsDropEvents,
		RTL837X_MIB_tx_etherStatsCollisions,
		RTL837X_MIB_rx_etherStatsFragments,
		RTL837X_MIB_rx_etherStatsJabbers,
		RTL837X_MIB_rx_etherStatsCRCAlignErrors,
		RTL837X_MIB_dot3StatsLateCollisions,
	};
	struct rtl8372n *chip_data = priv->chip_data;
	struct rtl8372n_port *p = &chip_data->ports[port];
	struct rtnl_link_stats64 *stats = &p->stats;
	u64 cnt[RTL837X_MIB_END] = {};
	int ret = 0;
	int i;

	mutex_lock(&priv->mib_lock);
	for (i = 0; i < ARRAY_SIZE(idx); i++) {
		ret = priv->ops->get_mib_counter(priv, port,
						 &priv->mib_counters[idx[i]],
						 &cnt[idx[i]]);
		if (ret)
			break;
	}
	mutex_unlock(&priv->mib_lock);

	/* Keep the old values if a counter could not be read */
	if (ret)
		return;

	spin_lock(&p->stats_lock);

	stats->rx_packets = cnt[RTL837X_MIB_ifInUcastPkts] +
			    cnt[RTL837X_MIB_ifInMulticastPkts] +
			    cnt[RTL837X_MIB_ifInBroadcastPkts];
	stats->tx_packets = cnt[RTL837X_MIB_ifOutUcastPkts] +
			    cnt[RTL837X_MIB_ifOutMulticastPkts] +
			    cnt[RTL837X_MIB_ifOutBroadcastPkts];

	/* if{In,Out}Octets include the FCS */
	stats->rx_bytes = cnt[RTL837X_MIB_ifInOctets] - 4 * stats->rx_packets;
	stats->tx_bytes = cnt[RTL837X_MIB_ifOutOctets] - 4 * stats->tx_packets;

	stats->rx_dropped = cnt[RTL837X_MIB_etherStatsDropEvents];
	stats->tx_dropped = cnt[RTL837X_MIB_ifOutDiscards];
	stats->multicast = cnt[RTL837X_MIB_ifInMulticastPkts];
	stats->collisions = cnt[RTL837X_MIB_tx_etherStatsCollisions];

	stats->rx_length_errors = cnt[RTL837X_MIB_rx_etherStatsFragments] +
				  cnt[RTL837X_MIB_rx_etherStatsJabbers];
	stats->rx_crc_errors = cnt[RTL837X_MIB_rx_etherStatsCRCAlignErrors];
	stats->rx_errors = stats->rx_length_errors + stats->rx_crc_errors;

	stats->tx_aborted_errors = cnt[RTL837X_MIB_ifOutDiscards];
	stats->tx_window_errors = cnt[RTL837X_MIB_dot3StatsLateCollisions];
	stats->tx_errors = stats->tx_aborted_errors + stats->tx_window_errors;

	spin_unlock(&p->stats_lock);
}

static void rtl8372n_stats_poll(struct work_struct *work)
{
	struct rtl8372n_port *p = container_of(to_delayed_work(work),
					       struct rtl8372n_port, mib_work);

	rtl8372n_stats_update(p->priv, p->index);
	schedule_delayed_work(&p->mib_work, RTL8372N_STATS_INTERVAL_JIFFIES);
}

static void rtl8372n_get_stats64(struct dsa_switch *ds, int port,
				 struct rtnl_link_stats64 *s)
{
	struct realtek_priv *priv = ds->priv;
	struct rtl8372n *chip_data = priv->chip_data;
	struct rtl8372n_port *p = &chip_data->ports[port];

	spin_lock(&p->stats_lock);
	memcpy(s, &p->stats, sizeof(*s));
	spin_unlock(&p->stats_lock);
}

static void rtl8372n_stats_start(struct dsa_switch *ds)
{
	struct realtek_priv *priv = ds->priv;
	struct rtl8372n *chip_data = priv->chip_data;
	struct dsa_port *dp;

	/* priv and index are set in rtl8372n_setup_port() */
	dsa_switch_for_each_available_port(dp, ds) {
		struct rtl8372n_port *p = &chip_data->ports[dp->index];

		spin_lock_init(&p->stats_lock);
		INIT_DELAYED_WORK(&p->mib_work, rtl8372n_stats_poll);
		schedule_delayed_work(&p->mib_work, 0);
	}
}

static void rtl8372n_teardown(struct dsa_switch *ds)
{
	struct realtek_priv *priv = ds->priv;
	struct rtl8372n *chip_data = priv->chip_data;
	struct dsa_port *dp;

	dsa_switch_for_each_available_port(dp, ds)
		cancel_delayed_work_sync(&chip_data->ports[dp->index].mib_work);
}

// TODO: init chip based on rtl837x_chip_info
static int rtl8372n_setup(struct dsa_switch *ds)
{
	struct rtl8372n *chip_data;
	struct realtek_priv *priv = ds->priv;
	struct dsa_port *dp;
	int cpu_dp_cnt = 0;
	int ret;

	chip_data = priv->chip_data;

	dsa_switch_for_each_port(dp, ds) {
		if (dsa_port_is_dsa(dp)) {
			dev_err(priv->dev, "Cascading (DSA link) not supported\n");
			return -EOPNOTSUPP;
		}

		if (dsa_port_is_cpu(dp)) {
			cpu_dp_cnt++;
			// TODO: multi CPU port support
			if (cpu_dp_cnt > 1) {
				dev_err(priv->dev, "We only support one cpu port now\n");
				return -ENODEV;
			}

			/*
			 * Set the external CPU port.
			 * This is used to trap frames that exceed the L2 auto-learning
			 * limit, not for the DSA tag (rtl8_4).
			 * With the rtl8_4 tagger, rtl8372n_set_tag_rtl() overwrites it.
			 */
			ret = rtl837x_reg_bits_write(priv, RTL837X_EXT_CPU_CTRL_REG,
						     RTL837X_EXT_CPU_CTRL_PORT_IDX_MASK, dp->index
					);
			if (ret)
				return ret;
		}
	}

	//#cfg_FWD_INVLD_MAC_CTRL_EN,cfg_FWD_UNKN_OPCODE_EN
	rtl837x_reg_bits_write(priv, RTL837X_MAC_L2_GLOBAL_CTRL_REG,
			       RTL837X_MAC_L2_GLOBAL_CTRL_FWD_UNKN_OPCODE_EN_MASK |
			       RTL837X_MAC_L2_GLOBAL_CTRL_FWD_INVLD_MAC_EN_MASK,
			       0b11);

	/* Set RS_LINK_FAULT_INDI_OFF to disable the link fault flag, which
	 * resolves a DSC expansion issue on port4-port7 link down.
	 */
	rtl837x_reg_bits_write(priv, RTL837X_RS_LAYER_CONFIG_REG,
			       RTL837X_RS_LAYER_CONFIG_RS_LINK_FAULT_INDI_OFF_MASK,
			       1);

	ret = rtl8372n_of_setup_phy_and_serdes(priv);
	if (ret)
		return ret;

	msleep(20);

	of_extra_init(ds);

	// enable SMI0/1/2 MDC clock output
	rtl837x_reg_bits_write(priv, RTL837X_SMI_CTRL_REG,
			       RTL837X_SMI_CTRL_SMI0_MDC_EN_MASK |
			  RTL837X_SMI_CTRL_SMI1_MDC_EN_MASK |
			  RTL837X_SMI_CTRL_SMI2_MDC_EN_MASK,
			 0xffffffff
			);

	dsa_switch_for_each_port(dp, ds) {
		ret = rtl8372n_setup_port(priv, dp->index);
		if (ret)
			return ret;
	}

	ret = rtl8372n_setup_mdio(priv);
	if (ret) {
		dev_err(priv->dev, "rtl8372n_setup_mdio Fail, error:%d\n", ret);
		return ret;
	}

	// Reset vlan table
	ret = rtl837x_reg_bits_write(priv, RTL837X_VLAN_CTRL_REG,
				     RTL837X_VLAN_CTRL_TABLE_RST_MASK, 1);
	if (ret)
		return ret;

	dsa_switch_for_each_user_port(dp, ds) {
		/* Forward only to the CPU */
		ret = rtl8372n_port_set_isolation(priv, dp->index,
						  dsa_cpu_ports(ds));
		if (ret)
			return ret;
	}

	dsa_switch_for_each_cpu_port(dp, ds) {
		ret = rtl8372n_port_set_isolation(priv, dp->index,
						  dsa_user_ports(ds));
		if (ret)
			return ret;
	}

	dsa_switch_for_each_available_port(dp, ds) {
		/* Flood every kind of frame to every port, as DSA expects for
		 * standalone ports; port_bridge_flags changes it per bridge port.
		 */
		ret = rtl8372n_port_set_ucast_flood(priv, dp->index, true);
		if (ret)
			return ret;

		ret = rtl8372n_port_set_mcast_flood(priv, dp->index, true);
		if (ret)
			return ret;

		ret = rtl8372n_port_set_bcast_flood(priv, dp->index, true);
		if (ret)
			return ret;

		/* Disable vlan-filtering for all ports */
		ret = rtl8372n_port_vlan_filtering(ds, dp->index, false, NULL);
		if (ret) {
			dev_err(priv->dev,
				"Failed to disable vlan filtering on port %d\n",
				dp->index);
			return ret;
		}

		/* The switch default pvid is 1, clean it */
		ret = rtl8372n_clean_pvid(priv, dp->index, 1);
		if (ret) {
			dev_err(priv->dev,
				"Failed to clean pvid %d on port %d\n",
				1, dp->index);
			return ret;
		}
	}

	ret = rtl837x_lut_l2_flush_all(priv);
	if (ret)
		return ret;

	// Enable vlan egrFilter
	ret = rtl837x_reg_bits_write(priv, RTL837X_VLAN_CTRL_REG,
				     RTL837X_VLAN_CTRL_CVLAN_FILTER_MASK, 1
			);
	if (ret)
		return ret;

	rtnl_lock();
	switch (priv->tag_proto) {
	case DSA_TAG_PROTO_MXL862_8021Q:
		ret = rtl8372n_set_tag_8021q(ds);
		break;
	case DSA_TAG_PROTO_RTL8_4:
		ret = rtl8372n_set_tag_rtl(ds);
		break;
	default:
		ret = -EPROTONOSUPPORT;
	}
	rtnl_unlock();
	if (ret)
		return ret;

	rtl8372n_stats_start(ds);

	return 0;
}

static const struct dsa_switch_ops rtl8372n_switch_ops_mdio = {
	.get_tag_protocol = rtl8372n_get_tag_protocol,
	.change_tag_protocol = rtl8372n_change_tag_protocol,
	.setup = rtl8372n_setup,
	.teardown = rtl8372n_teardown,

	.phylink_get_caps = rtl8372n_phylink_get_caps,

	.get_stats64 = rtl8372n_get_stats64,
	.get_strings = rtl8372n_get_strings,
	.get_ethtool_stats = rtl8372n_get_ethtool_stats,
	.get_sset_count = rtl8372n_get_sset_count,
	.get_eth_phy_stats = rtl8372n_get_phy_stats,
	.get_eth_mac_stats = rtl8372n_get_mac_stats,
	.get_eth_ctrl_stats = rtl8372n_get_ctrl_stats,

	.port_vlan_filtering = rtl8372n_port_vlan_filtering,
	.port_vlan_add = rtl8372n_vlan_add,
	.port_vlan_del = rtl8372n_vlan_del,

	.port_stp_state_set = rtl8372n_port_stp_state_set,

	.port_pre_bridge_flags = rtl8372n_port_pre_bridge_flags,
	.port_bridge_flags = rtl8372n_port_bridge_flags,
	.port_change_mtu = rtl8372n_port_change_mtu,
	.port_max_mtu = rtl8372n_port_max_mtu,
	.port_bridge_join = rtl8372n_port_bridge_join,
	.port_bridge_leave = rtl8372n_port_bridge_leave,

	.port_fdb_add  = rtl8372n_port_fdb_add,
	.port_fdb_del  = rtl8372n_port_fdb_del,
	.port_fdb_dump = rtl8372n_port_fdb_dump,
	.port_mdb_add  = rtl8372n_port_mdb_add,
	.port_mdb_del  = rtl8372n_port_mdb_del,
	.port_fast_age = rtl8372n_port_fast_age,

	.tag_8021q_vlan_add = rtl8372n_tag_8021q_vlan_add,
	.tag_8021q_vlan_del = rtl8372n_tag_8021q_vlan_del,

#if  KERNEL_VERSION(6, 12, 44) <= LINUX_VERSION_CODE
	.support_eee		= dsa_supports_eee,
#endif
	.set_mac_eee		= rtl8372n_set_mac_eee,
};

static const struct rtl837x_ops rtl8372n_ops = {
	.detect		= rtl8372n_detect,
	.get_vlan_4k	= rtl8372n_get_vlan_4k,
	.set_vlan_4k	= rtl8372n_set_vlan_4k,
	.get_mib_counter = rtl8372n_get_mib_counter,

	.phy_read_c22   = rtl837x_phy_read_c22,
	.phy_write_c22  = rtl837x_phy_write_c22,
	.phy_read_c45   = rtl837x_phy_read_c45,
	.phy_write_c45  = rtl837x_phy_write_c45,
};

const struct rtl837x_variant rtl8372n_variant = {
	.ds_ops_mdio = &rtl8372n_switch_ops_mdio,
	.ops = &rtl8372n_ops,
	.def_tag_proto = DSA_TAG_PROTO_RTL8_4,
	.pl_mac_ops = &rtl8372n_phylink_mac_ops,
	.chip_data_sz = sizeof(struct rtl8372n),
};
EXPORT_SYMBOL_GPL(rtl8372n_variant);
