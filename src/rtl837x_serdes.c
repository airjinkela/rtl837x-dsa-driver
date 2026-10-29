// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2025 StarField Xu <air_jinkela@163.com>
 */
#include <linux/version.h>

#include "rtl837x.h"
#include "rtl837x_serdes.h"


/*
 * As Realtek has not released the register manual
 * for the internal SerDes, the register definitions
 * below have been inferred from comments and names
 * found in OEM code and code for similar IP chips.
 */
/* PAGE_FRC */
#define SDS_PAGE_FRC            0x20
#define SDS_REG_FRC             0x00
#define   SDS_FRC_RX_EN_ON_MASK    BIT(4)
#define   SDS_FRC_RX_EN_VAL_MASK   BIT(5)
#define   SDS_FRC_PDOWN_ON_MASK    BIT(6)
#define   SDS_FRC_PDOWN_VAL_MASK   BIT(7)
#define   SDS_FRC_CMU_EN_ON_MASK   BIT(10)
#define   SDS_FRC_CMU_EN_VAL_MASK  BIT(11)

/* PAGE_NWAY_AN */
#define SDS_PAGE_NWAY_AN   0x07
#define SDS_REG_NWAY_AN    17
#define   SDS_NWAY_QHSG_AN_CH0_EN_MASK BIT(0)
#define   SDS_NWAY_QHSG_AN_CH1_EN_MASK BIT(1)
#define   SDS_NWAY_QHSG_AN_CH2_EN_MASK BIT(2)
#define   SDS_NWAY_QHSG_AN_CH3_EN_MASK BIT(3)

/* PAGE_CTRL00 */
#define SDS_PAGE_CTRL00    0x00
#define SDS_REG_CTRL00_REG00     0x00
#define   SDS_CTRL00_REG00_XSG_TX_INV_MASK  BIT(8)
#define   SDS_CTRL00_REG00_XSG_RX_INV_MASK  BIT(9)

/*	I Guess
 *                         Force_En  Force_Dis Auto
 * BIT8   SP_SDS_FRC_AN       1        1        0
 * BIT9   SP_SDS_FRC_AN_EN    1        0        x
 *                            3        1        0
 * 0: NWAY_AUTO
 * 1: NWAY_FORCE_DIS
 * 3: NWAY_FORCE_EN (maybe?)
 */
#define SDS_REG_CTRL00_REG02     0x02
#define   SDS_CTRL00_REG02_XSG_SP_SDS_FRC_AN    BIT(8)
#define   SDS_CTRL00_REG02_XSG_SP_SDS_FRC_AN_EN BIT(9)

#define SDS_REG_CTRL00_REG04     0x04
#define   SDS_CTRL00_REG04_SP_CFG_EN_LINK_FIB1G_MASK   BIT(2)

/* PAGE_CTRL01 */
#define SDS_PAGE_CTRL01             0x01
#define SDS_REG_CTRL01_XSG_STS      0x1d     // I Guess
#define   SDS_CTRL01_XSG_STS_SYNC_OK   BIT(0) // I Guess
#define   SDS_CTRL01_XSG_STS_LINK_OK   BIT(4) // I Guess
#define   SDS_CTRL01_XSG_STS_SIG_OK    BIT(8) // I Guess

/* PAGE_CTRL02 */
#define SDS_PAGE_CTRL02       0x02
// It seems like 'Advertisement control register'
#define SDS_REG_CTRL02_XSG_AN   0x04
#define   SDS_CTRL02_XSG_AN_10_100_AsymmetricPause_MASK BIT(11)
#define   SDS_CTRL02_XSG_AN_10_100_Pause_MASK           BIT(10)
#define   SDS_CTRL02_XSG_AN_1G_AsymmetricPause_MASK BIT(8)
#define   SDS_CTRL02_XSG_AN_1G_Pause_MASK           BIT(7)
#define   SDS_CTRL02_XSG_AN_1G_HalfDuplex_MASK      BIT(6)
#define   SDS_CTRL02_XSG_AN_1G_FullDuplex_MASK      BIT(5)

