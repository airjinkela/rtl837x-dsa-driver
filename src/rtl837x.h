/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2025 StarField Xu <air_jinkela@163.com>
 */
#ifndef _RTL8372_COMMON_H
#define _RTL8372_COMMON_H

#include <linux/of_mdio.h>
#include <linux/regmap.h>
#include <linux/debugfs.h>
#include <linux/dsa/8021q.h>
#include <net/dsa.h>

#define RTL837x_C2SIDXMAX       127
#define RTL837x_FIDMAX          15
#define RTL837X_MAX_PORT_COUNT  9 /* 0~8 */
#define RTL837X_PORT_LED_COUNT  4
#define RTL837X_LED_SET_COUNT   4

/* Chip identification */
#define RTL837X_MODEL_NAME_INFO_ADDR                   0x4
#define RTL837X_CHIP_INFO_ADDR                         0xC
#define   RTL837X_CHIP_INFO_RL_VID_MASK                GENMASK(31, 28)
#define   RTL837X_CHIP_INFO_CHIP_INFO_EN_MASK          GENMASK(19, 16)


/* PHY configuration */
#define RTL837X_CFG_PHY_MDI_REVERSE_ADDR                              0xA90
#define   RTL837X_CFG_PHY_MDI_REVERSE_P3_MDI_REVERSE_MASK             BIT(3)
#define   RTL837X_CFG_PHY_MDI_REVERSE_P2_MDI_REVERSE_MASK             BIT(2)
#define   RTL837X_CFG_PHY_MDI_REVERSE_P1_MDI_REVERSE_MASK             BIT(1)
#define   RTL837X_CFG_PHY_MDI_REVERSE_P0_MDI_REVERSE_MASK             BIT(0)
#define RTL837X_CFG_PHY_TX_POLARITY_SWAP_ADDR                         0xA94
#define   RTL837X_CFG_PHY_TX_POLARITY_SWAP_P3_TX_POLARITY_SWAP_MASK   GENMASK(15, 12)
#define   RTL837X_CFG_PHY_TX_POLARITY_SWAP_P2_TX_POLARITY_SWAP_MASK   GENMASK(11, 8)
#define   RTL837X_CFG_PHY_TX_POLARITY_SWAP_P1_TX_POLARITY_SWAP_MASK   GENMASK(7, 4)
#define   RTL837X_CFG_PHY_TX_POLARITY_SWAP_P0_TX_POLARITY_SWAP_MASK   GENMASK(3, 0)
#define RTL837X_RS_LAYER_CONFIG_ADDR                                  0xB7C
#define   RTL837X_RS_LAYER_CONFIG_RS_LINK_FAULT_INDI_OFF_MASK         BIT(5)
#define RTL837X_EEE_LPI_DLY_CYCLE_ADDR                                0x954
#define   RTL837X_EEE_LPI_DLY_CYCLE_CFG_WATER_LEVEL_ST_MASK           GENMASK(13, 8)
#define   RTL837X_EEE_LPI_DLY_CYCLE_TX_LPI_DLY_CYCLE_MASK             GENMASK(7, 4)
#define   RTL837X_EEE_LPI_DLY_CYCLE_RX_LPI_DLY_CYCLE_MASK             GENMASK(3, 0)

/* Internal SerDes */
#define RTL837X_SDS_MODE_SEL_ADDR                          0x7B20
#define   RTL837X_SDS_MODE_SEL_CFG_MAC8_8221B_MASK         BIT(22)
#define   RTL837X_SDS_MODE_SEL_CFG_MAC3_8221B_MASK         BIT(21)
#define   RTL837X_SDS_MODE_SEL_SDS1_USX_SUB_MODE_MASK      GENMASK(20, 16)
#define   RTL837X_SDS_MODE_SEL_SDS0_USX_SUB_MODE_MASK      GENMASK(14, 10)
#define   RTL837X_SDS_MODE_SEL_SDS1_MODE_SEL_MASK          GENMASK(9, 5)
#define   RTL837X_SDS_MODE_SEL_SDS0_MODE_SEL_MASK          GENMASK(4, 0)
#define RTL837X_SDS_INDACS_CMD_ADDR                        0x3F8
#define   RTL837X_SDS_INDACS_CMD_SDS_CMD_MASK              BIT(15)
#define   RTL837X_SDS_INDACS_CMD_SDS_RWOP_MASK             BIT(14)
#define   RTL837X_SDS_INDACS_CMD_SDS_REGAD_MASK            GENMASK(11, 7)
#define   RTL837X_SDS_INDACS_CMD_SDS_PAGE_MASK             GENMASK(6, 1)
#define   RTL837X_SDS_INDACS_CMD_SDS_INDEX_MASK            BIT(0)
#define RTL837X_SDS_INDACS_RD_ADDR                         0x3FC
#define RTL837X_SDS_INDACS_WD_ADDR                         0x400

