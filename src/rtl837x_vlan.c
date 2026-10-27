// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2025 StarField Xu <air_jinkela@163.com>
 */

#include "rtl837x.h"
#include "rtl837x_vlan.h"
#include "rtl837x_table.h"

/* vlan ingress filter */
#define RTL837X_VLAN_INGRESS_FILTER_REG(_p)            (0x4E18 + (((_p / 10) << 2)))
#define   RTL837X_VLAN_INGRESS_FILTER_MASK(_p)          BIT(_p % 0xA)

/* vlan ingress accept frame type. See &rtl837x_frame_ingress */
#define RTL837X_VLAN_ACCEPT_FRAME_TYPE_REG(_p)          (0x4E10 + (((_p / 10) << 2)))
#define   RTL837X_VLAN_ACCEPT_FRAME_TYPE_MASK(_p)       (GENMASK(1, 0) << ((_p % 0xA) << 1))

/* vlan transparent */
#define RTL837X_VLAN_EGRESS_TRANSPARENT_REG(_p)         (0x4EB8 + (((_p / 3) << 2)))
#define   RTL837X_VLAN_EGRESS_TRANSPARENT_MASK(_p)      (GENMASK(9, 0) << ((_p % 0x3) * 10))

/* vlan egress mode. see &rtl837x_vlan_egress_mode */
#define RTL837X_VLAN_EGRESS_TAG_MODE_REG(_p)            (0x6738 + (((_p / 10) << 2)))
#define   RTL837X_VLAN_EGRESS_TAG_MODE_MASK(_p)         (GENMASK(1, 0) << ((_p % 0xA) << 1))

/* vlan egress vlan keep */
#define RTL837X_VLAN_EGRESS_VLAN_KEEP_REG(_p)           (0x6728 + (((_p / 3) << 2)))
#define   RTL837X_VLAN_EGRESS_VLAN_KEEP_PMASK_MASK(_p)  (GENMASK(9, 0) << ((_p % 0x3) * 10))

/* vlan port based vid */
#define RTL837X_VLAN_PVID_REG(_p)                       (0x4E1C + (((_p >> 1) << 2)))
#define   RTL837X_VLAN_PVID_MASK(_p)                    (GENMASK(11, 0) << ((_p & 0x1) * 12))

int rtl837x_vlan_set(struct rtl837x_priv *priv, u16 vid, struct rtl837x_vlan_data *vlan)
{
	return rtl837x_ita_execute(priv, RTL837X_TABLE_CVLAN, RTL837X_TABLE_OPERA_WRITE,
			    &vid, -0, -0, &vlan->val, 1);
}

int rtl837x_vlan_get(struct rtl837x_priv *priv, u16 vid, struct rtl837x_vlan_data *vlan)
{
	int ret;
	u32 tmp;

	ret = rtl837x_ita_execute(priv, RTL837X_TABLE_CVLAN, RTL837X_TABLE_OPERA_READ,
		    &vid, -0, -0, &tmp, 1);
	if (ret)
		return ret;
	vlan->val = tmp;

	return 0;
}

int rtl837x_vlan_port_set_pvid(struct rtl837x_priv *priv, int port, u16 pvid)
{
	return rtl837x_reg_bits_write(priv, RTL837X_VLAN_PVID_REG(port),
			  RTL837X_VLAN_PVID_MASK(port),
			  pvid
			);
}

int rtl837x_vlan_port_get_pvid(struct rtl837x_priv *priv, int port, u16 *pvid)
{
	int ret;
	u32 tmp;

	ret = rtl837x_reg_bits_read(priv, RTL837X_VLAN_PVID_REG(port),
			  RTL837X_VLAN_PVID_MASK(port),
			  &tmp
			);
	if (ret)
		return ret;
	*pvid = tmp;
	return 0;
}

int rtl837x_vlan_port_set_framefilter(struct rtl837x_priv *priv, int port, enum rtl837x_frame_ingress frame_type)
{
	return rtl837x_reg_bits_write(priv, RTL837X_VLAN_ACCEPT_FRAME_TYPE_REG(port),
			  RTL837X_VLAN_ACCEPT_FRAME_TYPE_MASK(port),
			  frame_type
			);
}

int rtl837x_vlan_port_get_framefilter(struct rtl837x_priv *priv, int port, enum rtl837x_frame_ingress *frame_type)
{
	return rtl837x_reg_bits_read(priv, RTL837X_VLAN_ACCEPT_FRAME_TYPE_REG(port),
			  RTL837X_VLAN_ACCEPT_FRAME_TYPE_MASK(port),
			  frame_type
			);
}

int rtl837x_vlan_port_set_vlan_egress_mode(struct rtl837x_priv *priv, int port,
			 enum rtl837x_vlan_egress_mode mode)
{
	return rtl837x_reg_bits_write(priv, RTL837X_VLAN_EGRESS_TAG_MODE_REG(port),
		 RTL837X_VLAN_EGRESS_TAG_MODE_MASK(port), mode
		);
}