/* PAGE_CTRL05 */
#define SDS_PAGE_CTRL05             0x05
#define SDS_REG_CTRL05_10GR_STS     0x00       // I Guess
#define   SDS_CTRL05_10GR_STS_SYNC_OK   BIT(0)  // I Guess
#define   SDS_CTRL05_10GR_STS_HI_BER    BIT(1)  // I Guess
#define   SDS_CTRL05_10GR_STS_LINK_OK   BIT(12) // I Guess

/* PAGE_CTRL06 */
#define SDS_PAGE_CTRL06       0x06
#define SDS_REG_CTRL06_REG02   0x02
#define   SDS_CTRL06_REG02_FSM_RESET_MASK    BIT(12)
#define   SDS_CTRL06_REG02_10GR_RX_INV_MASK  BIT(13)
#define   SDS_CTRL06_REG02_10GR_TX_INV_MASK  BIT(14)

/* PAGE_CTRL1F */
#define SDS_PAGE_CTRL1F       0x1f
#define SDS_REG_CTRL1F_10GR_AN  0x0B
#define   SDS_CTRL1F_10GR_AN_Pause_MASK     BIT(2)
#define   SDS_CTRL1F_10GR_AN_AsymmetricPause_MASK  BIT(3)

static const u16 patch_an_10p3125g_a[][3] = {
	{0x0021, 0x0010, 0x4480}, {0x0021, 0x0013, 0x0400}, {0x0021, 0x0018, 0x6D02},
	{0x0021, 0x001B, 0x424E}, {0x0021, 0x001D, 0x0002}, {0x0036, 0x001C, 0x1390},
	{0x002E, 0x0004, 0x0080}, {0x002E, 0x0006, 0x0408}, {0x002E, 0x0007, 0x020D},
	{0x002E, 0x0009, 0x0601}, {0x002E, 0x000B, 0x222C}, {0x002E, 0x000C, 0x9217},
	{0x002E, 0x0016, 0x0743}, {0x002E, 0x001D, 0xABB0}
};

static const u16 patch_an_10p3125g_b[][3] = {
	{0x0021, 0x0010, 0x4480}, {0x0021, 0x0013, 0x0400}, {0x0021, 0x0018, 0x6D02},
	{0x0021, 0x001B, 0x424E}, {0x0021, 0x001D, 0x0002}, {0x0036, 0x001C, 0x1390},
	{0x0036, 0x0014, 0x003F}, {0x0036, 0x0010, 0x0200}, {0x002E, 0x0004, 0x0080},
	{0x002E, 0x0006, 0x0408}, {0x002E, 0x0007, 0x020D}, {0x002E, 0x0009, 0x0601},
	{0x002E, 0x000B, 0x222C}, {0x002E, 0x000C, 0xA217}, {0x002E, 0x000D, 0xFE40},
	{0x002E, 0x0015, 0xF5C1}, {0x002E, 0x0016, 0x0443}, {0x002E, 0x001D, 0xABB0}
};

static const u16 patch_an_3p125g_a[][3] = {
	{0x0021, 0x0010, 0x6480}, {0x0021, 0x0013, 0x0400}, {0x0021, 0x0018, 0x6D02},
	{0x0021, 0x001B, 0x424E}, {0x0021, 0x001D, 0x0002}, {0x0036, 0x001C, 0x1390},
	{0x0028, 0x0004, 0x0080}, {0x0028, 0x0009, 0x0601}, {0x0028, 0x000B, 0x232C},
	{0x0028, 0x000C, 0x9217}, {0x0028, 0x000F, 0x5B50}, {0x0028, 0x0016, 0x0443},
	{0x0028, 0x001D, 0xABB0}
};