/* MIB Control */
#define RTL837X_INDIRECT_ACCESS_CTRL_ADDR                     0xF60
#define   RTL837X_INDIRECT_ACCESS_CTRL_MIB_ID_MASK            GENMASK(10, 5)
#define   RTL837X_INDIRECT_ACCESS_CTRL_PORT_ID_MASK           GENMASK(4, 1)
#define   RTL837X_INDIRECT_ACCESS_CTRL_ACC_CMD_MASK           BIT(0)
#define RTL837X_INDIRECT_ACCESS_CNT_L_ADDR                    0xF64
#define RTL837X_INDIRECT_ACCESS_CNT_H_ADDR                    0xF68

/* Port MAC, link status */
#define RTL837X_MAC_PORT_CTRL_ADDR(_p)                          (0x122C + (((_p) << 8)))
#define   RTL837X_MAC_PORT_CTRL_BKPRES_EN_MASK                  BIT(0)

#define RTL837X_MAC_LINK_STS_ADDR                               0x63E8
#define RTL837X_MAC_LINK_SPD_STS_ADDR(_p)                       (0x63F0 + (((_p >> 3) << 2)))
#define   RTL837X_MAC_LINK_SPD_STS_SPD_STS_9_0_MASK(_p)         (GENMASK(3, 0) << ((_p & 0x7) << 2))
#define RTL837X_MAC_LINK_DUP_STS_ADDR                           0x63F8
#define RTL837X_MAC_TX_PAUSE_STS_ADDR                           0x63FC
#define RTL837X_MAC_RX_PAUSE_STS_ADDR                           0x6400
#define RTL837X_SMI_CTRL_ADDR                                   0x6454
#define   RTL837X_SMI_CTRL_SMI2_MDC_EN_MASK                     BIT(14)
#define   RTL837X_SMI_CTRL_SMI1_MDC_EN_MASK                     BIT(13)
#define   RTL837X_SMI_CTRL_SMI0_MDC_EN_MASK                     BIT(12)

/* Internal phy SMI access */
#define RTL837X_SMI_ACCESS_PHY_CTRL_0_ADDR                      0x6438
#define RTL837X_SMI_ACCESS_PHY_CTRL_1_ADDR                      0x643C
#define   RTL837X_SMI_ACCESS_PHY_CTRL_1_FAIL_MASK               GENMASK(26, 24)
#define   RTL837X_SMI_ACCESS_PHY_CTRL_1_MMD_DEVAD_4_0_MASK      GENMASK(23, 19)
#define   RTL837X_SMI_ACCESS_PHY_CTRL_1_MMD_REG_15_0_MASK       GENMASK(18, 3)
#define   RTL837X_SMI_ACCESS_PHY_CTRL_1_RWOP_MASK               BIT(2)
#define   RTL837X_SMI_ACCESS_PHY_CTRL_1_TYPE_MASK               BIT(1)
#define   RTL837X_SMI_ACCESS_PHY_CTRL_1_CMD_MASK                BIT(0)
#define RTL837X_SMI_ACCESS_PHY_CTRL_2_ADDR                      0x6440
#define   RTL837X_SMI_ACCESS_PHY_CTRL_2_DATA_15_0_MASK          GENMASK(15, 0)
#define RTL837X_SMI_ACCESS_PHY_CTRL_3_ADDR                      0x6444
#define   RTL837X_SMI_ACCESS_PHY_CTRL_3_INDATA_15_0_MASK        GENMASK(15, 0)

