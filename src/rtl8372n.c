#include <linux/version.h>
#include <linux/regmap.h>
#include <linux/if_bridge.h>

#include "rtl837x.h"

#define RTL8372N_NUM_PORTS 9
#define RTL8372N_VLAN_UNTAG_MASK 0x3FF
#define RTL8372N_VLAN_MEMBER_MASK 0x3FF
#define RTL8372N_VLAN_FID_MASK 0xF
#define RTL8372N_VLAN_MAX 4095
#define RTL8372N_LUT_MAX 4160

// TODO: this should check port is serdes mode or port mode
#define IS_SERDES_PORT(port) (((port)==3)||((port)==8))

#define PORT_TO_SERDES_IDX(port) ((port==3)?0:1)

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

static const struct rtl837x_mib_counter rtl8372n_mib_counters[] ={
	RTL837X_MAKE_MIB_COUNTER(0,  2, ifInOctets        ),
	RTL837X_MAKE_MIB_COUNTER(2,  2, ifOutOctets       ),
	RTL837X_MAKE_MIB_COUNTER(4,  2, ifInUcastPkts     ),
	RTL837X_MAKE_MIB_COUNTER(6,  2, ifInMulticastPkts ),
	RTL837X_MAKE_MIB_COUNTER(8,  2, ifInBroadcastPkts ),
	RTL837X_MAKE_MIB_COUNTER(10, 2, ifOutUcastPkts    ),
	RTL837X_MAKE_MIB_COUNTER(12, 2, ifOutMulticastPkts),
	RTL837X_MAKE_MIB_COUNTER(14, 2, ifOutBroadcastPkts),

	RTL837X_MAKE_MIB_COUNTER(16, 1, ifOutDiscards                    ),
	RTL837X_MAKE_MIB_COUNTER(17, 1, dot1dTpPortInDiscards            ),
	RTL837X_MAKE_MIB_COUNTER(18, 1, dot3StatsSingleCollisionFrames   ),
	RTL837X_MAKE_MIB_COUNTER(19, 1, dot3StatsMultipleCollisionFrames ),
	RTL837X_MAKE_MIB_COUNTER(20, 1, dot3StatsDeferredTransmissions   ),
	RTL837X_MAKE_MIB_COUNTER(21, 1, dot3StatsLateCollisions          ),
	RTL837X_MAKE_MIB_COUNTER(22, 1, dot3StatsExcessiveCollisions     ),
	RTL837X_MAKE_MIB_COUNTER(23, 1, dot3StatsSymbolErrors            ),
	RTL837X_MAKE_MIB_COUNTER(24, 1, dot3ControlInUnknownOpcodes      ),
	RTL837X_MAKE_MIB_COUNTER(25, 1, dot3InPauseFrames                ),
	RTL837X_MAKE_MIB_COUNTER(26, 1, dot3OutPauseFrames               ),
	RTL837X_MAKE_MIB_COUNTER(27, 1, etherStatsDropEvents             ),
	RTL837X_MAKE_MIB_COUNTER(28, 1, tx_etherStatsBroadcastPkts       ),
	RTL837X_MAKE_MIB_COUNTER(29, 1, tx_etherStatsMulticastPkts       ),
	RTL837X_MAKE_MIB_COUNTER(30, 1, tx_etherStatsCRCAlignErrors      ),
	RTL837X_MAKE_MIB_COUNTER(31, 1, rx_etherStatsCRCAlignErrors      ),
	RTL837X_MAKE_MIB_COUNTER(32, 1, tx_etherStatsUndersizePkts       ),
	RTL837X_MAKE_MIB_COUNTER(33, 1, rx_etherStatsUndersizePkts       ),
	RTL837X_MAKE_MIB_COUNTER(34, 1, tx_etherStatsOversizePkts        ),
	RTL837X_MAKE_MIB_COUNTER(35, 1, rx_etherStatsOversizePkts        ),
	RTL837X_MAKE_MIB_COUNTER(36, 1, tx_etherStatsFragments           ),
	RTL837X_MAKE_MIB_COUNTER(37, 1, rx_etherStatsFragments           ),
	RTL837X_MAKE_MIB_COUNTER(38, 1, tx_etherStatsJabbers             ),
	RTL837X_MAKE_MIB_COUNTER(39, 1, rx_etherStatsJabbers             ),
	RTL837X_MAKE_MIB_COUNTER(40, 1, tx_etherStatsCollisions          ),
	RTL837X_MAKE_MIB_COUNTER(41, 1, tx_etherStatsPkts64Octets        ),
	RTL837X_MAKE_MIB_COUNTER(42, 1, rx_etherStatsPkts64Octets        ),
	RTL837X_MAKE_MIB_COUNTER(43, 1, tx_etherStatsPkts65to127Octets   ),
	RTL837X_MAKE_MIB_COUNTER(44, 1, rx_etherStatsPkts65to127Octets   ),
	RTL837X_MAKE_MIB_COUNTER(45, 1, tx_etherStatsPkts128to255Octets  ),
	RTL837X_MAKE_MIB_COUNTER(46, 1, rx_etherStatsPkts128to255Octets  ),
	RTL837X_MAKE_MIB_COUNTER(47, 1, tx_etherStatsPkts256to511Octets  ),
	RTL837X_MAKE_MIB_COUNTER(48, 1, rx_etherStatsPkts256to511Octets  ),
	RTL837X_MAKE_MIB_COUNTER(49, 1, tx_etherStatsPkts512to1023Octets ),
	RTL837X_MAKE_MIB_COUNTER(50, 1, rx_etherStatsPkts512to1023Octets ),
	RTL837X_MAKE_MIB_COUNTER(51, 1, tx_etherStatsPkts1024to1518Octets),
	RTL837X_MAKE_MIB_COUNTER(52, 1, rx_etherStatsPkts1024to1518Octets),

	RTL837X_MAKE_MIB_COUNTER(54, 1, rx_etherStatsUndersizedropPkts        ),
	RTL837X_MAKE_MIB_COUNTER(55, 1, tx_etherStatsPkts1519toMaxOctets      ),
	RTL837X_MAKE_MIB_COUNTER(56, 1, rx_etherStatsPkts1519toMaxOctets      ),
	RTL837X_MAKE_MIB_COUNTER(57, 1, tx_etherStatsPktsOverMaxOctets        ),
	RTL837X_MAKE_MIB_COUNTER(58, 1, rx_etherStatsPktsOverMaxOctets        ),
	RTL837X_MAKE_MIB_COUNTER(59, 1, tx_etherStatsPktsFlexibleOctetsSET1   ),
	RTL837X_MAKE_MIB_COUNTER(60, 1, rx_etherStatsPktsFlexibleOctetsSET1   ),
	RTL837X_MAKE_MIB_COUNTER(61, 1, tx_etherStatsPktsFlexibleOctetsCRCSET1),
	RTL837X_MAKE_MIB_COUNTER(62, 1, rx_etherStatsPktsFlexibleOctetsCRCSET1),
	RTL837X_MAKE_MIB_COUNTER(63, 1, tx_etherStatsPktsFlexibleOctetsSET0   ),
	RTL837X_MAKE_MIB_COUNTER(64, 1, rx_etherStatsPktsFlexibleOctetsSET0   ),
	RTL837X_MAKE_MIB_COUNTER(65, 1, tx_etherStatsPktsFlexibleOctetsCRSET0C),
	RTL837X_MAKE_MIB_COUNTER(66, 1, rx_etherStatsPktsFlexibleOctetsCRSET0C),
	RTL837X_MAKE_MIB_COUNTER(67, 1, lengthFieldError                      ),
	RTL837X_MAKE_MIB_COUNTER(68, 1, falseCarrieimes                       ),
	RTL837X_MAKE_MIB_COUNTER(69, 1, underSizeOctets                       ),
	RTL837X_MAKE_MIB_COUNTER(70, 1, framingErrors                         ),

	RTL837X_MAKE_MIB_COUNTER(72, 1, rxMacDiscards              ),
	RTL837X_MAKE_MIB_COUNTER(73, 1, rxMacIPGShortDropRT        ),

	RTL837X_MAKE_MIB_COUNTER(75, 1, dot1dTpLearnedEntryDiscards),
	RTL837X_MAKE_MIB_COUNTER(76, 1, egrQueue7DropPktRT         ),
	RTL837X_MAKE_MIB_COUNTER(77, 1, egrQueue6DropPktRT         ),
	RTL837X_MAKE_MIB_COUNTER(78, 1, egrQueue5DropPktRT         ),
	RTL837X_MAKE_MIB_COUNTER(79, 1, egrQueue4DropPktRT         ),
	RTL837X_MAKE_MIB_COUNTER(80, 1, egrQueue3DropPktRT         ),
	RTL837X_MAKE_MIB_COUNTER(81, 1, egrQueue2DropPktRT         ),
	RTL837X_MAKE_MIB_COUNTER(82, 1, egrQueue1DropPktRT         ),
	RTL837X_MAKE_MIB_COUNTER(83, 1, egrQueue0DropPktRT         ),
	RTL837X_MAKE_MIB_COUNTER(84, 1, egrQueue7OutPktRT          ),
	RTL837X_MAKE_MIB_COUNTER(85, 1, egrQueue6OutPktRT          ),
	RTL837X_MAKE_MIB_COUNTER(86, 1, egrQueue5OutPktRT          ),
	RTL837X_MAKE_MIB_COUNTER(87, 1, egrQueue4OutPktRT          ),
	RTL837X_MAKE_MIB_COUNTER(88, 1, egrQueue3OutPktRT          ),
	RTL837X_MAKE_MIB_COUNTER(89, 1, egrQueue2OutPktRT          ),
	RTL837X_MAKE_MIB_COUNTER(90, 1, egrQueue1OutPktRT          ),
	RTL837X_MAKE_MIB_COUNTER(91, 1, egrQueue0OutPktRT          ),

	RTL837X_MAKE_MIB_COUNTER(92, 2, TxGoodCnt                  ),
	RTL837X_MAKE_MIB_COUNTER(94, 2, RxGoodCnt                  ),

	RTL837X_MAKE_MIB_COUNTER(96, 1, RxErrorCnt                 ),
	RTL837X_MAKE_MIB_COUNTER(97, 1, TxErrorCnt                 ),

	RTL837X_MAKE_MIB_COUNTER(98, 2, TxGoodCnt_phy              ),
	RTL837X_MAKE_MIB_COUNTER(100,2, RxGoodCnt_phy              ),

	RTL837X_MAKE_MIB_COUNTER(102, 1,RxErrorCnt_phy             ),
	RTL837X_MAKE_MIB_COUNTER(103, 1,TxErrorCnt_phy             )
};

static_assert(ARRAY_SIZE(rtl8372n_mib_counters) == RTL837X_MIB_END);

struct rtl8372n_pcs
{
	struct phylink_pcs pcs;
	struct rtl837x_priv *priv;
	int index;
};

struct rtl8372n {
	struct rtl8372n_pcs pcs[RTL8372N_NUM_PORTS];
	netdev_features_t csum_feature_backup;
	bool pvid_enabled[RTL8372N_NUM_PORTS];
	bool dsa_tag_8021q_vid[RTL8372N_VLAN_MAX+1];
};