static const u16 patch_an_3p125g_b[][3] = {
	{0x0021, 0x0010, 0x6480}, {0x0021, 0x0013, 0x0400}, {0x0021, 0x0018, 0x6D02},
	{0x0021, 0x001B, 0x424E}, {0x0021, 0x001D, 0x0002}, {0x0036, 0x001C, 0x1390},
	{0x0036, 0x0014, 0x003F}, {0x0036, 0x0010, 0x0200}, {0x0028, 0x0004, 0x0080},
	{0x0028, 0x0007, 0x1201}, {0x0028, 0x0009, 0x0601}, {0x0028, 0x000B, 0x232C},
	{0x0028, 0x000C, 0x9217}, {0x0028, 0x000F, 0x5B50}, {0x0028, 0x0015, 0xE7F1},
	{0x0028, 0x0016, 0x0443}, {0x0028, 0x001D, 0xABB0}
};

static const u16 patch_an_1p25g_a[][3] = {
	{0x0021, 0x0010, 0x6480}, {0x0021, 0x0013, 0x0400}, {0x0021, 0x0018, 0x6D02},
	{0x0021, 0x001B, 0x424E}, {0x0021, 0x001D, 0x0002}, {0x0036, 0x001C, 0x1390},
	{0x0036, 0x0014, 0x003F}, {0x0036, 0x0010, 0x0300}, {0x0024, 0x0004, 0x0080},
	{0x0024, 0x0007, 0x1201}, {0x0024, 0x0009, 0x0601}, {0x0024, 0x000B, 0x232C},
	{0x0024, 0x000C, 0x9217}, {0x0024, 0x000F, 0x5B50}, {0x0024, 0x0015, 0xE7C1},
	{0x0024, 0x0016, 0x0443}, {0x0024, 0x001D, 0xABB0}
};

static const u16 patch_an_1p25g_b[][3] = {
	{0x0021, 0x0010, 0x6480}, {0x0021, 0x0013, 0x0400}, {0x0021, 0x0018, 0x6D02},
	{0x0021, 0x001B, 0x424E}, {0x0021, 0x001D, 0x0002}, {0x0036, 0x001C, 0x1390},
	{0x0036, 0x0014, 0x003F}, {0x0036, 0x0010, 0x0300}, {0x0024, 0x0004, 0x0080},
	{0x0024, 0x0007, 0x1201}, {0x0024, 0x0009, 0x0601}, {0x0024, 0x000B, 0x232C},
	{0x0024, 0x000C, 0x9217}, {0x0024, 0x000F, 0x5B50}, {0x0024, 0x0015, 0xE7C1},
	{0x0024, 0x0016, 0x0443}, {0x0024, 0x001D, 0xABB0}
};

static const u16 patch_an_125m_a[][3] = {
	{0x0021, 0x0010, 0x6480}, {0x0021, 0x0013, 0x0400}, {0x0021, 0x0018, 0x6D02},
	{0x0021, 0x001B, 0x424E}, {0x0021, 0x001D, 0x0002}, {0x0036, 0x001C, 0x1390},
	{0x0026, 0x0004, 0x0080}, {0x0026, 0x0009, 0x0601}, {0x0026, 0x000B, 0x232C},
	{0x0026, 0x000C, 0x9217}, {0x0026, 0x000F, 0x5B50}, {0x0026, 0x0016, 0x0443},
	{0x0026, 0x001D, 0xABB0}
};

static const u16 patch_an_125m_b[][3] = {
	{0x0021, 0x0010, 0x6480}, {0x0021, 0x0013, 0x0400}, {0x0021, 0x0018, 0x6D02},
	{0x0021, 0x001B, 0x424E}, {0x0021, 0x001D, 0x0002}, {0x0036, 0x001C, 0x1390},
	{0x0036, 0x0014, 0x003F}, {0x0036, 0x0010, 0x0300}, {0x0026, 0x0004, 0x0080},
	{0x0026, 0x0007, 0x1201}, {0x0026, 0x0009, 0x0601}, {0x0026, 0x000B, 0x232C},
	{0x0026, 0x000C, 0x9217}, {0x0026, 0x000F, 0x5B50}, {0x0026, 0x0015, 0xE7C1},
	{0x0026, 0x0016, 0x0443}, {0x0026, 0x001D, 0xABB0}
};

