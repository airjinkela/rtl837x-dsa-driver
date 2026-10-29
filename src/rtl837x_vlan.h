/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2025 StarField Xu <air_jinkela@163.com>
 */

#ifndef _RTL837X_VLAN_H
#define _RTL837X_VLAN_H

#include "rtl837x.h"

struct rtl837x_vlan_data {
	union {
		struct {
			u32 mbr  : 10;
			u32 untag: 10;
			u32 fid  : 4;
			u32 svlan_chk_ivl_svl: 1;
			u32 ivl_en: 1;
			u32 resv : 6;
		};
		u32 val;
	};
};

/**
 * enum rtl837x_frame_ingress - port accept ingress frame
 * @RTL837X_FRAME_TYPE_ANY_FRAME:  accept tagged and untagged frame
 * @RTL837X_FRAME_TYPE_TAGGED_ONLY: only accept tagged frame, untagged frame
 * will be dorp
 * @RTL837X_FRAME_TYPE_UNTAGGED_ONLY: only accept untagged frame, tagged frame
 * will be dorp
 * It is the same as the RTL8365MB
 */
enum rtl837x_frame_ingress {
	RTL837X_FRAME_TYPE_ANY_FRAME = 0,
	RTL837X_FRAME_TYPE_TAGGED_ONLY,
	RTL837X_FRAME_TYPE_UNTAGGED_ONLY,
};

/**
 * enum rtl837x_vlan_egress_mode - port VLAN egress mode
 * @RTL837X_VLAN_EGRESS_MODE_ORIGINAL: follow untag mask in VLAN4k table entry
 * @RTL837X_VLAN_EGRESS_MODE_KEEP: the VLAN tag format of egressed packets
 * will remain the same as their ingressed format, but the priority and VID
 * fields may be altered
 * @RTL837X_VLAN_EGRESS_MODE_PRI_TAG: always egress with priority tag
 * @RTL837X_VLAN_EGRESS_MODE_REAL_KEEP: the VLAN tag format of egressed
 * packets will remain the same as their ingressed format, and neither the
 * priority nor VID fields can be altered
 * It is the same as the RTL8365MB
 */
enum rtl837x_vlan_egress_mode {
	RTL837X_VLAN_EGRESS_MODE_ORIGINAL = 0,
	RTL837X_VLAN_EGRESS_MODE_KEEP = 1,
	RTL837X_VLAN_EGRESS_MODE_PRI_TAG = 2,
	RTL837X_VLAN_EGRESS_MODE_REAL_KEEP = 3,
};

int rtl837x_vlan_set(struct rtl837x_priv *priv, u16 vid, struct rtl837x_vlan_data *vlan);
int rtl837x_vlan_get(struct rtl837x_priv *priv, u16 vid, struct rtl837x_vlan_data *vlan);
int rtl837x_vlan_port_set_pvid(struct rtl837x_priv *priv, int port, u16 pvid);
int rtl837x_vlan_port_get_pvid(struct rtl837x_priv *priv, int port, u16 *pvid);

int rtl837x_vlan_port_set_framefilter(struct rtl837x_priv *priv, int port,
		    enum rtl837x_frame_ingress frame_type);
int rtl837x_vlan_port_get_framefilter(struct rtl837x_priv *priv, int port,
		    enum rtl837x_frame_ingress *frame_type);

int rtl837x_vlan_port_set_vlan_egress_mode(struct rtl837x_priv *priv, int port,
		    enum rtl837x_vlan_egress_mode mode);
int rtl837x_vlan_port_get_vlan_egress_mode(struct rtl837x_priv *priv, int port,
		    enum rtl837x_vlan_egress_mode *mode);

int rtl837x_vlan_port_set_transparent(struct rtl837x_priv *priv,
		    int igr_port, int egr_port, bool enable);
int rtl837x_vlan_port_get_transparent_mbr(struct rtl837x_priv *priv,
		    int egr_port, u32 *igr_mbr);
int rtl837x_vlan_port_set_egress_vlan_keep(struct rtl837x_priv *priv,
			int igr_port, int egr_port, bool enable);
int rtl837x_vlan_port_get_egress_vlan_keep_mbr(struct rtl837x_priv *priv,
			int egr_port, u32 *igr_mbr);

int rtl837x_vlan_port_set_ingress_filtering(struct rtl837x_priv *priv, int port, bool enable);
int rtl837x_vlan_port_get_ingress_filtering(struct rtl837x_priv *priv, int port, bool *enable);


#endif /* _RTL837X_VLAN_H */