static int rtl8372n_detect(struct rtl837x_priv *priv)
{
	struct device *dev = priv->dev;
	int ret;
	u32 val;

    switch_chip_t sw_chip;

	ret = rtl837x_reg_read(priv, RTL8373_MODEL_NAME_INFO_ADDR, &val);
    dev_info(dev, "CHIP_ID: 0x%08x \n", val);

	switch (val >> 8)
	{
		case 0x837300:
			sw_chip = CHIP_RTL8373;
			break;
		case 0x837200:
			sw_chip = CHIP_RTL8372;
			break;
		case 0x822400:
			sw_chip = CHIP_RTL8224;
			break;
		case 0x837370:
			sw_chip = CHIP_RTL8373N;
			break;
		case 0x837270:
			sw_chip = CHIP_RTL8372N;
			break;
		case 0x822470:
			sw_chip = CHIP_RTL8224N;
			break;
		case 0x8366A8:
			sw_chip = CHIP_RTL8366U;
			break;
		default:
			sw_chip = CHIP_END;
			break;
	}

	switch (sw_chip) {
        case CHIP_RTL8372N:
            dev_info(dev, "found an %s switch\n", chipid_to_chip_name(sw_chip));
            priv->num_ports = RTL8372N_NUM_PORTS;
            priv->mib_counters = rtl8372n_mib_counters;
            priv->num_mib_counters = ARRAY_SIZE(rtl8372n_mib_counters);
            break;
        case CHIP_RTL8373:
        case CHIP_RTL8224:
        case CHIP_RTL8372:
        case CHIP_RTL8373N:
        case CHIP_RTL8221B:
        case CHIP_RTL8224N:
            dev_info(dev, "found an %s switch\n", chipid_to_chip_name(sw_chip));
            dev_err(dev, "This switch is not yet supported!\n");
            return -ENODEV;
        default:
            dev_info(dev, "found an Unknown Realtek switch (id=0x%04x)\n",
                val);
            return -ENODEV;
	}

	return 0;
}

// Set Ingress frame type
static int rtl8372n_drop_untagged(struct rtl837x_priv *priv, int port, bool drop)
{
	// ACCEPT_FRAME_TYPE_ALL = 0,             /* untagged, priority-tagged and tagged */
	// ACCEPT_FRAME_TYPE_TAG_ONLY,         /* tagged */
	// ACCEPT_FRAME_TYPE_UNTAG_ONLY,     /* untagged and priority-tagged */
	// ACCEPT_FRAME_TYPE_END
	dev_dbg(priv->dev, "[%s]: port (%d), (%s)\n", __func__, port, drop ? "drop" : "keep");

	return rtl837x_reg_bits_write(priv, RTL8373_VLAN_PORT_AFT_ADDR(port),
			  RTL8373_VLAN_PORT_AFT_CTAG_ACCEPT_TYPE_MASK(port),
			  drop ? 1 : 0
			);
}

static int rtl8372n_get_vlan_4k(struct rtl837x_priv *priv, u32 vid,
				 struct rtl837x_vlan_4k *vlan4k)
{
	int ret;

	struct rtl837x_vlan_data vlan;
	vlan.vid = vid;

	ret = rtl837x_vlan_get(priv, &vlan);
	if (ret)
	{
		dev_dbg(priv->dev, "[%s]: failed: ret: %d\n", __func__, ret);
		return ret;
	}

	vlan4k->vid = vlan.vid;
	vlan4k->member = vlan.mbr;
	vlan4k->untag = vlan.untag;
	vlan4k->fid = vlan.fid;

	return 0;
}
static int rtl8372n_set_vlan_4k(struct rtl837x_priv *priv,
			       const struct rtl837x_vlan_4k *vlan4k)
{
	int ret;
	
	struct rtl837x_vlan_data vlan = {0};
	vlan.vid = vlan4k->vid;
	vlan.mbr = vlan4k->member;
	vlan.untag = vlan4k->untag;
    vlan.fid = vlan4k->fid;
    vlan.ivl_en = 1;

	ret = rtl837x_vlan_set(priv, &vlan);
	if (ret)
	{
		dev_dbg(priv->dev, "rtl837x_vlan_set: failed: ret: %d\n", ret);
		return ret;
	}
	return 0;
}

static int rtl8372n_vlan_update(struct rtl837x_priv *priv, int vid, u32 member,
		     u32 untag, u32 fid)
{
	int ret;
	struct rtl837x_vlan_4k vlan4k;

	ret = priv->ops->get_vlan_4k(priv, vid, &vlan4k);
	if (ret)
		return ret;

	vlan4k.member |= member;
	vlan4k.untag &= ~member;
	vlan4k.untag |= untag;
	vlan4k.fid = fid;
	vlan4k.vid = vid;
	ret = priv->ops->set_vlan_4k(priv, &vlan4k);

	return ret;
}

/*
 * set the port vlan egress mode
 * If a port disable the tag rewrite
 * frame will ignore the vlan tag config, frames can be sent out as they enter
 * else
 * the tag will remove/keep by vlan config
*/
static int rtl8372n_port_vlan_tag_rewrite(struct rtl837x_priv *priv, int port,
		     bool enable)
{
	/*
	 *	VLAN_EGRESS_TAG_MODE_ORIGINAL = 0,
	 *	VLAN_EGRESS_TAG_MODE_KEEP_FORMAT,
	 *	VLAN_EGRESS_TAG_MODE_PRI,
	 *	VLAN_EGRESS_TAG_MODE_REAL_KEEP,
	 *	VLAN_EGRESS_TAG_MODE_END
	*/
	return rtl837x_reg_bits_write(priv, RTL8373_VLAN_PORT_EGR_TAG_ADDR(port),
		 RTL8373_VLAN_PORT_EGR_TAG_MODE_MASK(port), enable == true ? 0 : 1
		);
}

static int rtl8372n_set_pvid(struct rtl837x_priv *priv, int port,
		     u32 vid)
{
	int ret;
	struct rtl8372n *chip_data = priv->chip_data;
	struct dsa_switch *ds = priv->ds;
	bool pvid_enabled;

	dev_dbg(priv->dev, "[%s]: port (%d), vid (%d)\n", __func__, port, vid);
	pvid_enabled = !!vid;

	ret = rtl837x_reg_bits_write(priv, RTL8373_VLAN_PORT_PB_VLAN_ADDR(port),
			  RTL8373_VLAN_PORT_PB_VLAN_PVID_MASK(port), vid
			);
	if (ret)
		return ret;

	chip_data->pvid_enabled[port] = pvid_enabled;

	/* If VLAN filtering is enabled and PVID is also enabled, we must
	 * not drop any untagged or C-tagged frames. Make sure to update the
	 * filtering setting.
	 */
	if (dsa_port_is_vlan_filtering(dsa_to_port(ds, port)))
		ret = rtl8372n_drop_untagged(priv, port, !pvid_enabled);
	return ret;
}

static int rtl8372n_get_mib_counter(struct rtl837x_priv *priv,
                int port,
                const struct rtl837x_mib_counter *mib,
                u64 *mibvalue)
{
    int ret;
	u32 val_h, val_l, val;

    int mib_id = (mib->offset)/2;

	u32 tmp = (FIELD_PREP(RTL8373_INDIRECT_ACCESS_CTRL_PORT_ID_MASK, port) |
					FIELD_PREP(RTL8373_INDIRECT_ACCESS_CTRL_MIB_ID_MASK, mib_id) |
					FIELD_PREP(RTL8373_INDIRECT_ACCESS_CTRL_ACC_CMD_MASK, 1));

	ret = rtl837x_reg_write(priv, RTL8373_INDIRECT_ACCESS_CTRL_ADDR, tmp);
    if(ret) return ret;

	ret = regmap_read_poll_timeout(priv->map, RTL8373_INDIRECT_ACCESS_CTRL_ADDR, tmp, ((tmp & RTL8373_INDIRECT_ACCESS_CTRL_ACC_CMD_MASK) == 0), 0, 1000);
    if(ret) return ret;

	if (mib->length > 1)
	{
		ret = rtl837x_reg_read(priv, RTL8373_INDIRECT_ACCESS_CNT_L_ADDR, &val_l);
		if(ret) return ret;
		ret = rtl837x_reg_read(priv, RTL8373_INDIRECT_ACCESS_CNT_H_ADDR, &val_h);
		if(ret) return ret;
		*mibvalue = ((u64)val_l << 32) | val_h;
		return 0;
	} else
	{
		if(mib->offset % 2)
			ret = rtl837x_reg_read(priv, RTL8373_INDIRECT_ACCESS_CNT_H_ADDR, &val);
		else
			ret = rtl837x_reg_read(priv, RTL8373_INDIRECT_ACCESS_CNT_L_ADDR, &val);
		*mibvalue = val;
		return ret;
	}
}

static enum dsa_tag_protocol rtl8372n_get_tag_protocol(struct dsa_switch *ds,
                                                        int port,
                                                        enum dsa_tag_protocol mp)
{
    struct rtl837x_priv *priv = ds->priv;
	struct device *dev = priv->dev;
    dev_dbg(dev, "get_DSA_PROTO port:%d\n", port);

	return priv->tag_proto;
}

static int rtl8372n_mdio_phy_read_c22(struct mii_bus *bus, int addr, int regnum)
{
	struct rtl837x_priv *priv = bus->priv;
	u16 val;

	int ret = priv->ops->phy_read_c22(priv, addr, regnum, &val);
	if (ret)
		return ret;

	return val;
}

static int rtl8372n_mdio_phy_write_c22(struct mii_bus *bus, int addr, int regnum, u16 val)
{
	struct rtl837x_priv *priv = bus->priv;

	return priv->ops->phy_write_c22(priv, addr, regnum, val);
}

static int rtl8372n_mdio_phy_read_c45(struct mii_bus *bus, int port, int devad, int regnum)
{
	struct rtl837x_priv *priv = bus->priv;
	u16 val;
	int ret = priv->ops->phy_read_c45(priv, port, devad, regnum, &val);
	if (ret)
		return ret;

	return val;
}

static int rtl8372n_mdio_phy_write_c45(struct mii_bus *bus, int port, int devad, int regnum, u16 val)
{
	struct rtl837x_priv *priv = bus->priv;

	return priv->ops->phy_write_c45(priv, port, devad, regnum, val);
}

static int rtl8372n_setup_mdio(struct rtl837x_priv *priv)
{
	struct device_node *np = priv->dev->of_node;
    struct device_node *mnp;
	struct dsa_switch *ds = priv->ds;
	struct device *dev = priv->dev;
	struct mii_bus *bus;
	static int idx;
	int ret = 0;

    mnp = of_get_child_by_name(np, "mdio");

	if (mnp && !of_device_is_available(mnp))
		goto out;

	bus = devm_mdiobus_alloc(dev);
	if (!bus) {
		ret = -ENOMEM;
		goto out;
	}

#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,12,44)
	if (!mnp)
		ds->user_mii_bus = bus;
#endif

    bus->priv = priv;
	bus->name = KBUILD_MODNAME "-mii";
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
	}

out:
	of_node_put(mnp);
	return ret;
}