static const u16 patch_data_mac[][3] = {
	{0x0006, 0x0012, 0x5078}, {0x0007, 0x0006, 0x9401}, {0x0007, 0x0008, 0x9401},
	{0x0007, 0x000A, 0x9401}, {0x0007, 0x000C, 0x9401}, {0x001F, 0x000B, 0x0003},
	{0x0006, 0x0003, 0xC45C}, {0x0006, 0x001F, 0x2100}
};

static const u16 patch_data_phy[][3] = {
	{0x0006, 0x0012, 0x5078}, {0x0006, 0x0003, 0xc45c}, {0x0006, 0x001E, 0x000C},
	{0x0006, 0x001F, 0x2100}
};

int rtl837x_serdes_pnswap_rx(struct rtl837x_priv *priv, u8 sds_idx, bool enable)
{
	int ret;

	/* SerDes PN swap: page 0x00 reg 0x00 (XSG, ≤5G) + page 0x06 reg 0x02 (10GR) */
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, SDS_PAGE_CTRL00, SDS_REG_CTRL00_REG00,
				SDS_CTRL00_REG00_XSG_RX_INV_MASK, !!enable); //#SDS0 RX PN swap
	if (ret)
		return ret;
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, SDS_PAGE_CTRL06, SDS_REG_CTRL06_REG02,
				SDS_CTRL06_REG02_10GR_RX_INV_MASK, !!enable);
	if (ret)
		return ret;

	return 0;
}

int rtl837x_serdes_pnswap_tx(struct rtl837x_priv *priv, u8 sds_idx, bool enable)
{
	int ret;

	/* SerDes PN swap: page 0x00 reg 0x00 (XSG, ≤5G) + page 0x06 reg 0x02 (10GR) */
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, SDS_PAGE_CTRL00, SDS_REG_CTRL00_REG00,
				SDS_CTRL00_REG00_XSG_TX_INV_MASK, !!enable); //#SDS0 RX PN swap
	if (ret)
		return ret;
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, SDS_PAGE_CTRL06, SDS_REG_CTRL06_REG02,
				SDS_CTRL06_REG02_10GR_TX_INV_MASK, !!enable);
	if (ret)
		return ret;

	return 0;
}

int rtl837x_serdes_reset_10G(struct rtl837x_priv *priv, u8 sds_idx)
{
	bool rx_idle, nsq, sync_ok, link_ok, hi_ber; // nsq:(Noise Squelch) hi_ber:(High bit error rate)
	u16 rx_sts, tmp;
	int ret;

	ret = rtl837x_sds_reg_bits_read(priv, sds_idx, SDS_PAGE_FRC, SDS_REG_FRC,
			  SDS_FRC_RX_EN_VAL_MASK | SDS_FRC_RX_EN_ON_MASK,
			  &rx_sts
			);
	if (ret)
		return ret;

	// check serdes rx status
	// 0,2,3: rx is enabled
	// 1    : rx is disabled
	if (rx_sts == 1) // do nothing when rx is disabled
		return 0;

	// enable rx test
	// switch debug port (What is this?)
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x21, 0x00, BIT(2), 1); // RX_TEST_EN
	if (ret)
		return ret;
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x36, 0x05, 0xf<<11, 8); // REG0_DEBUG_SEL
	if (ret)
		return ret;
	ret = rtl837x_sds_reg_write(priv, sds_idx, 0x1f, 0x02, 0x1f); // switch debug port
	if (ret)
		return ret;

	// Get RX status
	ret = rtl837x_sds_reg_read(priv, sds_idx, 0x1f, 0x15, &tmp);
	if (ret)
		return ret;
	rx_idle = !!((tmp>>7)&1);
	nsq = !!((tmp>>6)&1);

	// Do reset when RX is not idle or RX Noise Squelch
	if (!(nsq == 1 || rx_idle == 0))
		return 0;

	// check sync_ok
	ret = rtl837x_sds_reg_read(priv, sds_idx, SDS_PAGE_CTRL05, SDS_REG_CTRL05_10GR_STS, &tmp);
	if (ret)
		return ret;

	sync_ok = FIELD_GET(SDS_CTRL05_10GR_STS_SYNC_OK, tmp);
	link_ok = FIELD_GET(SDS_CTRL05_10GR_STS_LINK_OK, tmp);
	hi_ber =  FIELD_GET(SDS_CTRL05_10GR_STS_HI_BER, tmp);

	if (sync_ok == 0)
		goto do_reset;
	else
		if ((link_ok == 0) || (hi_ber == 1))
			goto do_reset;
	return 0;

