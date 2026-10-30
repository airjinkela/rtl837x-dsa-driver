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

#define rtl837x_reg_read(priv, reg, pval) regmap_read(priv->map, reg, pval)
#define rtl837x_reg_write(priv, reg, val) regmap_write(priv->map, reg, val)

extern int rtl837x_reg_bits_read(struct rtl837x_priv *priv, u32 reg, u32 mask, u32 *pval);
extern int rtl837x_reg_bits_write(struct rtl837x_priv *priv, u32 reg, u32 mask, u32 val);

extern int rtl837x_set_led(struct rtl837x_priv *priv);

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
