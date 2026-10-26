/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Forwarding and multicast database interface for the rtl837x switch family
 *
 * Copyright (C) 2025 StarField Xu <air_jinkela@163.com>
 */

#ifndef _RTL837X_L2_H
#define _RTL837X_L2_H

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

	bool auth;
	bool is_static;
};

struct rtl837x_l2_mc {
	struct rtl837x_l2_key key;
	u16 mbr;

	bool igmp_asic;
	u8 igmp_idx;
};

struct rtl837x_l3 {
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
	union {
		struct rtl837x_l2_uc uc;
		struct rtl837x_l2_mc mc;
		struct rtl837x_l3 l3;
	};
};

int rtl837x_lut_query(struct rtl837x_priv *priv,
			  enum rtl837x_l2_method method,
			  struct rtl837x_lut_entry *entry);
int rtl837x_lut_set(struct rtl837x_priv *priv,
			  struct rtl837x_lut_entry *entry);
int rtl837x_lut_del(struct rtl837x_priv *priv,
				  u32 addr);

int rtl837x_lut_port_set_learning(struct rtl837x_priv *priv, int port,
				       bool enable);

#endif /* _RTL837X_L2_H */
