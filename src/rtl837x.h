/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2025 StarField Xu <air_jinkela@163.com>
 */
#ifndef __RTL8372_COMMON_H__
#define __RTL8372_COMMON_H__

#include <linux/of_mdio.h>
#include <linux/regmap.h>
#include <linux/debugfs.h>
#include <linux/dsa/8021q.h>
#include <net/dsa.h>

#include "rtl8373_reg_definition.h"

#define MDC_MDIO_CTRL_REG           21
#define MDC_MDIO_ADDR_REG           22
#define MDC_MDIO_DATA_LOW           23
#define MDC_MDIO_DATA_HIGH          24
#define MDC_MDIO_READ_CMD           0x1B
#define MDC_MDIO_WRITE_CMD          0x19

#define RTL837x_C2SIDXMAX (127)
#define RTL837x_FIDMAX    (15)

typedef enum
{
    SERDES_10GQXG,
    SERDES_10GUSXG = 0xD,
    SERDES_10GR = 0x1A,
    SERDES_HSG = 0x12,
    SERDES_2500BASEX = 0x16,
    SERDES_SG = 2,
    SERDES_1000BASEX = 4,
    SERDES_100FX = 5,  
    SERDES_OFF = 0x1F,
    SERDES_8221B = 0x21,
    SERDES_ON = 0x22,
    SERDES_END
} rtk_sds_mode_t;

enum _rtk_tb_op
{
    TB_OP_READ = 0,
    TB_OP_WRITE
};

enum _rtk_tb_access_execute
{
    TB_NOT_EXECUTE = 0,
    TB_EXECUTE,
};

enum _rtk_tb_access_target
{
    TB_TARGET_ACLRULE = 1,
    TB_TARGET_ACLACT,
    TB_TARGET_CVLAN,
    TB_TARGET_L2,
    TB_TARGET_IGMP_GROUP,
    TB_TARGET_HSA,
    TB_TARGET_HSB
};

typedef enum switch_chip_e
{
    CHIP_RTL8373 = 0,
    CHIP_RTL8224 = 1,
	CHIP_RTL8372,
	CHIP_RTL8373N,
	CHIP_RTL8221B,
	CHIP_RTL8366U,
	CHIP_RTL8372N,
	CHIP_RTL8224N,
    CHIP_END
}switch_chip_t;

struct rtl837x_mib_counter {
	unsigned int	offset;
	unsigned int	length;
	const char	*name;
};

struct rtl837x_sdsmode_map {
	rtk_sds_mode_t mode;
	const char *name;
};

struct rtl837x_priv {
 	struct device *dev;
	struct gpio_desc	*reset;
 	struct mii_bus *bus;
	struct regmap		*map;
	struct mutex		map_lock;
	struct regmap		*map_8224;
	struct mutex		map_8224_lock;
	int			mdio_addr;
	enum dsa_tag_protocol tag_proto;

	struct dentry *debugfs_parent;

	const char *chip_name;
	switch_chip_t chip_id;
	u32 chip_ver;
	u32 chip_ver_8224;

	unsigned int num_ports;

	struct dsa_switch	*ds;

    const struct rtl837x_mib_counter *mib_counters;
	unsigned int num_mib_counters;
	struct mutex mib_lock;

	const struct rtl837x_ops *ops;
	int			(*write_reg_noack)(void *ctx, u32 addr, u32 data);

	void			*chip_data; /* Per-chip extra variant data */
};

struct rtl837x_vlan_4k {
	u16	vid;
	u16	untag;
	u16	member;
	u8	fid;
};

struct rtl837x_vlan_data {
	u16 vid;
	union {
		struct {
			u32 mbr  : 10;
			u32 untag: 10;
			u32 fid  : 4 ;
			u32 svlan_chk_ivl_svl: 1;
			u32 ivl_en: 1;
			u32 resv : 6;
		};
		u32 val;
	};
};

enum rtl837x_l2_method {
	LUT_READ_METHOD_MAC = 0,
	LUT_READ_METHOD_ADDRESS,
	LUT_READ_METHOD_NEXT_ADDRESS,
	LUT_READ_METHOD_NEXT_L2UC,
	LUT_READ_METHOD_NEXT_L2MC,
	LUT_READ_METHOD_NEXT_L3MC,
	LUT_READ_METHOD_NEXT_L2L3MC,
	LUT_READ_METHOD_NEXT_L2UCSPA,
};

enum rtl837x_lut_type {
	LUT_TYPE_L2_UC = 1,
	LUT_TYPE_L2_MC,
	LUT_TYPE_L3,
};

struct rtl837x_l2_key {
	u8 mac_addr[ETH_ALEN];
	u16 vid_fid;
	bool ivl;
};

struct rtl837x_l2_uc {
	struct rtl837x_l2_key key;
	u8 port;
	u8 age;