do_reset:
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, SDS_PAGE_FRC, SDS_REG_FRC,
		  SDS_FRC_RX_EN_VAL_MASK | SDS_FRC_RX_EN_ON_MASK,
		  0x3
		);
	if (ret)
		return ret;
	msleep(20);
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, SDS_PAGE_FRC, SDS_REG_FRC,
		  SDS_FRC_RX_EN_VAL_MASK | SDS_FRC_RX_EN_ON_MASK,
		  0x1);
	if (ret)
		return ret;
	msleep(20);
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, SDS_PAGE_FRC, SDS_REG_FRC,
		  SDS_FRC_RX_EN_VAL_MASK | SDS_FRC_RX_EN_ON_MASK,
		  0x3
		);
	if (ret)
		return ret;
	msleep(20);
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, SDS_PAGE_FRC, SDS_REG_FRC,
		  SDS_FRC_RX_EN_VAL_MASK | SDS_FRC_RX_EN_ON_MASK,
		  0x0);
	if (ret)
		return ret;
	msleep(20);

	return 0;
}

// 100M/1G/2.5G reset
int rtl837x_serdes_reset_XSG(struct rtl837x_priv *priv, u8 sds_idx)
{
	bool sig_ok, sync_ok, link_ok;
	u16 rx_sts, tmp;
	int ret;

	ret = rtl837x_sds_reg_bits_read(priv, sds_idx, SDS_PAGE_FRC, SDS_REG_FRC,
			  SDS_FRC_RX_EN_VAL_MASK | SDS_FRC_RX_EN_ON_MASK,
			  &rx_sts
			);
	if (ret)
		return ret;

	// check serdes rx status
	// 0,2,3: rx is enabled
	// 1    : rx is disabled
	if (rx_sts == 1) // do nothing when rx is disabled
		return 0;

	ret = rtl837x_sds_reg_read(priv, sds_idx, SDS_PAGE_CTRL01, SDS_REG_CTRL01_XSG_STS, &tmp);
	if (ret)
		return ret;

	sig_ok =  FIELD_GET(SDS_CTRL01_XSG_STS_SIG_OK, tmp);
	link_ok = FIELD_GET(SDS_CTRL01_XSG_STS_LINK_OK, tmp);
	sync_ok = FIELD_GET(SDS_CTRL01_XSG_STS_SYNC_OK, tmp);

	if (!sig_ok)
		return 0;

	if (sync_ok == 0)
		goto do_reset;
	else
		if (link_ok == 0)
			goto do_reset;
	return 0;

do_reset:
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x00, 0x00, BIT(1), 1);
	if (ret)
		return ret;
	msleep(20);
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x00, 0x00, BIT(1), 0);
	if (ret)
		return ret;
	msleep(20);
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x00, 0x00, BIT(1), 1);
	if (ret)
		return ret;
	msleep(20);
	return 0;
}