/* Internal PHY OCP access */
#define RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_0_ADDR                                  0xBC8
#define   RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_ADDR_MASK        GENMASK(31, 16)
#define   RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_PHYADR_MASK      GENMASK(8, 4)
#define   RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_FAIL_MASK        BIT(2)
#define   RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_RW_MASK          BIT(1)
#define   RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_0_INT_PHY_OCP_INDACC_CMD_MASK         BIT(0)
#define RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_1_ADDR                                  0xBCC
#define   RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_1_INT_PHY_OCP_INDACC_RDDATA_MASK      GENMASK(15, 0)
#define RTL837X_INT_PHY_OCP_INDR_ACC_CTRL_2_ADDR                                  0xBD0

/* L2 learning control */
#define RTL837X_MAC_L2_GLOBAL_CTRL0_ADDR                          0x5FD4
#define   RTL837X_MAC_L2_GLOBAL_CTRL0_FWD_INVLD_MAC_CTRL_EN_MASK  BIT(20)
#define   RTL837X_MAC_L2_GLOBAL_CTRL0_FWD_UNKN_OPCODE_EN_MASK     BIT(19)
#define RTL837X_MAC_L2_PORT_CTRL_ADDR(_p)                         (0x1238 + (((_p) << 8)))
#define   RTL837X_MAC_L2_PORT_CTRL_CLOCK_SWITCH_MASK              BIT(8)
#define   RTL837X_MAC_L2_PORT_CTRL_RX_CHK_CRC_EN_MASK             BIT(4)

/* EEE control */
#define RTL837X_EEE_CTRL_ADDR(_p)                   (0x125C + (((_p) << 8)))
#define   RTL837X_EEE_CTRL_EEE_RX_STS_MASK          BIT(3)
#define   RTL837X_EEE_CTRL_EEE_TX_STS_MASK          BIT(2)
#define   RTL837X_EEE_CTRL_EEE_PORT_TX_EN_MASK      BIT(1)
#define   RTL837X_EEE_CTRL_EEE_PORT_RX_EN_MASK      BIT(0)

/* CPU tagging */
#define RTL837X_CPU_TAG_CTRL_ADDR                           0x6720
#define   RTL837X_CPU_TAG_CTRL_EXT_CPUTAG_INSERTMOD_MASK    GENMASK(11, 10)
#define   RTL837X_CPU_TAG_CTRL_EXT_CPUTAG_EN_MASK           BIT(1)
#define RTL837X_EXT_CPU_CTRL_ADDR                           0x6724
#define   RTL837X_EXT_CPU_CTRL_PORT_MASK                    GENMASK(3, 0)
#define RTL837X_CPU_TAG_AWARE_CTRL_ADDR                     0x603C


/* VLAN and VLAN stacking */
#define RTL837X_VLAN_CTRL_ADDR                                   0x4E14
#define   RTL837X_VLAN_CTRL_TABLE_RST_MASK                       BIT(3)
#define   RTL837X_VLAN_CTRL_CVLAN_FILTER_MASK                    BIT(2)

#define RTL837X_VS_GLB_CTRL_ADDR                                 0x6044
#define RTL837X_VS_UPLINK_PORT_ADDR                              0x57C0
#define RTL837X_VS_CTRL_ADDR                                     0x57C4
#define   RTL837X_VS_CTRL_SPRISEL_MASK                           GENMASK(4, 3)
#define   RTL837X_VS_CTRL_UIFSEG_MASK                            BIT(2)
#define   RTL837X_VS_CTRL_UNTAG_MASK                             GENMASK(1, 0)
#define RTL837X_VS_PORT_DFLT_SVID_ADDR(_p)                       (0x57CC + (((_p >> 1) << 2)))
#define   RTL837X_VS_PORT_DFLT_SVID_PORT_DFLT_SVID_MASK(_p)      (GENMASK(11, 0) << ((_p & 0x1) * 12))
#define RTL837X_SVLAN_TRAP_CTRL_ADDR                             0x4EC8
#define   RTL837X_SVLAN_TRAP_CTRL_CPU_PMSK_MASK                  GENMASK(17, 16)
#define RTL837X_VLAN_C2S_ENTRY_ADDR(_i)                          (0x57E0 + (((_i) << 3))) /* RTL837x_C2SIDXMAX */