static int rtl8372n_pcs_validate(struct phylink_pcs *pcs,
			       unsigned long *supported,
			       const struct phylink_link_state *state)
{
	return 0;
}
#if LINUX_VERSION_CODE >= KERNEL_VERSION(6,18,0)
static void rtl8372n_sds_pcs_get_state(struct phylink_pcs *pcs, unsigned int neg_mode,
				 struct phylink_link_state *state)
#else
static void rtl8372n_sds_pcs_get_state(struct phylink_pcs *pcs,
				 struct phylink_link_state *state)
#endif
{
	int ret;
	u32 tmp;
	struct rtl8372n_pcs *_pcs = container_of(pcs, struct rtl8372n_pcs, pcs);
	struct rtl837x_priv *priv = _pcs->priv;
	int port = _pcs->index;

	ret = rtl837x_reg_bits_read(priv, RTL8373_MAC_LINK_STS_ADDR, BIT(port), &tmp);
	if (ret < 0)
		return;
	state->link = tmp&1;
	state->an_complete = tmp&1;

	ret = rtl837x_reg_bits_read(priv, RTL8373_MAC_LINK_DUP_STS_ADDR, BIT(port), &tmp);
	if (ret < 0)
		return;
	state->duplex = tmp&1;

	state->pause &= ~(MLO_PAUSE_RX | MLO_PAUSE_TX);

	ret = rtl837x_reg_bits_read(priv, RTL8373_MAC_RX_PAUSE_STS_ADDR, BIT(port), &tmp);
	if (ret < 0)
		return;
	if (tmp&1)
		state->pause |= MLO_PAUSE_RX;

	ret = rtl837x_reg_bits_read(priv, RTL8373_MAC_TX_PAUSE_STS_ADDR, BIT(port), &tmp);
	if (ret < 0)
		return;
	if (tmp&1)
		state->pause |= MLO_PAUSE_TX;

	ret = rtl837x_reg_read(priv, RTL8373_MAC_LINK_SPD_STS_ADDR(port), &tmp);
	if (ret)
		return;
	tmp = (tmp & RTL8373_MAC_LINK_SPD_STS_SPD_STS_9_0_MASK(port)) >> __ffs(RTL8373_MAC_LINK_SPD_STS_SPD_STS_9_0_MASK(port));
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

    // dev_dbg(priv->dev, "[%s] port(%d) speed: %d, link: %d, duplex: %d\n", __func__,
	// 				  port, state->speed, state->link, state->duplex);
}

static int rtl8372n_pcs_config(struct phylink_pcs *pcs, unsigned int neg_mode,
			     phy_interface_t interface,
			     const unsigned long *advertising,
			     bool permit_pause_to_mac)
{
	return 0;
}

static void rtl8372n_pcs_an_restart(struct phylink_pcs *pcs)
{
}

static const struct phylink_pcs_ops rtl8372n_sds_pcs_ops = {
	.pcs_validate = rtl8372n_pcs_validate,
	.pcs_get_state = rtl8372n_sds_pcs_get_state,
	.pcs_config = rtl8372n_pcs_config,
	.pcs_an_restart = rtl8372n_pcs_an_restart,
};