	bool auth; // 802.1X: not used in this driver
	bool is_static;
};

struct rtl837x_l2_mc {
	struct rtl837x_l2_key key;
	u16 mbr;

	bool igmp_asic;
	u8 igmp_idx;
};

struct rtl837x_l3
{
	u32 sip;
	u32 dip;

	bool l3lookup;
	u16 mbr;
	bool igmp_asic;
	u8 igmp_idx;
};

struct rtl837x_lut_entry {
	enum rtl837x_lut_type type;
	u16 addr;
	union
	{
		struct rtl837x_l2_uc uc;
		struct rtl837x_l2_mc mc;
		struct rtl837x_l3 l3;
	};
};

struct rtl837x_variant {
	const struct dsa_switch_ops *ds_ops_mdio;
	const struct rtl837x_ops *ops;
	const enum dsa_tag_protocol def_tag_proto;
	const struct phylink_mac_ops *pl_mac_ops;
	const bool have_8224;
	size_t chip_data_sz;
};

struct rtl837x_ops {
	int	(*detect)(struct rtl837x_priv *priv);
	int	(*reset_chip)(struct rtl837x_priv *priv);

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

char* chipid_to_chip_name(switch_chip_t id);

#define rtl837x_reg_read(priv, reg, pval) regmap_read(priv->map, reg, pval)
#define rtl837x_reg_write(priv, reg, val) regmap_write(priv->map, reg, val)

extern int rtl837x_reg_bits_read(struct rtl837x_priv *priv, u32 reg, u32 mask, u32 *pval);
extern int rtl837x_reg_bits_write(struct rtl837x_priv *priv, u32 reg, u32 mask, u32 val);

extern int rtl837x_gpiochip_init(struct rtl837x_priv *priv);
extern rtk_sds_mode_t phy_interface_to_rtk_sds_mode(phy_interface_t interface);

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

#define rtl837x_rtl8224_reg_read(priv, reg, pval) (priv->map_8224 ? regmap_read(priv->map_8224, reg, pval) : -ENODEV)
#define rtl837x_rtl8224_reg_write(priv, reg, val) (priv->map_8224 ? regmap_write(priv->map_8224, reg, val) : -ENODEV)
extern int rtl837x_rtl8224_reg_bits_read(struct rtl837x_priv *priv, u32 reg, u32 mask, u32 *pval);
extern int rtl837x_rtl8224_reg_bits_write(struct rtl837x_priv *priv, u32 reg, u32 mask, u32 val);
extern int rtl837x_rtl8224_sds_reg_read(struct rtl837x_priv *priv, u8 sds_idx, u16 sds_page, u16 sds_reg, u16 *pdata);
extern int rtl837x_rtl8224_sds_reg_write(struct rtl837x_priv *priv, u8 sds_idx, u16 sds_page, u16 sds_reg, u16 data);
extern int rtl837x_rtl8224_sds_reg_bits_read(struct rtl837x_priv *priv, u8 sds_idx, u16 sds_page, u16 sds_reg, u16 mask, u16 *pdata);
extern int rtl837x_rtl8224_sds_reg_bits_write(struct rtl837x_priv *priv, u8 sds_idx, u16 sds_page, u16 sds_reg, u16 mask, u16 data);

extern int rtl837x_vlan_set(struct rtl837x_priv *priv, struct rtl837x_vlan_data *vlan);
extern int rtl837x_vlan_get(struct rtl837x_priv *priv, struct rtl837x_vlan_data *vlan);

extern int rtl837x_lut_query(struct rtl837x_priv *priv, 
                          enum rtl837x_l2_method method,
                          struct rtl837x_lut_entry *entry);
extern int rtl837x_lut_set(struct rtl837x_priv *priv, 
                          struct rtl837x_lut_entry *entry);
extern int rtl837x_lut_del(struct rtl837x_priv *priv, 
                        	  u32 addr);

extern int rtl837x_sds_reset_X(struct rtl837x_priv *priv, u8 sds_idx);
extern int rtl837x_sds_reset_R(struct rtl837x_priv *priv, u8 sds_idx);
extern int rtl837x_rtl8224_sds_reset_R(struct rtl837x_priv *priv, u8 sds_idx);

extern int rtl837x_serdes_set_mode(struct rtl837x_priv *priv, u8 sds_idx, rtk_sds_mode_t mode);

#if defined(RTL837X_PHY_PATCH)
extern int patch_phys_v008(struct rtl837x_priv *priv, u16 phy_mask);
extern int patch_phys_v008_rls_lockmain(struct rtl837x_priv *priv, u16 phy_mask);
extern int patch_phys_v009(struct rtl837x_priv *priv, u16 phy_mask);
extern int patch_phys_v009_rls_lockmain(struct rtl837x_priv *priv, u16 phy_mask);
#endif

extern const struct rtl837x_variant rtl8372n_variant;

#endif