int rtl837x_serdes_an_patch(struct rtl837x_priv *priv, u8 sds_idx, phy_interface_t interface)
{
	const u16 (*an_patch)[3];
	int an_patch_len;
	u16 chip_ver = 0;
	int ret;

	ret = rtl837x_get_chip_ver(priv, &chip_ver);
	if (ret) {
		dev_err(priv->dev, "Falied to get chip ver\n");
		return ret;
	}

	switch (interface) {
#if KERNEL_VERSION(6, 12, 44) <= LINUX_VERSION_CODE
	case PHY_INTERFACE_MODE_10G_QXGMII:
#endif
	case PHY_INTERFACE_MODE_USXGMII:
	case PHY_INTERFACE_MODE_10GBASER:
		if (chip_ver == 0) {
			an_patch = patch_an_10p3125g_a;
			an_patch_len = sizeof(patch_an_10p3125g_a)/(sizeof(u16)*3);
		} else {
			an_patch = patch_an_10p3125g_b;
			an_patch_len = sizeof(patch_an_10p3125g_b)/(sizeof(u16)*3);
		}
		break;
	//HSGMII ?
	case PHY_INTERFACE_MODE_2500BASEX:
		if (chip_ver == 0) {
			an_patch = patch_an_3p125g_a;
			an_patch_len = sizeof(patch_an_3p125g_a)/(sizeof(u16)*3);
		} else {
			an_patch = patch_an_3p125g_b;
			an_patch_len = sizeof(patch_an_3p125g_b)/(sizeof(u16)*3);
		}
		break;
	case PHY_INTERFACE_MODE_SGMII:
	case PHY_INTERFACE_MODE_1000BASEX:
		if (chip_ver == 0) {
			an_patch = patch_an_1p25g_a;
			an_patch_len = sizeof(patch_an_1p25g_a)/(sizeof(u16)*3);
		} else {
			an_patch = patch_an_1p25g_b;
			an_patch_len = sizeof(patch_an_1p25g_b)/(sizeof(u16)*3);
		}
		break;
	case PHY_INTERFACE_MODE_100BASEX:
		if (chip_ver == 0) {
			an_patch = patch_an_125m_a;
			an_patch_len = sizeof(patch_an_125m_a)/(sizeof(u16)*3);
		} else {
			an_patch = patch_an_125m_b;
			an_patch_len = sizeof(patch_an_125m_b)/(sizeof(u16)*3);
		}
		break;
	default:
		return 0;
	}

	for (int i = 0; i < an_patch_len; i++) {
		ret = rtl837x_sds_reg_write(priv, sds_idx, an_patch[i][0], an_patch[i][1], an_patch[i][2]);
		if (ret)
			return ret;
	}
	return 0;
}

int rtl837x_serdes_mac_patch(struct rtl837x_priv *priv, u8 sds_idx)
{
	int ret;

	for (int i = 0; i < sizeof(patch_data_mac)/(sizeof(u16)*3); i++) {
		ret = rtl837x_sds_reg_write(priv, sds_idx, patch_data_mac[i][0], patch_data_mac[i][1], patch_data_mac[i][2]);
		if (ret)
			return ret;
	}
	return 0;
}

int rtl837x_serdes_off(struct rtl837x_priv *priv, u8 sds_idx)
{
	int ret;
	// Force enable Rx
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, SDS_PAGE_FRC, SDS_REG_FRC,
			    SDS_FRC_RX_EN_VAL_MASK | SDS_FRC_RX_EN_ON_MASK,
			    0x3
		);
	if (ret)
		return ret;
	msleep(20);
	// Force disable Rx
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, SDS_PAGE_FRC, SDS_REG_FRC,
			    SDS_FRC_RX_EN_VAL_MASK | SDS_FRC_RX_EN_ON_MASK,
			    0x1
		);
	if (ret)
		return ret;
	msleep(50);

	// Force Power ON
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, SDS_PAGE_FRC, SDS_REG_FRC,
			    SDS_FRC_PDOWN_ON_MASK | SDS_FRC_PDOWN_VAL_MASK,
			    0x1);
	if (ret)
		return ret;
	msleep(20);
	// Force Power OFF
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, SDS_PAGE_FRC, SDS_REG_FRC,
			    SDS_FRC_PDOWN_ON_MASK | SDS_FRC_PDOWN_VAL_MASK,
			    0x3);
	if (ret)
		return ret;
	msleep(50);

	// Force enable CMU(Clock Multiplier Unit(PLL))
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, SDS_PAGE_FRC, SDS_REG_FRC,
			    SDS_FRC_CMU_EN_ON_MASK | SDS_FRC_CMU_EN_VAL_MASK,
			    0x3);
	if (ret)
		return ret;
	msleep(20);
	// Force disable CMU(Clock Multiplier Unit(PLL))
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, SDS_PAGE_FRC, SDS_REG_FRC,
			    SDS_FRC_CMU_EN_ON_MASK | SDS_FRC_CMU_EN_VAL_MASK,
			    0x1);
	if (ret)
		return ret;
	msleep(50);

	return 0;
}