/* Spanning tree */
#define RTL837X_MSPT_STATE_ADDR(_fid)       (0x5310 + (((_fid) << 2))) /* RTL837x_FIDMAX */
#define RTL837X_MSTP_STATE(port, state) \
	((state) << ((port) * 2))
#define RTL837X_MSTP_STATE_MASK(port) \
	RTL837X_MSTP_STATE((port), GENMASK(1, 0))

/* Isolation */
#define RTL837X_PORT_ISO_PORT_PMSK_ADDR(_p)                    (0x50C0 + (((_p) << 2)))
#define   RTL837X_PORT_ISO_PORT_PMSK_PMSK_MASK                 GENMASK(9, 0)

/* Mirroring */
#define RTL837X_MIR_CTRL_ADDR                                  0x50E8
#define   RTL837X_MIR_CTRL_MIR_RX_ISOLATE_LKY_MASK             BIT(6)
#define   RTL837X_MIR_CTRL_MIR_TX_ISOLATE_LKY_MASK             BIT(5)
#define   RTL837X_MIR_CTRL_MIR_RX_VLAN_LKY_MASK                BIT(4)
#define   RTL837X_MIR_CTRL_MIR_TX_VLAN_LKY_MASK                BIT(3)

/* Flow control */
#define RTL837X_FC_PORT_ACT_CTRL_ADDR(_p)                      (0x7124 + (((_p) << 2)))
#define   RTL837X_FC_PORT_ACT_CTRL_ACT_MASK                                                                   BIT(12)
#define   RTL837X_FC_PORT_ACT_CTRL_ALLOW_PAGE_CNT_MASK                                                        GENMASK(11, 0)

enum _rtk_tb_op {
	TB_OP_READ = 0,
	TB_OP_WRITE
};

enum _rtk_tb_access_execute {
	TB_NOT_EXECUTE = 0,
	TB_EXECUTE,
};

enum _rtk_tb_access_target {
	TB_TARGET_ACLRULE = 1,
	TB_TARGET_ACLACT,
	TB_TARGET_CVLAN,
	TB_TARGET_L2,
	TB_TARGET_IGMP_GROUP,
	TB_TARGET_HSA,
	TB_TARGET_HSB
};

enum switch_chip {
	CHIP_RTL8373 = 0,
	CHIP_RTL8224 = 1,
	CHIP_RTL8372,
	CHIP_RTL8373N,
	CHIP_RTL8221B,
	CHIP_RTL8366U,
	CHIP_RTL8372N,
	CHIP_RTL8224N,
	CHIP_END
};

struct rtl837x_mib_counter {
	unsigned int	offset;
	unsigned int	length;
	const char	*name;
};

struct rtl837x_led_set {
	struct rtl837x_priv *priv;
	u32 led_cfg_mask[RTL837X_PORT_LED_COUNT];
	atomic_t refcnt;
	u8 idx;
};

struct rtl837x_led {
	struct rtl837x_priv *priv;
	struct rtl837x_led_set *led_set;
	struct led_classdev cdev;
	bool is_hw_offload;
	bool active_low;
	u8 port_num;
	u8 led_pin;
	u8 led_id;
};

struct rtl837x_priv {
	struct device *dev;
	struct regmap		*map;
	struct mutex		map_lock;
	struct regmap		*map_nolock;
	struct mii_bus *bus;
	int	mdio_addr;

	struct gpio_desc	*reset;

	struct mutex		phy_mmd_lock;
	struct mutex		phy_ocp_lock;

	enum dsa_tag_protocol tag_proto;

	struct dentry *debugfs_parent;

	unsigned int num_ports;

	struct dsa_switch	*ds;

	const struct rtl837x_mib_counter *mib_counters;
	unsigned int num_mib_counters;
	struct mutex mib_lock;

	struct rtl837x_led ports_led[RTL837X_PORT_LED_COUNT*RTL837X_MAX_PORT_COUNT];
	struct rtl837x_led_set led_set[RTL837X_LED_SET_COUNT];

	int			(*write_reg_noack)(void *ctx, u32 addr, u32 data);
	const struct rtl837x_ops *ops;

	void			*chip_data; /* Per-chip extra variant data */
};

struct rtl837x_vlan_4k {
	u16	member;
	u16	untag;
	u16	vid;
	u8	fid;
};

