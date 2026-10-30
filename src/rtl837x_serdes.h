/* SPDX-License-Identifier: GPL-2.0-or-later */
/*
 * Copyright (C) 2025 StarField Xu <air_jinkela@163.com>
 */

#ifndef _RTL837X_SERDES_H
#define _RTL837X_SERDES_H

#include "rtl837x.h"

int rtl837x_serdes_pnswap_rx(struct realtek_priv *priv, u8 sds_idx, bool enable);
int rtl837x_serdes_pnswap_tx(struct realtek_priv *priv, u8 sds_idx, bool enable);

int rtl837x_serdes_reset_XSG(struct realtek_priv *priv, u8 sds_idx);
int rtl837x_serdes_reset_10G(struct realtek_priv *priv, u8 sds_idx);
int rtl837x_serdes_on(struct realtek_priv *priv, u8 sds_idx);
int rtl837x_serdes_off(struct realtek_priv *priv, u8 sds_idx);
int rtl837x_serdes_an_patch(struct realtek_priv *priv, u8 sds_idx, phy_interface_t interface);
int rtl837x_serdes_mac_patch(struct realtek_priv *priv, u8 sds_idx);

int rtl837x_serdes_pause_frame_config(struct realtek_priv *priv, u8 sds_idx,
			     phy_interface_t interface,
				 bool pause, bool asympause);
int rtl837x_serdes_an_config(struct realtek_priv *priv, u8 sds_idx,
			     phy_interface_t interface,
				 unsigned int neg_mode);

#endif /* _RTL837X_SERDES_H */