int rtl837x_serdes_on(struct rtl837x_priv *priv, u8 sds_idx)
{
	int ret;
	// Force disable CMU(Clock Multiplier Unit(PLL))
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, SDS_PAGE_FRC, SDS_REG_FRC,
		    SDS_FRC_CMU_EN_ON_MASK | SDS_FRC_CMU_EN_VAL_MASK,
		    0x1);
	if (ret)
		return ret;
	msleep(20);
	// Force enable CMU(Clock Multiplier Unit(PLL))
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, SDS_PAGE_FRC, SDS_REG_FRC,
		    SDS_FRC_CMU_EN_ON_MASK | SDS_FRC_CMU_EN_VAL_MASK,
		    0x3);
	if (ret)
		return ret;
	msleep(50);
	// Switch to auto Mode CMU(Clock Multiplier Unit(PLL))
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, SDS_PAGE_FRC, SDS_REG_FRC,
		    SDS_FRC_CMU_EN_ON_MASK | SDS_FRC_CMU_EN_VAL_MASK,
		    0x0);
	if (ret)
		return ret;
	msleep(20);

	// Force Power OFF
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, SDS_PAGE_FRC, SDS_REG_FRC,
		    SDS_FRC_PDOWN_ON_MASK | SDS_FRC_PDOWN_VAL_MASK,
		    0x3);
	if (ret)
		return ret;
	msleep(20);
	// Force Power ON
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, SDS_PAGE_FRC, SDS_REG_FRC,
		    SDS_FRC_PDOWN_ON_MASK | SDS_FRC_PDOWN_VAL_MASK,
		    0x1);
	if (ret)
		return ret;
	msleep(50);
	// Power Auto Mode
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, SDS_PAGE_FRC, SDS_REG_FRC,
		    SDS_FRC_PDOWN_ON_MASK | SDS_FRC_PDOWN_VAL_MASK,
		    0x0);
	if (ret)
		return ret;
	msleep(20);

	// Force disable Rx
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, SDS_PAGE_FRC, SDS_REG_FRC,
		    SDS_FRC_RX_EN_VAL_MASK | SDS_FRC_RX_EN_ON_MASK,
		    0x1
		);
	if (ret)
		return ret;
	msleep(20);
	// Force enable Rx
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, SDS_PAGE_FRC, SDS_REG_FRC,
		    SDS_FRC_RX_EN_VAL_MASK | SDS_FRC_RX_EN_ON_MASK,
		    0x3
		);
	if (ret)
		return ret;
	msleep(50);
	// Rx auto
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, SDS_PAGE_FRC, SDS_REG_FRC,
		    SDS_FRC_RX_EN_VAL_MASK | SDS_FRC_RX_EN_ON_MASK,
		    0x0
		);
	if (ret)
		return ret;
	msleep(50);

	return 0;
}

