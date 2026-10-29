/* SPDX-License-Identifier: GPL-2.0-or-later */
/* Forwarding and multicast database interface for the rtl837x switch family
 *
 * Copyright (C) 2025 StarField Xu <air_jinkela@163.com>
 */

#ifndef _RTL837X_L2_H
#define _RTL837X_L2_H

#include "rtl837x.h"

enum rtl837x_lut_method {
	RTL837X_LUT_METHOD_MAC = 0,
	RTL837X_LUT_METHOD_ADDRESS,
	RTL837X_LUT_METHOD_NEXT_ADDRESS,
	RTL837X_LUT_METHOD_NEXT_L2UC,
	RTL837X_LUT_METHOD_NEXT_L2MC,
	RTL837X_LUT_METHOD_NEXT_L3MC,
	RTL837X_LUT_METHOD_NEXT_L2L3MC,
	RTL837X_LUT_METHOD_NEXT_L2UCSPA,
};

enum rtl837x_lut_type {
	RTL837X_LUT_TYPE_L2_UC = 1,
	RTL837X_LUT_TYPE_L2_MC,
	RTL837X_LUT_TYPE_L3,
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

	bool is_static;
	bool auth;
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

	bool igmp_asic;
	bool l3lookup;
	u8 igmp_idx;
	u16 mbr;
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

enum rtl837x_l2_flush_mode {
	RTL837X_l2_FLUSH_MODE_PORT = 0,
	RTL837X_l2_FLUSH_MODE_VID,
	RTL837X_l2_FLUSH_MODE_FID,
};

int rtl837x_lut_query(struct rtl837x_priv *priv,
			  enum rtl837x_lut_method method,
			  struct rtl837x_lut_entry *entry);
int rtl837x_lut_set(struct rtl837x_priv *priv,
			  struct rtl837x_lut_entry *entry);
int rtl837x_lut_del(struct rtl837x_priv *priv,
				  u16 addr);

int rtl837x_lut_port_set_learning(struct rtl837x_priv *priv, int port,
				       bool enable);

int rtl837x_lut_l2_flush(struct rtl837x_priv *priv, enum rtl837x_l2_flush_mode mode,
		    int port, u16 vid, u16 fid);
int rtl837x_lut_l2_flush_all(struct rtl837x_priv *priv);

#endif /* _RTL837X_L2_H */