int rtl837x_vlan_port_get_vlan_egress_mode(struct rtl837x_priv *priv, int port,
			 enum rtl837x_vlan_egress_mode *mode)
{
	int ret;
	u32 tmp;

	ret = rtl837x_reg_bits_read(priv, RTL837X_VLAN_EGRESS_TAG_MODE_REG(port),
		 RTL837X_VLAN_EGRESS_TAG_MODE_MASK(port), &tmp
		);
	if (ret)
		return ret;

	*mode = (enum rtl837x_vlan_egress_mode)tmp;

	return 0;
}

int rtl837x_vlan_port_set_transparent(struct rtl837x_priv *priv,
			int igr_port, int egr_port, bool enable)
{
	int ret;
	u32 tmp;

	dev_dbg(priv->dev, "%s transparent VLAN from %d to %d\n",
		enable ? "Enable" : "Disable", igr_port, egr_port);

	/* "Transparent" between the two ports means that frame forwarded by
	 * igr_port and egressed on egr_port will not be filtered by the usual
	 * VLAN membership settings.
	 * It is the same as the RTL8365MB
	 */
	ret = rtl837x_reg_bits_read(priv, RTL837X_VLAN_EGRESS_TRANSPARENT_REG(egr_port),
				  RTL837X_VLAN_EGRESS_TRANSPARENT_MASK(egr_port), &tmp);
	if (ret)
		return ret;

	if (enable)
		tmp |= BIT(igr_port);
	else
		tmp &= ~BIT(igr_port);
	return rtl837x_reg_bits_write(priv, RTL837X_VLAN_EGRESS_TRANSPARENT_REG(egr_port),
				  RTL837X_VLAN_EGRESS_TRANSPARENT_MASK(egr_port), tmp);
}

int rtl837x_vlan_port_get_transparent_mbr(struct rtl837x_priv *priv,
		    int egr_port, u32 *igr_mbr)
{
	int ret;
	u32 tmp;

	ret = rtl837x_reg_bits_read(priv, RTL837X_VLAN_EGRESS_TRANSPARENT_REG(egr_port),
				  RTL837X_VLAN_EGRESS_TRANSPARENT_MASK(egr_port), &tmp);
	*igr_mbr = tmp;
	return ret;
}

int rtl837x_vlan_port_set_egress_vlan_keep(struct rtl837x_priv *priv,
			int igr_port, int egr_port, bool enable)
{
	int ret;
	u32 tmp;

	dev_dbg(priv->dev, "%s VLAN Keep from %d to %d\n",
		enable ? "Enable" : "Disable", igr_port, egr_port);

	/* "VLAN keep" between the two ports means that frames forwarded by
	 * igr_port and egressed on egr_port keep their original VLAN tag
	 * instead of being stripped by the untagged setting of the VLAN
	 * entry, but they are still subject to the VLAN membership rules.
	 */
	ret = rtl837x_reg_bits_read(priv, RTL837X_VLAN_EGRESS_VLAN_KEEP_REG(egr_port),
				  RTL837X_VLAN_EGRESS_VLAN_KEEP_PMASK_MASK(egr_port), &tmp);
	if (ret)
		return ret;

	if (enable)
		tmp |= BIT(igr_port);
	else
		tmp &= ~BIT(igr_port);
	return rtl837x_reg_bits_write(priv, RTL837X_VLAN_EGRESS_VLAN_KEEP_REG(egr_port),
				  RTL837X_VLAN_EGRESS_VLAN_KEEP_PMASK_MASK(egr_port), tmp);
}

int rtl837x_vlan_port_get_egress_vlan_keep_mbr(struct rtl837x_priv *priv,
			int egr_port, u32 *igr_mbr)
{
	int ret;
	u32 tmp;

	ret = rtl837x_reg_bits_read(priv, RTL837X_VLAN_EGRESS_VLAN_KEEP_REG(egr_port),
				  RTL837X_VLAN_EGRESS_VLAN_KEEP_PMASK_MASK(egr_port), &tmp);
	*igr_mbr = tmp;
	return ret;
}

int rtl837x_vlan_port_set_ingress_filtering(struct rtl837x_priv *priv, int port, bool enable)
{
	return rtl837x_reg_bits_write(priv, RTL837X_VLAN_INGRESS_FILTER_REG(port),
			RTL837X_VLAN_INGRESS_FILTER_MASK(port),
			enable ? 1 : 0
		);
}

int rtl837x_vlan_port_get_ingress_filtering(struct rtl837x_priv *priv, int port, bool *enable)
{
	int ret;
	u32 tmp;

	ret =  rtl837x_reg_bits_read(priv, RTL837X_VLAN_INGRESS_FILTER_REG(port),
			RTL837X_VLAN_INGRESS_FILTER_MASK(port),
			&tmp
		);
	if (ret)
		return ret;

	*enable = !!tmp;
	return 0;
}