static void rtl8372n_phylink_get_caps(struct dsa_switch *ds, int port,
				       struct phylink_config *config)
{
	if ((port == 3) || (port == 8)) {
		__set_bit(PHY_INTERFACE_MODE_10GKR, config->supported_interfaces);
		__set_bit(PHY_INTERFACE_MODE_10GBASER, config->supported_interfaces);
        __set_bit(PHY_INTERFACE_MODE_5GBASER, config->supported_interfaces);
        __set_bit(PHY_INTERFACE_MODE_USXGMII, config->supported_interfaces);
        __set_bit(PHY_INTERFACE_MODE_1000BASEX, config->supported_interfaces);
        __set_bit(PHY_INTERFACE_MODE_2500BASEX, config->supported_interfaces);

		config->mac_capabilities = MAC_10000FD | MAC_5000FD | MAC_2500FD | MAC_1000 | MAC_100 | MAC_10 |
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
	struct dsa_port *dp = dsa_phylink_to_port(config);
	struct rtl837x_priv *priv = dp->ds->priv;
	struct rtl8372n *chip_data = priv->chip_data;

	if (IS_SERDES_PORT(dp->index))
		return &(chip_data->pcs[dp->index].pcs);
	return NULL;
}

static void rtl8372n_phylink_mac_config(struct phylink_config *config, unsigned int mode,
			const struct phylink_link_state *state)
{
	struct dsa_port *dp = dsa_phylink_to_port(config);
	struct rtl837x_priv *priv = dp->ds->priv;
	int port = dp->index;

	// dev_info(priv->dev, "\n\ncalled rtl8372n_phylink_mac_config: port: %d, mode: %s\n\n\n", port, phy_modes(interface));

	if (!IS_SERDES_PORT(port))
		return;
	dev_info(priv->dev, "MAC config serdes port(%d) mode (%x)\n", 
			  PORT_TO_SERDES_IDX(port), 
			  phy_interface_to_rtk_sds_mode(state->interface));

	if (rtl837x_serdes_set_mode(priv, PORT_TO_SERDES_IDX(port), phy_interface_to_rtk_sds_mode(state->interface)))
		dev_err(priv->dev, "[%s]: Failed to set serdes mode\n", __func__);
}

static void rtl8372n_phylink_mac_link_down(struct phylink_config *config, unsigned int mode,
				phy_interface_t interface)
{
	struct dsa_port *dp = dsa_phylink_to_port(config);
	struct rtl837x_priv *priv = dp->ds->priv;
	int port = dp->index;
	int ret = 0;

	switch (port)
	{
	case 4 ... 7:
		dev_info(priv->dev, "MAC link down on phy port (%d)\n", port);
		break;
	case 3:
	case 8:
		dev_info(priv->dev, "MAC link down on serdes port (%d)\n", PORT_TO_SERDES_IDX(port));
		break;
	}

	if (ret) {
		dev_err(priv->dev, "MAC link down failed port(%d)\n", port);
		return;
	}
}

static void rtl8372n_phylink_mac_link_up(struct phylink_config *config,
			struct phy_device *phy, unsigned int mode,
			phy_interface_t interface, int speed, int duplex,
			bool tx_pause, bool rx_pause)
{
	struct dsa_port *dp = dsa_phylink_to_port(config);
	struct rtl837x_priv *priv = dp->ds->priv;
	int port = dp->index;
	int ret = 0;

	switch (port)
	{
	case 4 ... 7:
		dev_info(priv->dev, "MAC link up on phy port(%d)\n", port);
		break;
	case 3:
	case 8:
		dev_info(priv->dev, "MAC link up on serdes port(%d) mode (%x), speed (%d)\n", 
							PORT_TO_SERDES_IDX(port), 
							phy_interface_to_rtk_sds_mode(interface),
							speed);
		ret = rtl837x_serdes_set_mode(priv, PORT_TO_SERDES_IDX(port), phy_interface_to_rtk_sds_mode(interface));
		break;
	}

	if (ret) {
		dev_err(priv->dev, "[%s]: failed to enable the port(%d)\n", __func__, port);
		return;
	}
}

static const struct phylink_mac_ops rtl8372n_phylink_mac_ops = {
	.mac_select_pcs	= rtl8372n_phylink_mac_select_pcs,
	.mac_config	= rtl8372n_phylink_mac_config,
	.mac_link_down	= rtl8372n_phylink_mac_link_down,
	.mac_link_up	= rtl8372n_phylink_mac_link_up,
};

/*
 * I still don't understand how the chip GPIO register and LED configuration
 * register work, so we use additional initialization values to configure 
 * the chip LED and GPIO matrix or other things
*/
static int of_extra_init(struct dsa_switch *ds)
{
    struct rtl837x_priv *priv = ds->priv;
	struct device_node *np = ds->dev->of_node;
	const __be32 *list;
	int size, data_len;
	u32 reg, mask, val;

	list = of_get_property(np, "extra-init", &size);
	if (!list || !size) return 0;

	data_len = size / (3*sizeof(__be32));
	for (int i=0; i<data_len; i++)
	{
		reg = be32_to_cpu(*list);
		list++;
		mask = be32_to_cpu(*list);
		list++;
		val = be32_to_cpu(*list);
		list++;
		dev_dbg(ds->dev, "of_extra_init: reg:0x%04X mask:0x%08X val:0x%08X\n", 
							reg, mask, val);
		rtl837x_reg_bits_write(priv, reg, mask, val);
	}
	return 0;
}

static int rtl8372n_set_tag_rtl(struct dsa_switch *ds)
{
	int ret;
    struct rtl837x_priv *priv = ds->priv;
	struct rtl8372n *chip_data = priv->chip_data;
	struct dsa_port *dp, *cpu_dp = NULL;
	struct net_device *master_dev = NULL;
	dev_dbg(priv->dev, "[%s]\n", __func__);

	// Only support one CPU port
	dsa_switch_for_each_cpu_port(dp, ds) {
		cpu_dp = dp;
		break;
	}

	if (cpu_dp == NULL)
		return -ENODEV;

#if LINUX_VERSION_CODE < KERNEL_VERSION(6,12,44)
	master_dev = cpu_dp->master;
#else
	master_dev = cpu_dp->conduit;
#endif

	if (!master_dev)
	{
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
	ret = rtl837x_reg_bits_write(priv, RTL8373_CPU_TAG_CTRL_ADDR,
			  RTL8373_CPU_TAG_CTRL_EXT_CPUTAG_INSERTMOD_MASK, 0
			);
	if (ret)
		return ret;

	// Enable CPU tag
	ret = rtl837x_reg_bits_write(priv, RTL8373_CPU_TAG_CTRL_ADDR,
			  RTL8373_CPU_TAG_CTRL_EXT_CPUTAG_EN_MASK, 1
			);
	if (ret)
		return ret;

	// Add cpu port to RTL8_4 TAG aware port
	ret = rtl837x_reg_bits_write(priv, RTL8373_CPU_TAG_AWARE_CTRL_ADDR, BIT(cpu_dp->index), 1);
	if (ret)
		return ret;

	chip_data->csum_feature_backup = (master_dev->wanted_features & (NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM));
	chip_data->csum_feature_backup |= (master_dev->wanted_features & NETIF_F_HW_CSUM);

    master_dev->wanted_features &= ~(NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM);
    master_dev->wanted_features &= ~NETIF_F_HW_CSUM;
    netdev_update_features(master_dev);

	return 0;
}

static int rtl8372n_teardown_tag_rtl(struct dsa_switch *ds)
{
    struct rtl837x_priv *priv = ds->priv;
	struct rtl8372n *chip_data = priv->chip_data;
	struct dsa_port *dp, *cpu_dp = NULL;
	struct net_device *master_dev = NULL;
	dev_dbg(priv->dev, "[%s]\n", __func__);

	// Only support one CPU port
	dsa_switch_for_each_cpu_port(dp, ds) {
		cpu_dp = dp;
		break;
	}

	if (cpu_dp == NULL)
		return -ENODEV;

#if LINUX_VERSION_CODE < KERNEL_VERSION(6,12,44)
	master_dev = cpu_dp->master;
#else
	master_dev = cpu_dp->conduit;
#endif

	if (!master_dev)
	{
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
	rtl837x_reg_bits_write(priv, RTL8373_CPU_TAG_CTRL_ADDR,
			  RTL8373_CPU_TAG_CTRL_EXT_CPUTAG_INSERTMOD_MASK, 2
			);

	// Disable CPU tag
	rtl837x_reg_bits_write(priv, RTL8373_CPU_TAG_CTRL_ADDR,
			  RTL8373_CPU_TAG_CTRL_EXT_CPUTAG_EN_MASK, 0
			);

	// Remove cpu port from RTL8_4 TAG aware port
	dsa_switch_for_each_cpu_port(dp, ds) {
		rtl837x_reg_bits_write(priv, RTL8373_CPU_TAG_AWARE_CTRL_ADDR, BIT(dp->index), 0);
	}

    master_dev->wanted_features |= chip_data->csum_feature_backup;
    netdev_update_features(master_dev);

	rtl837x_reg_bits_write(priv, RTL8373_L2_TBL_FLUSH_ALL_ADDR, 
			  RTL8373_L2_TBL_FLUSH_ALL_FLUSH_ALL_MASK, 1
			);

	return 0;
}

static int rtl8372n_set_tag_8021q(struct dsa_switch *ds)
{
	int ret;
    struct rtl837x_priv *priv = ds->priv;
	struct dsa_port *dp = NULL;
	dev_dbg(priv->dev, "[%s]\n", __func__);

	u32 cpu_port_mask = 0;

	dsa_switch_for_each_cpu_port(dp, ds) {
		cpu_port_mask |= BIT(dp->index);
	}

	//  bit0: internal cpu; bit1: external cpu
	ret = rtl837x_reg_bits_write(priv, RTL8373_SVLAN_TRAP_CTRL_ADDR,
			  RTL8373_SVLAN_TRAP_CTRL_CPU_PMSK_MASK, BIT(1)
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
	ret = rtl837x_reg_bits_write(priv, RTL8373_VS_CTRL_ADDR,
			  RTL8373_VS_CTRL_SPRISEL_MASK, 1
			);
	if (ret)
		return ret;

	// Drop package when recv a package with out s-tag from cpu port
	/*
     *	UNTAG_DROP = 0,
     *	UNTAG_TRAP,
     *	UNTAG_ASSIGN,
     *	UNTAG_END
	*/
	ret = rtl837x_reg_bits_write(priv, RTL8373_VS_CTRL_ADDR,
			  RTL8373_VS_CTRL_UNTAG_MASK, 0
		);
	if (ret)
		return ret;

	// Set Custome TPID
	ret = rtl837x_reg_write(priv, RTL8373_VS_GLB_CTRL_ADDR, ETH_P_8021Q);
	if (ret)
		return ret;

	// Set cpu port as service port
	ret = rtl837x_reg_write(priv, RTL8373_VS_UPLINK_PORT_ADDR, cpu_port_mask);
	if (ret)
		return ret;

	for (int idx = 0; idx <= RTL837x_C2SIDXMAX;  idx++)
    {
        ret = rtl837x_reg_write(priv, RTL8373_VLAN_C2S_ENTRY_ADDR(idx)+4, 0);
		if (ret)
			return ret;

        ret = rtl837x_reg_write(priv, RTL8373_VLAN_C2S_ENTRY_ADDR(idx), 0);
		if (ret)
			return ret;
    }

	// Set Port Ingress Tag Action
	/*
	 *	UNASSIGN_PBSVID = 0,
	 *	UNASSIGN_TRAP,
	 *	UNASSIGN_END
	*/
	ret = rtl837x_reg_bits_write(priv, RTL8373_VS_CTRL_ADDR,
			  RTL8373_VS_CTRL_UIFSEG_MASK, 0
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
    struct rtl837x_priv *priv = ds->priv;
    struct rtl8372n *chip_data = priv->chip_data;
	struct dsa_port *dp = NULL;
	dev_dbg(priv->dev, "[%s]\n", __func__);

	if (ds->tag_8021q_ctx) {
		dsa_tag_8021q_unregister(ds);
	}

	// Clean service port
	rtl837x_reg_write(priv, RTL8373_VS_UPLINK_PORT_ADDR, 0);

	struct rtl837x_vlan_4k vlan4k;
	memset(&vlan4k, 0, sizeof(vlan4k));
	vlan4k.member = 0;
	vlan4k.untag = 0;
	vlan4k.fid = 0;

	// Remove s-tag vlan entrys
	for (int i=0; i<=RTL8372N_VLAN_MAX; i++)
	{
		if (chip_data->dsa_tag_8021q_vid[i])
		{
			vlan4k.vid = i;
			chip_data->dsa_tag_8021q_vid[i] = false;
			priv->ops->set_vlan_4k(priv, &vlan4k);
		}
	}

	dsa_switch_for_each_user_port(dp, ds) {
		rtl837x_reg_bits_write(priv, RTL8373_VS_PORT_DFLT_SVID_ADDR(dp->index), 
				  RTL8373_VS_PORT_DFLT_SVID_PORT_DFLT_SVID_MASK(dp->index), 0
				);
	}

	rtl837x_reg_bits_write(priv, RTL8373_L2_TBL_FLUSH_ALL_ADDR, 
			  RTL8373_L2_TBL_FLUSH_ALL_FLUSH_ALL_MASK, 1
			);
	return 0;
}

static int rtl8372n_tag_8021q_vlan_add(struct dsa_switch *ds, int port,
				       u16 vid, u16 flags)
{
	int ret = 0;
    struct rtl837x_priv *priv = ds->priv;
	struct rtl8372n *chip_data = priv->chip_data;

	dev_dbg(priv->dev, "[%s]: port: %d, vid:%u, flags: %u\n", __func__, port, vid, flags);

	// Set Port SVID
	if (flags & BRIDGE_VLAN_INFO_PVID)
		ret = rtl837x_reg_bits_write(priv, RTL8373_VS_PORT_DFLT_SVID_ADDR(port), 
				  RTL8373_VS_PORT_DFLT_SVID_PORT_DFLT_SVID_MASK(port), vid
				);
	if (ret)
		return ret;

	u32 member = 0, untag = 0;
	member |= BIT(port);
	if (dsa_is_user_port(ds, port))
	{
		untag |= BIT(port);
	}

	ret = rtl8372n_vlan_update(priv, vid, member, untag, 0);
	if (ret)
		goto fail_rollback;

	chip_data->dsa_tag_8021q_vid[vid] = true;
	return 0;

fail_rollback:
	dev_err(priv->dev, "failed to add 8021q tag for port(%d) vid: %u\n", port, vid);

	if (flags & BRIDGE_VLAN_INFO_PVID)
		rtl837x_reg_bits_write(priv, RTL8373_VS_PORT_DFLT_SVID_ADDR(port), 
				  RTL8373_VS_PORT_DFLT_SVID_PORT_DFLT_SVID_MASK(port), 0
				);
	return ret;
}

static int rtl8372n_tag_8021q_vlan_del(struct dsa_switch *ds, int port,
				       u16 vid)
{
	int ret;
    struct rtl837x_priv *priv = ds->priv;
	struct rtl8372n *chip_data = priv->chip_data;

	struct rtl837x_vlan_4k vlan4k;
	dev_dbg(priv->dev, "[%s]: port: %d, vid: %u\n", __func__, port, vid);

	ret = priv->ops->get_vlan_4k(priv, vid, &vlan4k);
	if (ret)
		return ret;

	vlan4k.member &= ~BIT(port);
	vlan4k.untag &= ~BIT(port);

	if (!vlan4k.member) {
		vlan4k.vid = vid;
		vlan4k.untag = 0;
		vlan4k.fid = 0;
	}

	ret = priv->ops->set_vlan_4k(priv, &vlan4k);
	if (ret) {
		dev_err(priv->dev,
			"failed to remove SVLAN %u\n",
			vid);
		return ret;
	}

	chip_data->dsa_tag_8021q_vid[vid] = false;

	// Clean Port SVID
	if (dsa_is_user_port(ds, port))
		ret = rtl837x_reg_bits_write(priv, RTL8373_VS_PORT_DFLT_SVID_ADDR(port), 
				  RTL8373_VS_PORT_DFLT_SVID_PORT_DFLT_SVID_MASK(port), 0
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
	int ret;
    struct rtl837x_priv *priv = ds->priv;

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

static int rtl8372n_port_set_isolation(struct rtl837x_priv *priv, int port,
					u32 mask)
{
	return rtl837x_reg_write(priv, RTL8373_PORT_ISO_PORT_PMSK_ADDR(port), mask);
}

static int rtl8372n_port_add_isolation(struct rtl837x_priv *priv, int port,
					u32 mask)
{
	return rtl837x_reg_bits_write(priv, RTL8373_PORT_ISO_PORT_PMSK_ADDR(port), 
				  mask, 0xffffffff);
}

static int rtl8372n_port_remove_isolation(struct rtl837x_priv *priv, int port,
					   u32 mask)
{
	return rtl837x_reg_bits_write(priv, RTL8373_PORT_ISO_PORT_PMSK_ADDR(port), 
				  mask, 0);
}

static int rtl8372n_port_add_vlan_transparent(struct rtl837x_priv *priv, int port, u32 mask)
{
	dev_dbg(priv->dev, "[%s]: port:%d mask:0x%08x\n", __func__,
				port, mask);
	return rtl837x_reg_bits_write(priv, RTL8373_VLAN_PORT_EGR_TRANS_ADDR(port), 
				  RTL8373_VLAN_PORT_EGR_TRANS_PMSK_MASK(port) & (mask << RTL8373_VLAN_PORT_EGR_TRANS_PMSK_OFFSET(port)), 0xffffffff);
}

static int rtl8372n_port_remove_vlan_transparent(struct rtl837x_priv *priv, int port, u32 mask)
{
	dev_dbg(priv->dev, "[%s]: port:%d mask:0x%08x\n", __func__,
			port, mask);
	return rtl837x_reg_bits_write(priv, RTL8373_VLAN_PORT_EGR_TRANS_ADDR(port), 
				  RTL8373_VLAN_PORT_EGR_TRANS_PMSK_MASK(port) & (mask << RTL8373_VLAN_PORT_EGR_TRANS_PMSK_OFFSET(port)), 0);
}

static int rtl8372n_port_add_cpu_vlan_transparent(struct rtl837x_priv *priv, int port)
{
	struct dsa_port *cpu_dp = NULL;
	u32 cpu_portmask = 0;

	dsa_switch_for_each_cpu_port(cpu_dp, priv->ds) {
		cpu_portmask |= BIT(cpu_dp->index);
		rtl8372n_port_add_vlan_transparent(priv, cpu_dp->index, BIT(port));
	}

	return rtl8372n_port_add_vlan_transparent(priv, port, cpu_portmask);
}

static int rtl8372n_port_remove_cpu_vlan_transparent(struct rtl837x_priv *priv, int port)
{
	struct dsa_port *cpu_dp = NULL;
	u32 cpu_portmask = 0;

	dsa_switch_for_each_cpu_port(cpu_dp, priv->ds) {
		rtl8372n_port_remove_vlan_transparent(priv, cpu_dp->index, BIT(port));
		cpu_portmask |= BIT(cpu_dp->index);
	}
	return rtl8372n_port_remove_vlan_transparent(priv, port, cpu_portmask);
}

static int rtl8372n_bridge_port_add_resv_vlan(struct rtl837x_priv *priv, int port)
{
	return rtl8372n_vlan_update(priv, 0xfff, BIT(port), BIT(port), 0);
}

static int rtl8372n_bridge_port_remove_resv_vlan(struct rtl837x_priv *priv, int port)
{
	int ret;
	struct rtl837x_vlan_4k vlan4k;

	ret = priv->ops->get_vlan_4k(priv, 0xfff, &vlan4k);
	if (ret)
		return ret;

	vlan4k.member &= ~BIT(port);
	vlan4k.untag &= ~BIT(port);
	vlan4k.fid = 0;
	vlan4k.vid = 0xfff;
	ret = priv->ops->set_vlan_4k(priv, &vlan4k);

	return ret;
}

static void rtl8372n_get_strings(struct dsa_switch *ds, int port, u32 stringset,
			 uint8_t *data)
{
	struct rtl837x_priv *priv = ds->priv;
	const struct rtl837x_mib_counter *mib;
	int i;

	if (port >= priv->num_ports)
		return;

	for (i = 0; i < priv->num_mib_counters; i++) {
		mib = &priv->mib_counters[i];
		strncpy(data + i * ETH_GSTRING_LEN,
			mib->name, ETH_GSTRING_LEN);
	}
}

static void rtl8372n_get_ethtool_stats(struct dsa_switch *ds, int port, uint64_t *data)
{
	struct rtl837x_priv *priv = ds->priv;
	int i;
	int ret;
	const struct rtl837x_mib_counter *mib;

	if (port >= priv->num_ports)
		return;

	mutex_lock(&priv->mib_lock);
	for (i = 0; i < priv->num_mib_counters; i++) {
		u64 mibvalue = 0;

		mib = &priv->mib_counters[i];
		ret = priv->ops->get_mib_counter(priv, port, mib, &mibvalue);
		if (ret) {
			dev_err(priv->dev, "[%s]: Error reading MIB counter %s\n", __func__,
				mib->name);
		}
		data[i] = mibvalue;
	}
	mutex_unlock(&priv->mib_lock);
}

static int rtl8372n_get_sset_count(struct dsa_switch *ds, int port, int sset)
{
	struct rtl837x_priv *priv = ds->priv;

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
	struct rtl837x_priv *priv = ds->priv;
	const struct rtl837x_mib_counter *mib;

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

	struct rtl837x_priv *priv = ds->priv;
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
	struct rtl837x_priv *priv = ds->priv;
	const struct rtl837x_mib_counter *mib = 
		  &priv->mib_counters[RTL837X_MIB_dot3ControlInUnknownOpcodes];

	mutex_lock(&priv->mib_lock);
	priv->ops->get_mib_counter(priv, port, mib,
			  &ctrl_stats->UnsupportedOpcodesReceived);
	mutex_unlock(&priv->mib_lock);
}

// TODO: Fail rollback
static int rtl8372n_vlan_filtering(struct dsa_switch *ds, int port,
                                        bool vlan_filtering, struct netlink_ext_ack *extack)
{
    int ret;
    struct rtl837x_priv *priv = ds->priv;
	struct rtl8372n *chip_data = priv->chip_data;
    
	dev_dbg(priv->dev, "[%s]: port (%d), filtering: %s\n", __func__,
				  port, !!vlan_filtering ? "true" : "false");

	// Set Ingress filter
	ret = rtl837x_reg_bits_write(priv, RTL8373_VLAN_PORT_IGR_FLTR_ADDR(port),
			  RTL8373_VLAN_PORT_IGR_FLTR_IGR_FLTR_ACT_MASK(port),
			  vlan_filtering ? 1 : 0
			);

	if (vlan_filtering)
		ret = rtl8372n_drop_untagged(priv, port, !chip_data->pvid_enabled[port]);
	else
		ret = rtl8372n_drop_untagged(priv, port, false);

    return ret;
}

// TODO: Fail rollback
static int rtl8372n_vlan_add(struct dsa_switch *ds, int port,
                            const struct switchdev_obj_port_vlan *vlan,
                            struct netlink_ext_ack *extack)
{
    int ret;
	bool untagged = !!(vlan->flags & BRIDGE_VLAN_INFO_UNTAGGED);
	bool pvid = !!(vlan->flags & BRIDGE_VLAN_INFO_PVID);
    struct rtl837x_priv *priv = ds->priv;

    u16 vid = vlan->vid;
	u32 member = 0;
	u32 untag = 0;

    if (vid > RTL8372N_VLAN_MAX)
    {
        NL_SET_ERR_MSG_MOD(extack, "VLAN ID not valid");
        return -EINVAL;
    }

	if (vid_is_dsa_8021q(vid) && priv->tag_proto == DSA_TAG_PROTO_MXL862_8021Q)
    {
        NL_SET_ERR_MSG_MOD(extack, "Range 3072-4095 reserved for dsa_8021q operation");
        return -EINVAL;
    }

	member |= BIT(port);

	if (untagged)
		untag |= BIT(port);

	dev_dbg(priv->dev, "[%s] add VLAN %d on port %d, %s, %s\n", __func__,
		vlan->vid, port, untagged ? "untagged" : "tagged",
		pvid ? "PVID" : "no PVID");

	ret = rtl8372n_vlan_update(priv, vid, member, untag, 0);
	if (ret) {
		dev_err(priv->dev, "[%s]: Failed to set up VLAN %04x", __func__,
										  vid);
		return ret;
	}

	if (!pvid)
		return 0;

	ret = rtl8372n_set_pvid(priv, port, vid);
	if (ret) {
		dev_err(priv->dev, "[%s]: Failed to set PVID on port %d to VLAN %04x", __func__,
			port, vid);
		return ret;
	}

    return 0;
}

// TODO: Fail rollback
static int rtl8372n_vlan_del(struct dsa_switch *ds, int port,
                                 const struct switchdev_obj_port_vlan *vlan)
{
	int ret;
	struct rtl837x_priv *priv = ds->priv;

	dev_dbg(priv->dev, "[%s]: port (%d) vid (%d)\n", __func__,
							  port, vlan->vid);

	struct rtl837x_vlan_4k vlan4k;

	ret = priv->ops->get_vlan_4k(priv, vlan->vid, &vlan4k);
	if (ret)
		return ret;

	vlan4k.member &= ~BIT(port);
	vlan4k.untag &= ~BIT(port);

	if (!vlan4k.member) {
		vlan4k.vid = vlan->vid;
		vlan4k.untag = 0;
		vlan4k.fid = 0;
	}

	ret = priv->ops->set_vlan_4k(priv, &vlan4k);
	if (ret) {
		dev_err(priv->dev,
			"failed to remove VLAN %04x\n",
			vlan->vid);
		return ret;
	}
	return 0;
}

// TODO: Fail rollback
static int
rtl8372n_port_bridge_join(struct dsa_switch *ds, int port,
			   struct dsa_bridge bridge,
			   bool *tx_fwd_offload,
			   struct netlink_ext_ack *extack)
{
    struct rtl837x_priv *priv = ds->priv;
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
		if (ret)
			dev_err(priv->dev, "failed to join port %d\n", port);

		port_bitmap |= BIT(dp->index);
	}
	dev_dbg(priv->dev, "[%s]: port(%d) isolate(0x%04x)\n", __func__,
						  port, port_bitmap);

	ret = rtl8372n_bridge_port_add_resv_vlan(priv, port);
	if (ret)
		dev_err(priv->dev, "failed to add port(%d) resv vlan err: %d\n", port, ret);

	/*
	 * Filter and forward the frame by vlan table
	*/
	ret = rtl8372n_port_remove_cpu_vlan_transparent(priv, port);
	if (ret)
		dev_err(priv->dev, "failed to remove port(%d)<->cpu vlan transparent err: %d\n", port, ret);

	if (!dsa_is_cpu_port(ds, port))
	{
		ret = rtl8372n_port_vlan_tag_rewrite(priv, port, true);
		if (ret)
			return ret;
	}

	/* Set the bits for the ports we can access */
	ret = rtl8372n_port_add_isolation(priv, port, port_bitmap);
	return ret;
}

// TODO: Fail rollback?
static void
rtl8372n_port_bridge_leave(struct dsa_switch *ds, int port,
			    struct dsa_bridge bridge)
{
    struct rtl837x_priv *priv = ds->priv;
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
		if (ret)
			dev_err(priv->dev, "failed to leave port %d\n", port);

		port_bitmap |= BIT(dp->index);
	}

	ret = rtl8372n_bridge_port_remove_resv_vlan(priv, port);
	if (ret)
		dev_err(priv->dev, "failed to remove port(%d) resv vlan err: %d\n", port, ret);

	/*
	 * When the port is not in the bridge, in order 
	 * to allow all VLAN tags to be accepted, 
	 * VLAN transparent transmission is set
	*/
	ret = rtl8372n_port_add_cpu_vlan_transparent(priv, port);
	if (ret)
		dev_err(priv->dev, "failed to add port(%d)<->cpu vlan transparent err: %d\n", dp->index, ret);

	/*
	 * Set the hardware do not add/remove/edit the vlan tag
	 * The VLAN remains completely unchanged when the frame enters and exits
	*/
	rtl8372n_port_vlan_tag_rewrite(priv, port, false);

	/* Clear the bits for the ports we can not access, leave ourselves */
	rtl8372n_port_remove_isolation(priv, port, port_bitmap);
}

static int rtl8372n_port_enable(struct dsa_switch *ds, int port,
			       struct phy_device *phy)
{
    struct rtl837x_priv *priv = ds->priv;
	int ret;

	if (IS_SERDES_PORT(port))
		return 0;

	ret = priv->ops->phy_write_c45(priv, port, 31, 0xa610, 0x2058);
	if (ret)
		return ret;

	return 0;
}

static void rtl8372n_port_disable(struct dsa_switch *ds, int port)
{
    struct rtl837x_priv *priv = ds->priv;

	if (IS_SERDES_PORT(port))
		return;

	priv->ops->phy_write_c45(priv, port, 31, 0xa610, 0x2858);
}

static int
rtl8372n_port_fdb_static_add(struct rtl837x_priv *priv, int port,
		    const unsigned char *addr, u16 vid)
{
	int ret;
	struct rtl837x_lut_entry entry = {0};

	memcpy(entry.uc.key.mac_addr, addr, ETH_ALEN);
	entry.type = LUT_TYPE_L2_UC;
	entry.uc.key.vid_fid = vid;
	entry.uc.key.ivl = true;

	// age shoule greater than 0, so that the static entry will not be flushed by hardware
	entry.uc.age = 6;
	entry.uc.port = port;
	entry.uc.is_static = true;

	ret = rtl837x_lut_set(priv, &entry);
	if (ret == -ENOENT)
	{
		dev_dbg(priv->dev, "[%s]:addfailed mac:%02X:%02X:%02X:%02X:%02X:%02X port:%d vid:%04d\n", __func__,
						addr[0], addr[1], addr[2], addr[3], addr[4], addr[5],
						port, vid);
		return -ENOSPC;
	}
	dev_dbg(priv->dev, "[%s]:addsucceed mac:%02X:%02X:%02X:%02X:%02X:%02X port:%d vid:%04d addr: %04d\n", __func__,
					addr[0], addr[1], addr[2], addr[3], addr[4], addr[5],
					port, vid, entry.addr);
	return ret;
}

static int
rtl8372n_port_fdb_static_del(struct rtl837x_priv *priv,
		    const unsigned char *addr, u16 vid)
{
	int ret;
	struct rtl837x_lut_entry entry = {0};

	memcpy(entry.uc.key.mac_addr, addr, ETH_ALEN);
	entry.type = LUT_TYPE_L2_UC;
	entry.uc.key.vid_fid = vid;
	entry.uc.key.ivl = true;

	// query the entry in lut table
	ret = rtl837x_lut_query(priv, LUT_READ_METHOD_MAC, &entry);
	if (ret == -ENOENT)
	{
		dev_dbg(priv->dev, "[%s]:notfound mac:%02X:%02X:%02X:%02X:%02X:%02X vid:%04d\n", __func__,
						addr[0], addr[1], addr[2], addr[3], addr[4], addr[5],
						vid);
		// Not found in lut table
		// just return success
		return 0;
	}
	dev_dbg(priv->dev, "[%s]:deleted mac:%02X:%02X:%02X:%02X:%02X:%02X vid:%04d addr: %04d\n", __func__,
					addr[0], addr[1], addr[2], addr[3], addr[4], addr[5],
					vid, entry.addr);
	return rtl837x_lut_del(priv, entry.addr);
}

static int
rtl8372n_port_fdb_add(struct dsa_switch *ds, int port,
		    const unsigned char *addr, u16 vid,
		    struct dsa_db db)
{
	struct rtl837x_priv *priv = ds->priv;
	int ret;

	if (db.type != DSA_DB_PORT && db.type != DSA_DB_BRIDGE)
		return -EOPNOTSUPP;

	dev_dbg(priv->dev, "[%s]:type: %s port:%d vid:%04d\n", __func__,
					db.type == DSA_DB_PORT ? "DSA_DB_PORT" : "DSA_DB_BRIDGE",
					port, vid);

	if (db.type == DSA_DB_BRIDGE && priv->tag_proto == DSA_TAG_PROTO_MXL862_8021Q)
	{
		//Because the CPU port can only learn 802.1q tags, So VLAN tags are ignored
		if (vid != 0)
			return 0;
		/*
		* When the DSA tag protocol is DSA_TAG_PROTO_MXL862_8021Q, the L2 VLAN
		* learned on the CPU port is the 802.1q tag rather than the port's own
		* VLAN ID. Therefore, write the 802.1q tags of all ports into the
		* static FDB entries.
		*/
		struct dsa_port *dp;
		dsa_switch_for_each_user_port(dp, ds) {
			/* Add static fdb entry */
			ret = rtl8372n_port_fdb_static_add(priv, port, addr, dsa_tag_8021q_standalone_vid(dp));
			if (ret)
				return ret;
		}
	} else
	{
		return rtl8372n_port_fdb_static_add(priv, port, addr, vid);
	}
	return 0;
}

static int
rtl8372n_port_fdb_del(struct dsa_switch *ds, int port,
		    const unsigned char *addr, u16 vid,
		    struct dsa_db db)
{
	struct rtl837x_priv *priv = ds->priv;
	int ret;

	if (db.type != DSA_DB_PORT && db.type != DSA_DB_BRIDGE)
		return -EOPNOTSUPP;

	if (dsa_fdb_present_in_other_db(ds, port, addr, vid, db))
		return 0;

	if (db.type == DSA_DB_BRIDGE && priv->tag_proto == DSA_TAG_PROTO_MXL862_8021Q)
	{
		if (vid != 0)
			return 0;
		struct dsa_port *dp;
		dsa_switch_for_each_user_port(dp, ds) {
			/* Del static fdb entry */
			ret = rtl8372n_port_fdb_static_del(priv, addr, dsa_tag_8021q_standalone_vid(dp));
			if (ret)
				return ret;
		}
	} else
	{
		return rtl8372n_port_fdb_static_del(priv, addr, vid);
	}
	return 0;
}

static int
rtl8372n_port_fdb_dump(struct dsa_switch *ds, int port,
		     dsa_fdb_dump_cb_t *cb, void *data)
{
	struct rtl837x_priv *priv = ds->priv;
	int ret;
	struct rtl837x_lut_entry entry = {0};

	for (int i = 0; i < RTL8372N_LUT_MAX; i++) {
		entry.addr = i;
		entry.uc.port = port;
		ret = rtl837x_lut_query(priv, LUT_READ_METHOD_NEXT_L2UCSPA, &entry);
		if (ret)
			break;
		if (entry.addr < i)
			break;
		/* We need to hide the dsa_8021q VLANs from the user. */
		if (vid_is_dsa_8021q(entry.uc.key.vid_fid) && priv->tag_proto == DSA_TAG_PROTO_MXL862_8021Q)
			entry.uc.key.vid_fid = 0;
		ret = cb(entry.uc.key.mac_addr, entry.uc.key.vid_fid, entry.uc.is_static,
				data);
		if (ret < 0)
			break;
		i = entry.addr;
	}
	return 0;
}

static int
rtl8372n_port_mdb_add(struct dsa_switch *ds, int port,
		    const struct switchdev_obj_port_mdb *mdb,
		    struct dsa_db db)
{
	struct rtl837x_priv *priv = ds->priv;
	int ret;
	const u8 *addr = mdb->addr;
	u16 vid = mdb->vid;
	struct rtl837x_lut_entry entry = {0};

	memcpy(entry.mc.key.mac_addr, addr, ETH_ALEN);
	entry.type = LUT_TYPE_L2_MC;
	entry.mc.key.vid_fid = vid;
	entry.mc.key.ivl = true;
	ret = rtl837x_lut_query(priv, LUT_READ_METHOD_MAC, &entry);
	if (ret != -ENOENT && ret != 0)
		return ret;

	if (ret == 0)
	{
		dev_dbg(priv->dev, "[%s]:found mac:%02X:%02X:%02X:%02X:%02X:%02X port:%d vid:%04d addr:%04d\n", __func__,
						addr[0], addr[1], addr[2], addr[3], addr[4], addr[5],
						port, vid, entry.addr);
		if (entry.type != LUT_TYPE_L2_MC)
		{
			dev_err(priv->dev, "[%s]: got an unexpect entry from lut table, this should not happen\n", __func__);
			return -EINVAL;
		}
	}

	entry.mc.mbr |= BIT(port);
	ret = rtl837x_lut_set(priv, &entry);
	if (ret == -ENOENT)
	{
		dev_dbg(priv->dev, "[%s]:addfailed mac:%02X:%02X:%02X:%02X:%02X:%02X port:%d vid:%04d\n", __func__,
						addr[0], addr[1], addr[2], addr[3], addr[4], addr[5],
						port, vid);
		return -ENOSPC;
	}
	dev_dbg(priv->dev, "[%s]:addsucceed mac:%02X:%02X:%02X:%02X:%02X:%02X port:%d vid:%04d addr: %04d\n", __func__,
					addr[0], addr[1], addr[2], addr[3], addr[4], addr[5],
					port, vid, entry.addr);
	return ret;
}

static int
rtl8372n_port_mdb_del(struct dsa_switch *ds, int port,
		    const struct switchdev_obj_port_mdb *mdb,
		    struct dsa_db db)
{
	struct rtl837x_priv *priv = ds->priv;
	int ret;
	const u8 *addr = mdb->addr;
	u16 vid = mdb->vid;
	struct rtl837x_lut_entry entry = {0};
	dev_dbg(priv->dev, "[%s]:mac:%02X:%02X:%02X:%02X:%02X:%02X port:%d vid:%04d\n", __func__,
					addr[0], addr[1], addr[2], addr[3], addr[4], addr[5],
					port, vid);

	memcpy(entry.mc.key.mac_addr, addr, ETH_ALEN);
	entry.type = LUT_TYPE_L2_MC;
	entry.mc.key.vid_fid = vid;
	entry.mc.key.ivl = true;
	ret = rtl837x_lut_query(priv, LUT_READ_METHOD_MAC, &entry);
	if (ret == -ENOENT)
		return 0;

	if (ret == 0)
	{
		dev_dbg(priv->dev, "[%s]:found mac:%02X:%02X:%02X:%02X:%02X:%02X port:%d vid:%04d addr:%04d\n", __func__,
						addr[0], addr[1], addr[2], addr[3], addr[4], addr[5],
						port, vid, entry.addr);
		if (entry.type != LUT_TYPE_L2_MC)
		{
			dev_err(priv->dev, "[%s]: got an unexpect entry from lut table, this should not happen\n", __func__);
			return -EINVAL;
		}
	}else
		return ret;

	entry.mc.mbr &= ~BIT(port);
	if (!entry.mc.mbr)
	{
		return rtl837x_lut_del(priv, entry.addr);
	}

	ret = rtl837x_lut_set(priv, &entry);
	if (ret == -ENOENT)
	{
		dev_dbg(priv->dev, "[%s]:delfailed mac:%02X:%02X:%02X:%02X:%02X:%02X port:%d vid:%04d\n", __func__,
						addr[0], addr[1], addr[2], addr[3], addr[4], addr[5],
						port, vid);
		return -ENOSPC;
	}
	dev_dbg(priv->dev, "[%s]:delsuceed mac:%02X:%02X:%02X:%02X:%02X:%02X port:%d vid:%04d addr: %04d\n", __func__,
					addr[0], addr[1], addr[2], addr[3], addr[4], addr[5],
					port, vid, entry.addr);
	return ret;
}

enum RTL8373_FLUSHMODE
{
    FLUSHMDOE_PORT = 0,
    FLUSHMDOE_VID,
    FLUSHMDOE_FID,
    FLUSHMDOE_END,
};

static void rtl8372n_port_fast_age(struct dsa_switch *ds, int port)
{
	struct rtl837x_priv *priv = ds->priv;
	int ret;
	u32 tmp;

	// check busy
	ret = regmap_read_poll_timeout(priv->map, RTL8373_L2_TBL_FLUSH_CMD_ADDR, tmp,
		  ((tmp & RTL8373_L2_TBL_FLUSH_CMD_FLUSH_BUSY_MASK) == 0),
		  10, 1000);
	if (ret)
		return;

	ret = rtl837x_reg_bits_write(priv, RTL8373_L2_TBL_FLUSH_MODE_ADDR,
					  RTL8373_L2_TBL_FLUSH_MODE_FLUSH_MODE_MASK, FLUSHMDOE_PORT);
	if (ret)
		return;
	ret = rtl837x_reg_bits_write(priv, RTL8373_L2_TBL_FLUSH_CMD_ADDR,
					  RTL8373_L2_TBL_FLUSH_CMD_FLUSH_PMSK_MASK, BIT(port));
	if (ret)
		return;

	ret = rtl837x_reg_bits_write(priv, RTL8373_L2_TBL_FLUSH_CMD_ADDR,
					  RTL8373_L2_TBL_FLUSH_CMD_FLUSH_ACT_MASK, 1);
	if (ret)
		return;

	// check busy
	regmap_read_poll_timeout(priv->map, RTL8373_L2_TBL_FLUSH_CMD_ADDR, tmp,
		  ((tmp & RTL8373_L2_TBL_FLUSH_CMD_FLUSH_BUSY_MASK) == 0),
		  10, 1000);
	return;
}

enum RTL8373_MSTP_STATE
{
    MSTP_DISABLE = 0,
    MSTP_BLOCKING,
    MSTP_LEARNING,
    MSTP_FORWARDING,
    MSTP_END
};

#define RTL8373_STP_STATE(port, state) \
	((state) << ((port) * 2))
#define RTL8373_STP_STATE_MASK(port) \
	RTL8373_STP_STATE((port), GENMASK(1, 0))

static void rtl8372n_port_stp_state_set(struct dsa_switch *ds, int port, u8 state)
{
	struct rtl837x_priv *priv = ds->priv;
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
		rtl837x_reg_bits_write(priv, RTL8373_MSPT_STATE_ADDR(i),
				  RTL8373_STP_STATE_MASK(port), val
				);
	}
}

static int rtl8372n_setup(struct dsa_switch *ds)
{
    int ret;
    struct rtl837x_priv *priv = ds->priv;
	struct device_node *np = priv->dev->of_node;
	struct rtl8372n *chip_data = priv->chip_data;
	struct dsa_port *cpu_dp = NULL;
	struct dsa_port *dp;
	u32 downports_mask = 0,
		cpu_port_mask = 0;

	int cpu_dp_cnt = 0;
	dsa_switch_for_each_cpu_port(dp, ds) {
		cpu_port_mask |= BIT(dp->index);
		cpu_dp = dp;
		cpu_dp_cnt++;
	}

	// TODO: muilt CPU port support
	if (cpu_dp_cnt > 1)
	{
		dev_err(priv->dev,"We only support one cpu port now\n");
		return -ENODEV;
	}

	if (!cpu_dp) {
		dev_err(priv->dev,"No CPU port found\n");
		return -ENODEV;
	}

	chip_data->pcs[3].pcs.ops = &rtl8372n_sds_pcs_ops;
#if LINUX_VERSION_CODE < KERNEL_VERSION(6,18,0)
	chip_data->pcs[3].pcs.neg_mode = true;
#endif
	chip_data->pcs[3].priv = priv;
	chip_data->pcs[3].index = 3;

	chip_data->pcs[8].pcs.ops = &rtl8372n_sds_pcs_ops;
#if LINUX_VERSION_CODE < KERNEL_VERSION(6,18,0)
	chip_data->pcs[8].pcs.neg_mode = true;
#endif
	chip_data->pcs[8].priv = priv;
	chip_data->pcs[8].index = 8;

	// set port 3 and port 8 as serdes port
	rtl837x_reg_bits_write(priv, RTL8373_SMI_MAC_TYPE_CTRL_ADDR, 
			 RTL8373_SMI_MAC_TYPE_CTRL_MAC_PORT8_TYPE_MASK | RTL8373_SMI_MAC_TYPE_CTRL_MAC_PORT3_TYPE_MASK,
			 0
			);

	// set port4-7 polling internal resolution reg
	rtl837x_reg_bits_write(priv, RTL8373_SMI_PORT_POLLING_SEL_ADDR, 
			 RTL8373_SMI_PORT_POLLING_SEL_SMI_POLLING_SEL4_MASK | RTL8373_SMI_PORT_POLLING_SEL_SMI_POLLING_SEL5_MASK |
			  RTL8373_SMI_PORT_POLLING_SEL_SMI_POLLING_SEL6_MASK | RTL8373_SMI_PORT_POLLING_SEL_SMI_POLLING_SEL7_MASK,
			 0b1111
			);

	// enable SMI0/1/2 MDC clock output
	rtl837x_reg_bits_write(priv, RTL8373_SMI_CTRL_ADDR,
			 RTL8373_SMI_CTRL_SMI0_MDC_EN_MASK | RTL8373_SMI_CTRL_SMI1_MDC_EN_MASK | RTL8373_SMI_CTRL_SMI2_MDC_EN_MASK,
			 0b111
			);

	//  puts "Power down PHY 4~7"
	rtl837x_phys_write_c45(priv, 0xF0, 31, 0xa610, 0x2858);

	if (of_property_read_bool(np, "sds0-rx-swap"))
	{
		rtl837x_sds_reg_bits_write(priv, 0, 0, 0, 0x200, 1); //#SDS0RX PN swap
		rtl837x_sds_reg_bits_write(priv, 0, 6, 2, 0x2000, 1);
	}

	if (of_property_read_bool(np, "sds0-tx-swap"))
	{
		rtl837x_sds_reg_bits_write(priv, 0, 0, 0, 1 << 8, 1); //#SDS0RTX PN swap
		rtl837x_sds_reg_bits_write(priv, 0, 6, 2, 1 << 14, 1);
	}

	if (of_property_read_bool(np, "sds1-rx-swap"))
	{
		rtl837x_sds_reg_bits_write(priv, 1, 0, 0, 0x200, 1); //#SDS1RX PN swap
		rtl837x_sds_reg_bits_write(priv, 1, 6, 2, 0x2000, 1);
	}

	if (of_property_read_bool(np, "sds1-tx-swap"))
	{
		rtl837x_sds_reg_bits_write(priv, 1, 0, 0, 1 << 8, 1); //#SDS1TX PN swap
		rtl837x_sds_reg_bits_write(priv, 1, 6, 2, 1 << 14, 1);
	}

	rtl837x_sds_reset_R(priv, 1);
	msleep(5);
	rtl837x_sds_reset_R(priv, 0);
	msleep(5);

	/*
	  What The Fuck Is This??????? 
	  I can't understand this register design.

	  We have two PCB routing

	  RJ45 port pin mapping:
	    white Orange ->  0+
	          Orange ->  0-
	    white Green  ->  1+
	          Blue   ->  2+
	    white Blue   ->  2-
	          Green  ->  1-
	    white Brown  ->  3+
	          Brown  ->  3-
	  Chip side:
	  	A+ A- B+ B- C+ C- D+ D-

	  Case 1: No phy-mdi-reverse and no phy-tx-polarity-swap (normal connection)
	    A+ → 0+   A- → 0-   (Pair A → RJ45 Pair 0)
	    B+ → 1+   B- → 1-   (Pair B → RJ45 Pair 1)
	    C+ → 2+   C- → 2-   (Pair C → RJ45 Pair 2)
	    D+ → 3+   D- → 3-   (Pair D → RJ45 Pair 3)
	  Case 2: With phy-mdi-reverse and phy-tx-polarity-swap
	    A+ → 3+   A- → 3-   (Pair A → RJ45 Pair 3)
	    B+ → 2+   B- → 2-   (Pair B → RJ45 Pair 2)
	    C+ → 1-   C- → 1+   (Pair C → RJ45 Pair 1, with polarity REVERSED)
	    D+ → 0-   D- → 0+   (Pair D → RJ45 Pair 0, with polarity REVERSED)
	*/

    // ##MDI reverse configuration for Demo Tap UP RJ45, RTL8366U/RTL8373N/RTL8372N
	if (of_property_read_bool(np, "phy-mdi-reverse"))
		rtl837x_reg_bits_write(priv, RTL8373_CFG_PHY_MDI_REVERSE_ADDR, 
				  RTL8373_CFG_PHY_MDI_REVERSE_P0_MDI_REVERSE_MASK | RTL8373_CFG_PHY_MDI_REVERSE_P1_MDI_REVERSE_MASK |
				   RTL8373_CFG_PHY_MDI_REVERSE_P2_MDI_REVERSE_MASK | RTL8373_CFG_PHY_MDI_REVERSE_P3_MDI_REVERSE_MASK,
				  0xC
				);

	if (of_property_read_bool(np, "phy-tx-polarity-swap"))
		rtl837x_reg_bits_write(priv, RTL8373_CFG_PHY_TX_POLARITY_SWAP_ADDR,
				 RTL8373_CFG_PHY_TX_POLARITY_SWAP_P0_TX_POLARITY_SWAP_MASK | RTL8373_CFG_PHY_TX_POLARITY_SWAP_P1_TX_POLARITY_SWAP_MASK |
				  RTL8373_CFG_PHY_TX_POLARITY_SWAP_P2_TX_POLARITY_SWAP_MASK | RTL8373_CFG_PHY_TX_POLARITY_SWAP_P3_TX_POLARITY_SWAP_MASK,
				 0x596A
				); //#TX_POLARITY_SWAP

	//#cfg_FWD_INVLD_MAC_CTRL_EN,cfg_FWD_UNKN_OPCODE_EN
	rtl837x_reg_bits_write(priv, RTL8373_MAC_L2_GLOBAL_CTRL0_ADDR,
			 RTL8373_MAC_L2_GLOBAL_CTRL0_FWD_UNKN_OPCODE_EN_MASK | RTL8373_MAC_L2_GLOBAL_CTRL0_FWD_INVLD_MAC_CTRL_EN_MASK,
			 0b11
			);

	// #RS_LINK_FAULT_INDI_OFF=1 disable link fault flag, resolve port4-port7 linkdown dsc expand issue
    rtl837x_reg_bits_write(priv, RTL8373_RS_LAYER_CONFIG_ADDR,
		 RTL8373_RS_LAYER_CONFIG_RS_LINK_FAULT_INDI_OFF_MASK, 1
		);

	rtl837x_reg_bits_write(priv, RTL8373_DW8051_CFG_ADDR,
			 RTL8373_DW8051_CFG_DW8051_READY_MASK, 1
			);

#if defined(RTL837X_PHY_PATCH)
	if (priv->chip_ver == 2)
	{
		patch_phys_v008(priv, 0xf0);
		patch_phys_v008_rls_lockmain(priv, 0xf0);
	}
#endif

    //RTL8372/RTL8372N/RTL8366U set polling mask 0x1f8, port 3/8 from serdes need config bit8=1
    rtl837x_reg_bits_write(priv, RTL8373_SMI_GLB_CTRL_ADDR,
		 RTL8373_SMI_GLB_CTRL_SMI_POLLING_MASK_MASK, 0x1f8
		);
	msleep(5);


	dsa_switch_for_each_port(dp, ds) {
		ret = rtl8372n_port_remove_vlan_transparent(priv, dp->index, 0xffffffff);
		if(ret)
			return ret;
	}

	of_extra_init(ds);

    ret = rtl8372n_setup_mdio(priv);
	if(ret){
		dev_err(priv->dev, "rtl8372n_setup_mdio Fail, error:%d\n", ret);
		return ret;
	}

	// Reset vlan table
	ret = rtl837x_reg_bits_write(priv, RTL8373_VLAN_CTRL_ADDR, RTL8373_VLAN_CTRL_TABLE_RST_MASK, 1);
	if (ret)
		return ret;

	dsa_switch_for_each_port(dp, ds) {
		int port = dp->index;

        ret = rtl837x_reg_write(priv, RTL8373_FC_PORT_ACT_CTRL_ADDR(port), 0x1050);
		if (ret)
			return ret;

		ret = rtl837x_reg_bits_write(priv, RTL8373_MAC_L2_PORT_CTRL_ADDR(port),
			  RTL8373_MAC_L2_PORT_CTRL_RX_CHK_CRC_EN_MASK, 1
			);
		if (ret)
			return ret;

		ret = rtl837x_reg_bits_write(priv, RTL8373_MAC_L2_PORT_CTRL_ADDR(port),
			  RTL8373_MAC_L2_PORT_CTRL_CLOCK_SWITCH_MASK, 1
			);
		if (ret)
			return ret;

		rtl8372n_port_stp_state_set(ds, port, BR_STATE_DISABLED);

		/* Start with all port completely isolated */
		ret = rtl8372n_port_set_isolation(priv, port, 0);
		if (ret)
			return ret;

		if (dsa_port_is_unused(dp))
			continue;

		ret = rtl8372n_set_pvid(priv, port, 0xfff);
		if (ret)
			return ret;

		// FORWARD:0, DROP:1, TO_CPU:2
		ret = rtl837x_reg_bits_write(priv, RTL8373_L2_LRN_PORT_CONSTRT_ACT_ADDR,
			 RTL8373_L2_LRN_PORT_CONSTRT_ACT_LRN_ACT_MASK, 0
			);
		if (ret)
			return ret;

		ret = rtl8372n_port_vlan_tag_rewrite(priv, port, false);
		if (ret)
			return ret;

		// Accept untaged Frame
		ret = rtl8372n_drop_untagged(priv, port, false);
		if (ret)
			return ret;

		// Disable Ingress filter
		ret = rtl837x_reg_bits_write(priv, RTL8373_VLAN_PORT_IGR_FLTR_ADDR(port),
			  RTL8373_VLAN_PORT_IGR_FLTR_IGR_FLTR_ACT_MASK(port), 0
			);
		if (ret)
			return ret;

		// Disable port EEE feature
		ret = rtl837x_reg_bits_write(priv, RTL8373_EEE_CTRL_ADDR(port), 
				  RTL8373_EEE_CTRL_EEE_PORT_TX_EN_MASK | RTL8373_EEE_CTRL_EEE_PORT_RX_EN_MASK,
				  0
				);
		if (ret)
			return ret;

		// Enable backpressure
		ret = rtl837x_reg_bits_write(priv, RTL8373_MAC_PORT_CTRL_ADDR(port),
				  RTL8373_MAC_PORT_CTRL_BKPRES_EN_MASK, 1
				);
		if (ret)
			return ret;

		if (dsa_is_cpu_port(ds, port))
		{
			ret = rtl8372n_bridge_port_add_resv_vlan(priv, port);
			if (ret)
				return ret;
		}

		if (!dsa_port_is_user(dp))
			continue;

		ret = rtl8372n_port_add_cpu_vlan_transparent(priv, port);
		if (ret)
			return ret;

		/* Forward only to the CPU */
		ret = rtl8372n_port_set_isolation(priv, dp->index,
						   BIT(cpu_dp->index));
		if (ret)
			return ret;

		downports_mask |= BIT(dp->index);
	}

	ret = rtl8372n_port_set_isolation(priv, cpu_dp->index,
						downports_mask);
	if (ret)
		return ret;

	/*
	 * Warning! 
	 * The following understanding may be incorrect
	 * 
	 * !!! The annotation is outdated !!!
	*/

	/*
	 * So What is this.
	 * We use vlan1 to forward the untag frame in the bridge
	 * If the CPU wants to send a frame to a port and the frame does not have a VLAN tag
	 * The switch will insert VLAN1 into the frame and remove the VLAN1 when sending the frame
	 * The same applies to frames entering through ports
	 * 
	 * 
	 * If a frame send from CPU (CPU->switch) whithout cvid only with a DSA TAG
	 * ================================================================================
	 * --------------------------------------------------------------------------------
	 *                                 SVLAN Process
	 *  |DMAC|SMAC|DSA TAG(SVLAN)|...| -------------> |DMAC|SMAC|...| ----..
	 *              remove dsa tag (maybe? or remove the tag before the frame sent out?)
	 *                              mark destination port
	 *                                  
	 * --------------------------------------------------------------------------------         
	 *       CVLAN process                            Send to dest port       
	 * ..---------------------> |DMAC|SMAC|CVLAN|...| -----------------> |DMAC|SMAC|...|
	 *  frame with out CVLAN tag                     remove the CVLAN tag
	 * mark the cpuport pvid(1)
	 * we use vid 1(1) to forward the
	 *     no CVLAN frame
	 * --------------------------------------------------------------------------------
	 * ================================================================================
	 * 
	 * If a frame send from CPU (CPU->switch) whith DSA tag(SVLAN) and CVLAN
	 * ================================================================================
	 * --------------------------------------------------------------------------------
	 *                                       SVLAN Process
	 *  |DMAC|SMAC|DSA TAG(SVLAN)|CVLAN|...| -------------> |DMAC|SMAC|CVLAN|...| ----..
	 *                                       remove dsa tag
	 *                                    mark destination port
	 * --------------------------------------------------------------------------------         
	 *       CVLAN process                            Send to dest port       
	 * ..---------------------> |DMAC|SMAC|CVLAN|...| -----------------> |DMAC|SMAC|CVLAN(may not exist)|...|
	 *  frame already with CVLAN tag             remove or keep the CVLAN tag
	 * 
	 * --------------------------------------------------------------------------------
	 * ================================================================================
	 * 
	 * If a frame send to CPU (switch->CPU) whithout vlan tag
	 * ================================================================================
	 * --------------------------------------------------------------------------------
	 *                  Port based vlan tag
	 *  |DMAC|SMAC|...| -------------------> |DMAC|SMAC|CVLAN|...| ----...
	 *               add the port based vlan id
	 * 
	 * --------------------------------------------------------------------------------         
	 *   SVLAN process
	 *  ..-----------------> |DMAC|SMAC|DSA TAG(SVLAN)|CVLAN|...|
	 * mark the src port svid
	 * 
	 * --------------------------------------------------------------------------------
	 * ================================================================================
	 * 
	 * If a frame send to CPU (switch->CPU) whit vlan tag
	 * ================================================================================
	 * --------------------------------------------------------------------------------
	 *                        Port Ingerss check
	 *  |DMAC|SMAC|CVLAN|...| -------------------> |DMAC|SMAC|CVLAN|...| ----...
	 *                         drop or forward
	 *                         
	 * --------------------------------------------------------------------------------         
	 *   SVLAN process
	 *  ..-----------------> |DMAC|SMAC|DSA TAG(SVLAN)|CVLAN|...|
	 * mark the src port svid
	 * 
	 * --------------------------------------------------------------------------------
	 * ================================================================================
	*/

	/*
	 * So when a port is not in the bridge
	 * what will happens
	 * 
	 * when the port is not in the bridge, it will enable vlan transparent on it
	 * This way, the port will not be restricted by the VLAN table
	 * the frame will only add/remove the DSA TAG(SVLAN), and there will be no changes to CVLAN
	 * 
	 * If a frame send to CPU (switch->CPU)
	 * ================================================================================
	 * --------------------------------------------------------------------------------
	 *                                Port Ingerss check
	 *  |DMAC|SMAC|CVLAN(or not)|...| -------------------> |DMAC|SMAC|CVLAN(or not)|...| ----...
	 *                                 check is disabled
	 *                               all frames can came in
	 * --------------------------------------------------------------------------------         
	 *   SVLAN process
	 *  ..-----------------> |DMAC|SMAC|DSA TAG(SVLAN)|CVLAN(or not)|...|
	 * mark the src port svid
	 * 
	 * --------------------------------------------------------------------------------
	 * ================================================================================
	 * 
	 * If a frame send from CPU (CPU->switch)
	 * ================================================================================
	 * --------------------------------------------------------------------------------
	 *                                               SVLAN Process
	 *  |DMAC|SMAC|DSA TAG(SVLAN)|CVLAN(or not)|...| -------------> |DMAC|SMAC|CVLAN(or not)|...| ----..
	 *                                              remove dsa tag
	 *                                           mark destination port
	 * --------------------------------------------------------------------------------         
	 *       CVLAN process                                    Send to dest port       
	 * ..---------------------> |DMAC|SMAC|CVLAN(or not)|...| -----------------> |DMAC|SMAC|CVLAN((or not))|...|
	 *        do nothing
	 * --------------------------------------------------------------------------------
	 * ================================================================================
	*/

	// Set external CPU port
	ret = rtl837x_reg_bits_write(priv, RTL8373_EXT_CPU_CTRL_ADDR,
			  RTL8373_EXT_CPU_CTRL_PORT_MASK, cpu_dp->index
			);
	if (ret)
		return ret;

	ret = rtl837x_reg_bits_write(priv, RTL8373_L2_TBL_FLUSH_ALL_ADDR,
				  RTL8373_L2_TBL_FLUSH_ALL_FLUSH_ALL_MASK, 1);
	if (ret)
		return ret;

	ret = rtl837x_reg_write(priv, RTL8373_VLAN_L2_LRN_DIS_ADDR(0), 0);
	if (ret)
		return ret;

	ret = rtl837x_reg_write(priv, RTL8373_VLAN_L2_LRN_DIS_ADDR(1), 0);
	if (ret)
		return ret;

	// FORWARD:0, DROP:1, TO_CPU:2
	ret = rtl837x_reg_bits_write(priv, RTL8373_L2_LRN_PORT_CONSTRT_ACT_ADDR,
			  RTL8373_L2_LRN_PORT_CONSTRT_ACT_LRN_ACT_MASK, 0
			);
	if (ret)
		return ret;

	// Enable vlan egrFilter
	ret = rtl837x_reg_bits_write(priv, RTL8373_VLAN_CTRL_ADDR,
			  RTL8373_VLAN_CTRL_CVLAN_FILTER_MASK, 1
			);
	if (ret)
		return ret;

	// Disable vlan leaky
	// Disable isolate leaky
	ret = rtl837x_reg_bits_write(priv, RTL8373_MIR_CTRL_ADDR,
			  RTL8373_MIR_CTRL_MIR_TX_VLAN_LKY_MASK | RTL8373_MIR_CTRL_MIR_RX_VLAN_LKY_MASK |
			  RTL8373_MIR_CTRL_MIR_TX_ISOLATE_LKY_MASK | RTL8373_MIR_CTRL_MIR_RX_ISOLATE_LKY_MASK,
			  0
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

    return 0;
}

static const struct dsa_switch_ops rtl8372n_switch_ops_mdio = {
	.get_tag_protocol = rtl8372n_get_tag_protocol,
	.change_tag_protocol = rtl8372n_change_tag_protocol,
	.setup = rtl8372n_setup,

	.phylink_get_caps = rtl8372n_phylink_get_caps,

	.get_strings = rtl8372n_get_strings,
	.get_ethtool_stats = rtl8372n_get_ethtool_stats,
	.get_sset_count = rtl8372n_get_sset_count,
	.get_eth_phy_stats = rtl8372n_get_phy_stats,
	.get_eth_mac_stats = rtl8372n_get_mac_stats,
	.get_eth_ctrl_stats = rtl8372n_get_ctrl_stats,

	.port_vlan_filtering = rtl8372n_vlan_filtering,
	.port_vlan_add = rtl8372n_vlan_add,
	.port_vlan_del = rtl8372n_vlan_del,

	.port_stp_state_set = rtl8372n_port_stp_state_set,

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

	.port_enable = rtl8372n_port_enable,
	.port_disable = rtl8372n_port_disable
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
	.have_8224 = false,
	.chip_data_sz = sizeof(struct rtl8372n),
};
EXPORT_SYMBOL_GPL(rtl8372n_variant);