int rtl837x_serdes_pause_frame_config(struct rtl837x_priv *priv, u8 sds_idx,
			     phy_interface_t interface,
				 bool pause, bool asympause)
{
	// Set Auto Negotiation Pause/AsymPause
	int adv = 0;
	int ret;

	if (pause)
		adv |= BIT(0);
	if (asympause)
		adv |= BIT(1);

	switch (interface) {
	case PHY_INTERFACE_MODE_100BASEX:
		ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x1f, 5, 0x1<<2, 0x1);
		if (ret)
			return ret;
		ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x1f, 5, 0x1<<3, 0x1);
		if (ret)
			return ret;
		ret = rtl837x_sds_reg_bits_write(priv, sds_idx, SDS_PAGE_CTRL02, SDS_REG_CTRL02_XSG_AN,
				  SDS_CTRL02_XSG_AN_10_100_AsymmetricPause_MASK | SDS_CTRL02_XSG_AN_10_100_Pause_MASK,
				  adv
				);
		if (ret)
			return ret;
		break;
	case PHY_INTERFACE_MODE_1000BASEX:
	case PHY_INTERFACE_MODE_2500BASEX:
		ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x1f, 5, 0x1<<2, 0x1);
		if (ret)
			return ret;
		ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x1f, 5, 0x1<<3, 0x0);
		if (ret)
			return ret;
		ret = rtl837x_sds_reg_bits_write(priv, sds_idx, SDS_PAGE_CTRL02, SDS_REG_CTRL02_XSG_AN,
				  SDS_CTRL02_XSG_AN_1G_AsymmetricPause_MASK | SDS_CTRL02_XSG_AN_1G_Pause_MASK,
				  adv
				);
		if (ret)
			return ret;
		break;
	case PHY_INTERFACE_MODE_10GBASER:
		ret = rtl837x_sds_reg_bits_write(priv, sds_idx, SDS_PAGE_CTRL1F, SDS_REG_CTRL1F_10GR_AN,
			  SDS_CTRL1F_10GR_AN_AsymmetricPause_MASK | SDS_CTRL1F_10GR_AN_Pause_MASK,
			  adv
			);
		if (ret)
			return ret;
		break;
	default:
		break;
	}
	return 0;
}

int rtl837x_serdes_an_config(struct rtl837x_priv *priv, u8 sds_idx,
			     phy_interface_t interface,
				 unsigned int neg_mode)
{
	int ret;

	// Auto Negotiation
	switch (interface) {
	case PHY_INTERFACE_MODE_SGMII:
	case PHY_INTERFACE_MODE_1000BASEX:
	case PHY_INTERFACE_MODE_2500BASEX:
		if (neg_mode == PHYLINK_PCS_NEG_INBAND_ENABLED)
			ret = rtl837x_sds_reg_bits_write(priv, sds_idx, SDS_PAGE_CTRL00, SDS_REG_CTRL00_REG02,
						  SDS_CTRL00_REG02_XSG_SP_SDS_FRC_AN_EN | SDS_CTRL00_REG02_XSG_SP_SDS_FRC_AN,
						  0x3
						);
		else
			ret = rtl837x_sds_reg_bits_write(priv, sds_idx, SDS_PAGE_CTRL00, SDS_REG_CTRL00_REG02,
						  SDS_CTRL00_REG02_XSG_SP_SDS_FRC_AN_EN | SDS_CTRL00_REG02_XSG_SP_SDS_FRC_AN,
						  0x1
						);
		if (ret)
			return ret;

		/* set SP_CFG_EN_LINK_FIB1G for enable fiberNwayForceLink */
		ret = rtl837x_sds_reg_bits_write(priv, sds_idx, SDS_PAGE_CTRL00, SDS_REG_CTRL00_REG04,
			  SDS_CTRL00_REG04_SP_CFG_EN_LINK_FIB1G_MASK,
			  0x1
			);
		if (ret)
			return ret;
		break;
	case PHY_INTERFACE_MODE_USXGMII:
		if (neg_mode == PHYLINK_PCS_NEG_INBAND_ENABLED)
			ret = rtl837x_sds_reg_bits_write(priv, sds_idx, SDS_PAGE_NWAY_AN, SDS_REG_NWAY_AN,
					  SDS_NWAY_QHSG_AN_CH0_EN_MASK | SDS_NWAY_QHSG_AN_CH1_EN_MASK |
					  SDS_NWAY_QHSG_AN_CH2_EN_MASK | SDS_NWAY_QHSG_AN_CH3_EN_MASK,
					  0xf
					);
		else
			ret = rtl837x_sds_reg_bits_write(priv, sds_idx, SDS_PAGE_NWAY_AN, SDS_REG_NWAY_AN,
					  SDS_NWAY_QHSG_AN_CH0_EN_MASK | SDS_NWAY_QHSG_AN_CH1_EN_MASK |
					  SDS_NWAY_QHSG_AN_CH2_EN_MASK | SDS_NWAY_QHSG_AN_CH3_EN_MASK,
					  0x0
					);
		if (ret)
			return ret;
		break;
	default:
		break;
	}

	return 0;
}