struct rtl837x_variant {
	const enum dsa_tag_protocol def_tag_proto;
	const struct phylink_mac_ops *pl_mac_ops;
	const struct dsa_switch_ops *ds_ops_mdio;
	const struct rtl837x_ops *ops;
	size_t chip_data_sz;
};

struct rtl837x_ops {
	int	(*reset_chip)(struct rtl837x_priv *priv);
	int	(*detect)(struct rtl837x_priv *priv);

	int	(*get_mib_counter)(struct rtl837x_priv *priv,
					int port,
					const struct rtl837x_mib_counter *mib,
					u64 *mibvalue);

	int	(*get_vlan_4k)(struct rtl837x_priv *priv, u32 vid,
			       struct rtl837x_vlan_4k *vlan4k);
	int	(*set_vlan_4k)(struct rtl837x_priv *priv,
			       const struct rtl837x_vlan_4k *vlan4k);
	int	(*phy_read_c22)(struct rtl837x_priv *priv, u16 phy, int regnum,
				u16 *pval);
	int	(*phy_write_c22)(struct rtl837x_priv *priv, u16 phy, int regnum,
				u16 val);

	int	(*phy_read_c45)(struct rtl837x_priv *priv, int phy, int devad, int regnum,
				u16 *pval);
	int	(*phy_write_c45)(struct rtl837x_priv *priv, int phy, int devad, int regnum,
				u16 val);
};

char *chipid_to_chip_name(enum switch_chip id);

#define rtl837x_reg_read(priv, reg, pval) regmap_read(priv->map, reg, pval)
#define rtl837x_reg_write(priv, reg, val) regmap_write(priv->map, reg, val)

extern int rtl837x_reg_bits_read(struct rtl837x_priv *priv, u32 reg, u32 mask, u32 *pval);
extern int rtl837x_reg_bits_write(struct rtl837x_priv *priv, u32 reg, u32 mask, u32 val);

extern int rtl837x_set_led(struct rtl837x_priv *priv);
extern int rtl837x_get_chip_ver(struct rtl837x_priv *priv, u16 *chip_ver);

extern int rtl837x_debug_proc_init(struct rtl837x_priv *priv);
extern int rtl837x_debug_proc_deinit(struct rtl837x_priv *priv);

extern int rtl837x_phy_read_ocp(struct rtl837x_priv *priv, u16 phy, int regnum, u16 *pval);
extern int rtl837x_phy_write_ocp(struct rtl837x_priv *priv, u16 phy, int regnum, u16 val);
extern int rtl837x_phy_read_c22(struct rtl837x_priv *priv, u16 phy, int regnum, u16 *pval);
extern int rtl837x_phy_write_c22(struct rtl837x_priv *priv, u16 phy, int regnum, u16 val);
extern int rtl837x_phy_read_c45(struct rtl837x_priv *priv, int phy, int devad, int regnum, u16 *pval);
extern int rtl837x_phys_write_c45(struct rtl837x_priv *priv, u16 phy_mask, int devad, int regnum, u16 val);
extern int rtl837x_phy_write_c45(struct rtl837x_priv *priv, int phy, int devad, int regnum, u16 val);
extern int rtl837x_phy_bits_read_c45(struct rtl837x_priv *priv, int phy, int devad, int regnum, u16 mask, u16 *pdata);
extern int rtl837x_phy_bits_write_c45(struct rtl837x_priv *priv, int phy, int devad, int regnum, u16 mask, u16 data);

extern int rtl837x_sds_reg_read(struct rtl837x_priv *priv, u8 sds_idx, u16 sds_page, u16 sds_reg, u16 *pdata);
extern int rtl837x_sds_reg_write(struct rtl837x_priv *priv, u8 sds_idx, u16 sds_page, u16 sds_reg, u16 data);
extern int rtl837x_sds_reg_bits_write(struct rtl837x_priv *priv, u8 sds_idx, u16 sds_page, u16 sds_reg, u16 mask, u16 data);
extern int rtl837x_sds_reg_bits_read(struct rtl837x_priv *priv, u8 sds_idx, u16 sds_page, u16 sds_reg, u16 mask, u16 *pdata);

extern const struct rtl837x_variant rtl8372n_variant;

#endif
