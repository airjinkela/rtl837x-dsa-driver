// SPDX-License-Identifier: GPL-2.0-or-later
/*
 * Copyright (C) 2025 StarField Xu <air_jinkela@163.com>
 */
#include <linux/version.h>

#include "rtl837x.h"
#include "rtl837x_serdes.h"

/*
 * Chip version. The release id field is only valid while the EN field is
 * armed (0xa), so reading it requires a write-read-write sequence.
 */
#define RTL837X_CHIP_VER_REG                  0xC
#define   RTL837X_CHIP_VER_RELEASE_VID_MASK   GENMASK(31, 28)
#define   RTL837X_CHIP_VER_MCID_MASK          GENMASK(27, 24)
#define   RTL837X_CHIP_VER_BO_CHIP_MODE_MASK  GENMASK(23, 20)
#define   RTL837X_CHIP_VER_EN_MASK            GENMASK(19, 16)
#define   RTL837X_CHIP_VER_RELEASE_ID_MASK    GENMASK(15, 0)

/*
 * Internal SerDes configuration. SDS0 feeds MAC3 and SDS1 feeds MAC8; each
 * SerDes has a link mode and, for USXGMII, a separate sub-mode field.
 */
#define RTL837X_SDS_MODE_SEL_REG                          0x7B20
#define   RTL837X_SDS_MODE_SEL_CFG_MAC8_8221B_MASK         BIT(22)
#define   RTL837X_SDS_MODE_SEL_CFG_MAC3_8221B_MASK         BIT(21)
#define   RTL837X_SDS_MODE_SEL_SDS1_USX_SUB_MODE_MASK      GENMASK(20, 16)
#define   RTL837X_SDS_MODE_SEL_SDS0_USX_SUB_MODE_MASK      GENMASK(14, 10)
#define   RTL837X_SDS_MODE_SEL_SDS1_MODE_SEL_MASK          GENMASK(9, 5)
#define   RTL837X_SDS_MODE_SEL_SDS0_MODE_SEL_MASK          GENMASK(4, 0)

enum rtl837x_serdes_mode {
	RTL837X_SERDES_MODE_SGMII = 0x02,
	RTL837X_SERDES_MODE_1000BASEX = 0x04,
	RTL837X_SERDES_MODE_100FX = 0x05,
	RTL837X_SERDES_MODE_USXGMII = 0x0D,
	RTL837X_SERDES_MODE_HSGMII = 0x12,
	RTL837X_SERDES_MODE_2500BASEX = 0x16,
	RTL837X_SERDES_MODE_10GBASER = 0x1A,
	RTL837X_SERDES_MODE_OFF = 0x1f
};

enum rtl837x_serdes_usxgmii_submode {
	RTL837X_SERDES_USXGMII_SM_NONE = 0,
	RTL837X_SERDES_USXGMII_SM_10GQXGMII = 2,
};

/*
 * As Realtek has not released the register manual
 * for the internal SerDes, the register definitions
 * below have been inferred from comments and names
 * found in OEM code and code for similar IP chips.
 */

/* Paged SerDes registers */
/* Page define code from :
 * https://github.com/openwrt/openwrt/blob/main/target/linux/realtek/files-6.18/drivers/net/pcs/pcs-rtl-otto.c
 */
enum rtpcs_page {
	PAGE_SDS		= 0x00,
	PAGE_SDS_EXT		= 0x01,
	PAGE_FIB		= 0x02,
	PAGE_FIB_EXT		= 0x03,
	PAGE_TGR_STD_0		= 0x04,
	PAGE_TGR_STD_1		= 0x05,
	PAGE_TGR_PRO_0		= 0x06,
	PAGE_TGR_PRO_1		= 0x07,
	PAGE_TGX_STD_0		= 0x08,
	PAGE_TGX_STD_1		= 0x09,
	PAGE_TGX_PRO_0		= 0x0a,
	PAGE_TGX_PRO_1		= 0x0b,
	PAGE_WDIG		= 0x1f,
	PAGE_ANA_MISC		= 0x20,
	PAGE_ANA_COM		= 0x21,
	PAGE_ANA_SPD		= 0x22,
	PAGE_ANA_SPD_EXT	= 0x23,
	PAGE_ANA_1G2		= 0x24,
	PAGE_ANA_1G2_EXT	= 0x25,
	PAGE_ANA_2G5		= 0x26,
	PAGE_ANA_2G5_EXT	= 0x27,
	PAGE_ANA_3G1		= 0x28,
	PAGE_ANA_3G1_EXT	= 0x29,
	PAGE_ANA_5G0		= 0x2a,
	PAGE_ANA_5G0_EXT	= 0x2b,
	PAGE_ANA_6G2		= 0x2c,
	PAGE_ANA_6G2_EXT	= 0x2d,
	PAGE_ANA_10G		= 0x2e,
	PAGE_ANA_10G_EXT	= 0x2f,
	PAGE_GPON_SP		= 0x30,
	PAGE_GPON_SP_EXT	= 0x31,
	PAGE_EPON_SP		= 0x32,
	PAGE_EPON_SP_EXT	= 0x33,
	PAGE_ANA_6G0		= 0x34,
	PAGE_ANA_6G0_EXT	= 0x35,
};

/* PAGE_ANA_MISC (0x20) */
#define ANA_MISC_FRC             0x00
#define  ANA_MISC_FRC_RX_EN_ON_MASK    BIT(4)
#define  ANA_MISC_FRC_RX_EN_VAL_MASK   BIT(5)
#define  ANA_MISC_FRC_PDOWN_ON_MASK    BIT(6)
#define  ANA_MISC_FRC_PDOWN_VAL_MASK   BIT(7)
#define  ANA_MISC_FRC_CMU_EN_ON_MASK   BIT(10)
#define  ANA_MISC_FRC_CMU_EN_VAL_MASK  BIT(11)

/* PAGE_TGR_PRO_0 (0x06) */
#define TGR_PRO_0_REG02   0x02
#define  TGR_PRO_0_REG02_FSM_RESET_MASK    BIT(12)
#define  TGR_PRO_0_REG02_10GR_RX_INV_MASK  BIT(13)
#define  TGR_PRO_0_REG02_10GR_TX_INV_MASK  BIT(14)

#define TGR_PRO_0_AM_PERIOD             0x12
#define   TGR_PRO_0_AM_PERIOD_MASK      GENMASK(15, 0)

#define TGR_PRO_0_AM_CFG0      0x13
#define  TGR_PRO_0_AM_CFG0_AM0_M1		GENMASK(15, 8)
#define  TGR_PRO_0_AM_CFG0_AM0_M0		GENMASK(7, 0)

#define TGR_PRO_0_AM_CFG1      0x14
#define  TGR_PRO_0_AM_CFG1_AM1_M0		GENMASK(15, 8)
#define  TGR_PRO_0_AM_CFG1_AM0_M2		GENMASK(7, 0)

#define TGR_PRO_0_AM_CFG2      0x15
#define  TGR_PRO_0_AM_CFG2_AM1_M2		GENMASK(15, 8)
#define  TGR_PRO_0_AM_CFG2_AM1_M1		GENMASK(7, 0)

#define TGR_PRO_0_AM_CFG3      0x16
#define  TGR_PRO_0_AM_CFG3_AM2_M1		GENMASK(15, 8)
#define  TGR_PRO_0_AM_CFG3_AM2_M0		GENMASK(7, 0)

#define TGR_PRO_0_AM_CFG4      0x17
#define  TGR_PRO_0_AM_CFG4_AM3_M0		GENMASK(15, 8)
#define  TGR_PRO_0_AM_CFG4_AM2_M2		GENMASK(7, 0)

#define TGR_PRO_0_AM_CFG5      0x18
#define  TGR_PRO_0_AM_CFG5_AM3_M2		GENMASK(15, 8)
#define  TGR_PRO_0_AM_CFG5_AM3_M1		GENMASK(7, 0)

/* PAGE_TGR_PRO_1 (0x07) */
#define TGR_PRO_1_REG06			0x06
#define  TGR_PRO_1_REG06_CFG_QHSG_TXCFG_MAC_CH0	GENMASK(15, 0)

#define TGR_PRO_1_REG08			0x08
#define  TGR_PRO_1_REG08_CFG_QHSG_TXCFG_MAC_CH1	GENMASK(15, 0)

#define TGR_PRO_1_REG10			0x0a
#define  TGR_PRO_1_REG10_CFG_QHSG_TXCFG_MAC_CH2	GENMASK(15, 0)

#define TGR_PRO_1_REG12			0x0c
#define  TGR_PRO_1_REG12_CFG_QHSG_TXCFG_MAC_CH3	GENMASK(15, 0)

#define TGR_PRO_1_REG16			0x10
#define  TGR_PRO_1_REG16_CFG_QHSG_AN_OPC	GENMASK(7, 0)
#define   TGR_PRO_1_REG16_CFG_QHSG_AN_OPC_STD	0x03
#define   TGR_PRO_1_REG16_CFG_QHSG_AN_OPC_RTK	0xaa

#define TGR_PRO_1_REG17    0x11
#define  TGR_PRO_1_REG17_QHSG_AN_CH0_EN_MASK BIT(0)
#define  TGR_PRO_1_REG17_QHSG_AN_CH1_EN_MASK BIT(1)
#define  TGR_PRO_1_REG17_QHSG_AN_CH2_EN_MASK BIT(2)
#define  TGR_PRO_1_REG17_QHSG_AN_CH3_EN_MASK BIT(3)

/* PAGE_SDS (0x00) */
#define SDS_REG00     0x00
#define  SDS_REG00_XSG_TX_INV_MASK  BIT(8)
#define  SDS_REG00_XSG_RX_INV_MASK  BIT(9)
#define  SDS_REG00_XSG_RX_EN_MASK   BIT(1)
#define  SDS_REG00_XSG_TX_EN_MASK   BIT(0)

#define SDS_REG02     0x02
#define  SDS_REG02_XSG_SP_SDS_FRC_AN    BIT(8)
#define  SDS_REG02_XSG_SP_SDS_FRC_AN_EN BIT(9)

#define SDS_REG04     0x04
#define  SDS_REG04_SP_CFG_EN_LINK_FIB1G_MASK   BIT(2)

/* PAGE_SDS_EXT (0x01) */
#define SDS_EXT_XSG_STS      0x1d     // I Guess
#define  SDS_EXT_XSG_STS_SYNC_OK_MASK   BIT(0) // I Guess
#define  SDS_EXT_XSG_STS_LINK_OK_MASK   BIT(4) // I Guess
#define  SDS_EXT_XSG_STS_SIG_OK_MASK    BIT(8) // I Guess

/* PAGE_FIB (0x02) */
// It seems like 'Advertisement control register'
#define FIB_XSG_AN   0x04
#define  FIB_XSG_AN_10_100_ASYM_PAUSE_MASK BIT(11)
#define  FIB_XSG_AN_10_100_PAUSE_MASK           BIT(10)
#define  FIB_XSG_AN_1G_ASYM_PAUSE_MASK BIT(8)
#define  FIB_XSG_AN_1G_PAUSE_MASK           BIT(7)
#define  FIB_XSG_AN_1G_HALF_DUPLEX_MASK      BIT(6)
#define  FIB_XSG_AN_1G_FULL_DUPLEX_MASK      BIT(5)

/* PAGE_TGR_STD_1 (0x05) */
#define TGR_STD_1_10GR_STS     0x00       // I Guess
#define  TGR_STD_1_10GR_STS_SYNC_OK_MASK   BIT(0)  // I Guess
#define  TGR_STD_1_10GR_STS_HI_BER_MASK    BIT(1)  // I Guess
#define  TGR_STD_1_10GR_STS_LINK_OK_MASK   BIT(12) // I Guess

/* PAGE_WDIG (0x1f) */
#define WDIG_10GR_AN  0x0B
#define  WDIG_10GR_AN_PAUSE_MASK       BIT(2)
#define  WDIG_10GR_AN_ASYM_PAUSE_MASK  BIT(3)

/* PAGE_ANA_10G (0x2e) */
#define ANA_10G_REG06                     0x06
#define  ANA_10G_REG06_TX_POST1AMP_MASK   GENMASK(15, 10)
#define  ANA_10G_REG06_TX_POST1EN_MASK    BIT(3)

#define ANA_10G_REG07                     0x07
#define  ANA_10G_REG07_TX_MAINAMP_MASK    GENMASK(15, 10)
#define  ANA_10G_REG07_TX_PREAMP_MASK     GENMASK(8, 3)
#define  ANA_10G_REG07_TX_MAINEN_MASK     BIT(9)
#define  ANA_10G_REG07_TX_PREEN_MASK      BIT(2)

#define ANA_10G_REG11               0x0b
#define  ANA_10G_REG11_TX_Z0_MASK  GENMASK(10, 7)

static const u16 patch_an_10p3125g_a[][3] = {
	{PAGE_ANA_COM, 0x0010, 0x4480}, {PAGE_ANA_COM, 0x0013, 0x0400},
	{PAGE_ANA_COM, 0x0018, 0x6D02}, {PAGE_ANA_COM, 0x001B, 0x424E},
	{PAGE_ANA_COM, 0x001D, 0x0002}, {0x0036, 0x001C, 0x1390},
	{PAGE_ANA_10G, 0x0004, 0x0080}, {PAGE_ANA_10G, ANA_10G_REG06, 0x0408},
	{PAGE_ANA_10G, ANA_10G_REG07, 0x020D}, {PAGE_ANA_10G, 0x0009, 0x0601},
	{PAGE_ANA_10G, ANA_10G_REG11, 0x222C}, {PAGE_ANA_10G, 0x000C, 0x9217},
	{PAGE_ANA_10G, 0x0016, 0x0743}, {PAGE_ANA_10G, 0x001D, 0xABB0}
};

static const u16 patch_an_10p3125g_b[][3] = {
	{PAGE_ANA_COM, 0x0010, 0x4480}, {PAGE_ANA_COM, 0x0013, 0x0400},
	{PAGE_ANA_COM, 0x0018, 0x6D02}, {PAGE_ANA_COM, 0x001B, 0x424E},
	{PAGE_ANA_COM, 0x001D, 0x0002}, {0x0036, 0x001C, 0x1390},
	{0x0036, 0x0014, 0x003F}, {0x0036, 0x0010, 0x0200},
	{PAGE_ANA_10G, 0x0004, 0x0080}, {PAGE_ANA_10G, ANA_10G_REG06, 0x0408},
	{PAGE_ANA_10G, ANA_10G_REG07, 0x020D}, {PAGE_ANA_10G, 0x0009, 0x0601},
	{PAGE_ANA_10G, ANA_10G_REG11, 0x222C}, {PAGE_ANA_10G, 0x000C, 0xA217},
	{PAGE_ANA_10G, 0x000D, 0xFE40}, {PAGE_ANA_10G, 0x0015, 0xF5C1},
	{PAGE_ANA_10G, 0x0016, 0x0443}, {PAGE_ANA_10G, 0x001D, 0xABB0}
};

static const u16 patch_an_3p125g_a[][3] = {
	{PAGE_ANA_COM, 0x0010, 0x6480}, {PAGE_ANA_COM, 0x0013, 0x0400},
	{PAGE_ANA_COM, 0x0018, 0x6D02}, {PAGE_ANA_COM, 0x001B, 0x424E},
	{PAGE_ANA_COM, 0x001D, 0x0002}, {0x0036, 0x001C, 0x1390},
	{PAGE_ANA_3G1, 0x0004, 0x0080}, {PAGE_ANA_3G1, 0x0009, 0x0601},
	{PAGE_ANA_3G1, 0x000B, 0x232C}, {PAGE_ANA_3G1, 0x000C, 0x9217},
	{PAGE_ANA_3G1, 0x000F, 0x5B50}, {PAGE_ANA_3G1, 0x0016, 0x0443},
	{PAGE_ANA_3G1, 0x001D, 0xABB0}
};

static const u16 patch_an_3p125g_b[][3] = {
	{PAGE_ANA_COM, 0x0010, 0x6480}, {PAGE_ANA_COM, 0x0013, 0x0400},
	{PAGE_ANA_COM, 0x0018, 0x6D02}, {PAGE_ANA_COM, 0x001B, 0x424E},
	{PAGE_ANA_COM, 0x001D, 0x0002}, {0x0036, 0x001C, 0x1390},
	{0x0036, 0x0014, 0x003F}, {0x0036, 0x0010, 0x0200},
	{PAGE_ANA_3G1, 0x0004, 0x0080}, {PAGE_ANA_3G1, 0x0007, 0x1201},
	{PAGE_ANA_3G1, 0x0009, 0x0601}, {PAGE_ANA_3G1, 0x000B, 0x232C},
	{PAGE_ANA_3G1, 0x000C, 0x9217}, {PAGE_ANA_3G1, 0x000F, 0x5B50},
	{PAGE_ANA_3G1, 0x0015, 0xE7F1}, {PAGE_ANA_3G1, 0x0016, 0x0443},
	{PAGE_ANA_3G1, 0x001D, 0xABB0}
};

static const u16 patch_an_1p25g_a[][3] = {
	{PAGE_ANA_COM, 0x0010, 0x6480}, {PAGE_ANA_COM, 0x0013, 0x0400},
	{PAGE_ANA_COM, 0x0018, 0x6D02}, {PAGE_ANA_COM, 0x001B, 0x424E},
	{PAGE_ANA_COM, 0x001D, 0x0002}, {0x0036, 0x001C, 0x1390},
	{0x0036, 0x0014, 0x003F}, {0x0036, 0x0010, 0x0300},
	{PAGE_ANA_1G2, 0x0004, 0x0080}, {PAGE_ANA_1G2, 0x0007, 0x1201},
	{PAGE_ANA_1G2, 0x0009, 0x0601}, {PAGE_ANA_1G2, 0x000B, 0x232C},
	{PAGE_ANA_1G2, 0x000C, 0x9217}, {PAGE_ANA_1G2, 0x000F, 0x5B50},
	{PAGE_ANA_1G2, 0x0015, 0xE7C1}, {PAGE_ANA_1G2, 0x0016, 0x0443},
	{PAGE_ANA_1G2, 0x001D, 0xABB0}
};

static const u16 patch_an_1p25g_b[][3] = {
	{PAGE_ANA_COM, 0x0010, 0x6480}, {PAGE_ANA_COM, 0x0013, 0x0400},
	{PAGE_ANA_COM, 0x0018, 0x6D02}, {PAGE_ANA_COM, 0x001B, 0x424E},
	{PAGE_ANA_COM, 0x001D, 0x0002}, {0x0036, 0x001C, 0x1390},
	{0x0036, 0x0014, 0x003F}, {0x0036, 0x0010, 0x0300},
	{PAGE_ANA_1G2, 0x0004, 0x0080}, {PAGE_ANA_1G2, 0x0007, 0x1201},
	{PAGE_ANA_1G2, 0x0009, 0x0601}, {PAGE_ANA_1G2, 0x000B, 0x232C},
	{PAGE_ANA_1G2, 0x000C, 0x9217}, {PAGE_ANA_1G2, 0x000F, 0x5B50},
	{PAGE_ANA_1G2, 0x0015, 0xE7C1}, {PAGE_ANA_1G2, 0x0016, 0x0443},
	{PAGE_ANA_1G2, 0x001D, 0xABB0}
};

static const u16 patch_an_125m_a[][3] = {
	{PAGE_ANA_COM, 0x0010, 0x6480}, {PAGE_ANA_COM, 0x0013, 0x0400},
	{PAGE_ANA_COM, 0x0018, 0x6D02}, {PAGE_ANA_COM, 0x001B, 0x424E},
	{PAGE_ANA_COM, 0x001D, 0x0002}, {0x0036, 0x001C, 0x1390},
	{PAGE_ANA_2G5, 0x0004, 0x0080}, {PAGE_ANA_2G5, 0x0009, 0x0601},
	{PAGE_ANA_2G5, 0x000B, 0x232C}, {PAGE_ANA_2G5, 0x000C, 0x9217},
	{PAGE_ANA_2G5, 0x000F, 0x5B50}, {PAGE_ANA_2G5, 0x0016, 0x0443},
	{PAGE_ANA_2G5, 0x001D, 0xABB0}
};

static const u16 patch_an_125m_b[][3] = {
	{PAGE_ANA_COM, 0x0010, 0x6480}, {PAGE_ANA_COM, 0x0013, 0x0400},
	{PAGE_ANA_COM, 0x0018, 0x6D02}, {PAGE_ANA_COM, 0x001B, 0x424E},
	{PAGE_ANA_COM, 0x001D, 0x0002}, {0x0036, 0x001C, 0x1390},
	{0x0036, 0x0014, 0x003F}, {0x0036, 0x0010, 0x0300},
	{PAGE_ANA_2G5, 0x0004, 0x0080}, {PAGE_ANA_2G5, 0x0007, 0x1201},
	{PAGE_ANA_2G5, 0x0009, 0x0601}, {PAGE_ANA_2G5, 0x000B, 0x232C},
	{PAGE_ANA_2G5, 0x000C, 0x9217}, {PAGE_ANA_2G5, 0x000F, 0x5B50},
	{PAGE_ANA_2G5, 0x0015, 0xE7C1}, {PAGE_ANA_2G5, 0x0016, 0x0443},
	{PAGE_ANA_2G5, 0x001D, 0xABB0}
};

static const u16 patch_data_mac[][3] = {
	{PAGE_TGR_PRO_0, 0x0012, 0x5078}, {PAGE_TGR_PRO_1, 0x0006, 0x9401},
	{PAGE_TGR_PRO_1, 0x0008, 0x9401}, {PAGE_TGR_PRO_1, 0x000A, 0x9401},
	{PAGE_TGR_PRO_1, 0x000C, 0x9401}, {PAGE_WDIG, 0x000B, 0x0003},
	{PAGE_TGR_PRO_0, 0x0003, 0xC45C}, {PAGE_TGR_PRO_0, 0x001F, 0x2100}
};

static const u16 patch_data_phy[][3] = {
	{PAGE_TGR_PRO_0, 0x0012, 0x5078}, {PAGE_TGR_PRO_0, 0x0003, 0xc45c},
	{PAGE_TGR_PRO_0, 0x001E, 0x000C}, {PAGE_TGR_PRO_0, 0x001F, 0x2100}
};

/* TODO: save in chip private data struct on detect */
static int rtl837x_get_chip_ver(struct realtek_priv *priv, u16 *chip_ver)
{
	int ret;
	u32 tmp;

	ret = rtl837x_reg_bits_write(priv, RTL837X_CHIP_VER_REG,
				     RTL837X_CHIP_VER_EN_MASK, 0xa);
	if (ret)
		return ret;
	ret = rtl837x_reg_bits_read(priv, RTL837X_CHIP_VER_REG,
				    RTL837X_CHIP_VER_RELEASE_VID_MASK, &tmp);
	if (ret)
		return ret;
	ret = rtl837x_reg_bits_write(priv, RTL837X_CHIP_VER_REG,
				     RTL837X_CHIP_VER_EN_MASK, 0);

	*chip_ver = tmp;
	return 0;
}

int rtl837x_serdes_pnswap_rx(struct realtek_priv *priv, u8 sds_idx, bool enable)
{
	int ret;

	/* SerDes PN swap: page 0x00 reg 0x00 (XSG, <=5G) + page 0x06 reg 0x02 (10GR) */
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, PAGE_SDS,
					 SDS_REG00,
					 SDS_REG00_XSG_RX_INV_MASK,
					 !!enable);
	if (ret)
		return ret;

	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, PAGE_TGR_PRO_0,
					 TGR_PRO_0_REG02,
					 TGR_PRO_0_REG02_10GR_RX_INV_MASK,
					 !!enable);
	if (ret)
		return ret;

	return 0;
}

int rtl837x_serdes_pnswap_tx(struct realtek_priv *priv, u8 sds_idx, bool enable)
{
	int ret;

	/* SerDes PN swap: page 0x00 reg 0x00 (XSG, <=5G) + page 0x06 reg 0x02 (10GR) */
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, PAGE_SDS,
					 SDS_REG00,
					 SDS_REG00_XSG_TX_INV_MASK,
					 !!enable);
	if (ret)
		return ret;

	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, PAGE_TGR_PRO_0,
					 TGR_PRO_0_REG02,
					 TGR_PRO_0_REG02_10GR_TX_INV_MASK,
					 !!enable);
	if (ret)
		return ret;

	return 0;
}

int rtl837x_serdes_reset_10G(struct realtek_priv *priv, u8 sds_idx)
{
	/* nsq: noise squelch, hi_ber: high bit error rate */
	bool rx_idle, nsq, sync_ok, link_ok, hi_ber;
	u16 rx_sts, tmp;
	int ret;

	ret = rtl837x_sds_reg_bits_read(priv, sds_idx, PAGE_ANA_MISC,
					ANA_MISC_FRC,
					ANA_MISC_FRC_RX_EN_VAL_MASK |
					ANA_MISC_FRC_RX_EN_ON_MASK,
					&rx_sts);
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
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, 0x36, 0x05, 0xf << 11, 8); // REG0_DEBUG_SEL
	if (ret)
		return ret;
	ret = rtl837x_sds_reg_write(priv, sds_idx, 0x1f, 0x02, 0x1f); // switch debug port
	if (ret)
		return ret;

	// Get RX status
	ret = rtl837x_sds_reg_read(priv, sds_idx, 0x1f, 0x15, &tmp);
	if (ret)
		return ret;
	rx_idle = !!((tmp >> 7) & 1);
	nsq = !!((tmp >> 6) & 1);

	// Do reset when RX is not idle or RX Noise Squelch
	if (!(nsq == 1 || rx_idle == 0))
		return 0;

	/* Check whether the link is synced and healthy */
	ret = rtl837x_sds_reg_read(priv, sds_idx, PAGE_TGR_STD_1,
				   TGR_STD_1_10GR_STS, &tmp);
	if (ret)
		return ret;

	sync_ok = FIELD_GET(TGR_STD_1_10GR_STS_SYNC_OK_MASK, tmp);
	link_ok = FIELD_GET(TGR_STD_1_10GR_STS_LINK_OK_MASK, tmp);
	hi_ber = FIELD_GET(TGR_STD_1_10GR_STS_HI_BER_MASK, tmp);

	if (!sync_ok || !link_ok || hi_ber)
		goto do_reset;

	return 0;

do_reset:
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, PAGE_ANA_MISC,
					 ANA_MISC_FRC,
					 ANA_MISC_FRC_RX_EN_VAL_MASK |
					 ANA_MISC_FRC_RX_EN_ON_MASK, 0x3);
	if (ret)
		return ret;
	msleep(20);

	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, PAGE_ANA_MISC,
					 ANA_MISC_FRC,
					 ANA_MISC_FRC_RX_EN_VAL_MASK |
					 ANA_MISC_FRC_RX_EN_ON_MASK, 0x1);
	if (ret)
		return ret;
	msleep(20);

	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, PAGE_ANA_MISC,
					 ANA_MISC_FRC,
					 ANA_MISC_FRC_RX_EN_VAL_MASK |
					 ANA_MISC_FRC_RX_EN_ON_MASK, 0x3);
	if (ret)
		return ret;
	msleep(20);

	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, PAGE_ANA_MISC,
					 ANA_MISC_FRC,
					 ANA_MISC_FRC_RX_EN_VAL_MASK |
					 ANA_MISC_FRC_RX_EN_ON_MASK, 0x0);
	if (ret)
		return ret;
	msleep(20);

	return 0;
}

// 100M/1G/2.5G reset
int rtl837x_serdes_reset_XSG(struct realtek_priv *priv, u8 sds_idx)
{
	bool sig_ok, sync_ok, link_ok;
	u16 rx_sts, tmp;
	int ret;

	ret = rtl837x_sds_reg_bits_read(priv, sds_idx, PAGE_ANA_MISC, ANA_MISC_FRC,
					ANA_MISC_FRC_RX_EN_VAL_MASK | ANA_MISC_FRC_RX_EN_ON_MASK,
			  &rx_sts
			);
	if (ret)
		return ret;

	// check serdes rx status
	// 0,2,3: rx is enabled
	// 1    : rx is disabled
	if (rx_sts == 1) // do nothing when rx is disabled
		return 0;

	ret = rtl837x_sds_reg_read(priv, sds_idx, PAGE_SDS_EXT, SDS_EXT_XSG_STS, &tmp);
	if (ret)
		return ret;

	sig_ok =  FIELD_GET(SDS_EXT_XSG_STS_SIG_OK_MASK, tmp);
	link_ok = FIELD_GET(SDS_EXT_XSG_STS_LINK_OK_MASK, tmp);
	sync_ok = FIELD_GET(SDS_EXT_XSG_STS_SYNC_OK_MASK, tmp);

	if (!sig_ok)
		return 0;

	if (sync_ok == 0)
		goto do_reset;
	else
		if (link_ok == 0)
			goto do_reset;
	return 0;

do_reset:
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx,
					 PAGE_SDS, SDS_REG00,
					 SDS_REG00_XSG_RX_EN_MASK, 1);
	if (ret)
		return ret;
	msleep(20);
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx,
					 PAGE_SDS, SDS_REG00,
					 SDS_REG00_XSG_RX_EN_MASK, 0);
	if (ret)
		return ret;
	msleep(20);
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx,
					 PAGE_SDS, SDS_REG00,
					 SDS_REG00_XSG_RX_EN_MASK, 1);
	if (ret)
		return ret;
	msleep(20);
	return 0;
}

int rtl837x_serdes_an_patch(struct realtek_priv *priv, u8 sds_idx, phy_interface_t interface)
{
	const u16 (*an_patch)[3];
	int an_patch_len;
	u16 chip_ver = 0;
	int ret;

	ret = rtl837x_get_chip_ver(priv, &chip_ver);
	if (ret) {
		dev_err(priv->dev, "Failed to get chip ver\n");
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
			an_patch_len = sizeof(patch_an_10p3125g_a) / (sizeof(u16) * 3);
		} else {
			an_patch = patch_an_10p3125g_b;
			an_patch_len = sizeof(patch_an_10p3125g_b) / (sizeof(u16) * 3);
		}
		break;
	//HSGMII ?
	case PHY_INTERFACE_MODE_2500BASEX:
		if (chip_ver == 0) {
			an_patch = patch_an_3p125g_a;
			an_patch_len = sizeof(patch_an_3p125g_a) / (sizeof(u16) * 3);
		} else {
			an_patch = patch_an_3p125g_b;
			an_patch_len = sizeof(patch_an_3p125g_b) / (sizeof(u16) * 3);
		}
		break;
	case PHY_INTERFACE_MODE_SGMII:
	case PHY_INTERFACE_MODE_1000BASEX:
		if (chip_ver == 0) {
			an_patch = patch_an_1p25g_a;
			an_patch_len = sizeof(patch_an_1p25g_a) / (sizeof(u16) * 3);
		} else {
			an_patch = patch_an_1p25g_b;
			an_patch_len = sizeof(patch_an_1p25g_b) / (sizeof(u16) * 3);
		}
		break;
	case PHY_INTERFACE_MODE_100BASEX:
		if (chip_ver == 0) {
			an_patch = patch_an_125m_a;
			an_patch_len = sizeof(patch_an_125m_a) / (sizeof(u16) * 3);
		} else {
			an_patch = patch_an_125m_b;
			an_patch_len = sizeof(patch_an_125m_b) / (sizeof(u16) * 3);
		}
		break;
	default:
		return 0;
	}

	for (int i = 0; i < an_patch_len; i++) {
		ret = rtl837x_sds_reg_write(priv, sds_idx, an_patch[i][0],
					    an_patch[i][1], an_patch[i][2]);
		if (ret)
			return ret;
	}
	return 0;
}

int rtl837x_serdes_mac_patch(struct realtek_priv *priv, u8 sds_idx)
{
	int ret;

	for (int i = 0; i < sizeof(patch_data_mac) / (sizeof(u16) * 3); i++) {
		ret = rtl837x_sds_reg_write(priv, sds_idx, patch_data_mac[i][0],
					    patch_data_mac[i][1],
					    patch_data_mac[i][2]);
		if (ret)
			return ret;
	}
	return 0;
}

int rtl837x_serdes_set_serdes_mode(struct realtek_priv *priv, u8 sds_idx,
				   phy_interface_t interface)
{
	u32 sds_sub_mode_mask, sds_mode_mask;
	enum rtl837x_serdes_mode mode;
	int ret;

	sds_sub_mode_mask = sds_idx == 0 ?
			    RTL837X_SDS_MODE_SEL_SDS0_USX_SUB_MODE_MASK :
			    RTL837X_SDS_MODE_SEL_SDS1_USX_SUB_MODE_MASK;
	sds_mode_mask = sds_idx == 0 ?
		  RTL837X_SDS_MODE_SEL_SDS0_MODE_SEL_MASK : RTL837X_SDS_MODE_SEL_SDS1_MODE_SEL_MASK;

	if (sds_idx == 0)
		ret = rtl837x_reg_bits_write(priv, RTL837X_SDS_MODE_SEL_REG,
					     RTL837X_SDS_MODE_SEL_CFG_MAC3_8221B_MASK,
					     0);
	else
		ret = rtl837x_reg_bits_write(priv, RTL837X_SDS_MODE_SEL_REG,
					     RTL837X_SDS_MODE_SEL_CFG_MAC8_8221B_MASK,
					     0);
	if (ret)
		return ret;

	// TODO: USXGMII submode support
	ret = rtl837x_reg_bits_write(priv, RTL837X_SDS_MODE_SEL_REG,
				     sds_sub_mode_mask, RTL837X_SERDES_USXGMII_SM_NONE);
	if (ret)
		return ret;

	switch (interface) {
	case PHY_INTERFACE_MODE_USXGMII:
		mode = RTL837X_SERDES_MODE_USXGMII;
		break;
	case PHY_INTERFACE_MODE_10GBASER:
		mode = RTL837X_SERDES_MODE_10GBASER;
		break;
	case PHY_INTERFACE_MODE_2500BASEX:
		mode = RTL837X_SERDES_MODE_2500BASEX;
		break;
	case PHY_INTERFACE_MODE_1000BASEX:
		mode = RTL837X_SERDES_MODE_1000BASEX;
		break;
	case PHY_INTERFACE_MODE_SGMII:
		mode = RTL837X_SERDES_MODE_SGMII;
		break;
	case PHY_INTERFACE_MODE_100BASEX:
		mode = RTL837X_SERDES_MODE_100FX;
		break;
	default:
		dev_err(priv->ds->dev, "unsupported interface: %s\n",
			phy_modes(interface));
		return -EINVAL;
	}

	ret = rtl837x_reg_bits_write(priv, RTL837X_SDS_MODE_SEL_REG,
				     sds_mode_mask, mode);
	if (ret)
		return ret;
	return 0;
}

int rtl837x_serdes_off(struct realtek_priv *priv, u8 sds_idx)
{
	int ret;
	// Force enable Rx
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, PAGE_ANA_MISC, ANA_MISC_FRC,
					 ANA_MISC_FRC_RX_EN_VAL_MASK | ANA_MISC_FRC_RX_EN_ON_MASK,
			    0x3
		);
	if (ret)
		return ret;
	msleep(20);
	// Force disable Rx
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, PAGE_ANA_MISC, ANA_MISC_FRC,
					 ANA_MISC_FRC_RX_EN_VAL_MASK | ANA_MISC_FRC_RX_EN_ON_MASK,
			    0x1
		);
	if (ret)
		return ret;
	msleep(50);

	// Force Power ON
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, PAGE_ANA_MISC, ANA_MISC_FRC,
					 ANA_MISC_FRC_PDOWN_ON_MASK | ANA_MISC_FRC_PDOWN_VAL_MASK,
			    0x1);
	if (ret)
		return ret;
	msleep(20);
	// Force Power OFF
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, PAGE_ANA_MISC, ANA_MISC_FRC,
					 ANA_MISC_FRC_PDOWN_ON_MASK | ANA_MISC_FRC_PDOWN_VAL_MASK,
			    0x3);
	if (ret)
		return ret;
	msleep(50);

	// Force enable CMU(Clock Multiplier Unit(PLL))
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, PAGE_ANA_MISC, ANA_MISC_FRC,
					 ANA_MISC_FRC_CMU_EN_ON_MASK | ANA_MISC_FRC_CMU_EN_VAL_MASK,
			    0x3);
	if (ret)
		return ret;
	msleep(20);
	// Force disable CMU(Clock Multiplier Unit(PLL))
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, PAGE_ANA_MISC, ANA_MISC_FRC,
					 ANA_MISC_FRC_CMU_EN_ON_MASK | ANA_MISC_FRC_CMU_EN_VAL_MASK,
			    0x1);
	if (ret)
		return ret;
	msleep(50);

	return 0;
}

int rtl837x_serdes_on(struct realtek_priv *priv, u8 sds_idx)
{
	int ret;
	// Force disable CMU(Clock Multiplier Unit(PLL))
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, PAGE_ANA_MISC, ANA_MISC_FRC,
					 ANA_MISC_FRC_CMU_EN_ON_MASK | ANA_MISC_FRC_CMU_EN_VAL_MASK,
		    0x1);
	if (ret)
		return ret;
	msleep(20);
	// Force enable CMU(Clock Multiplier Unit(PLL))
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, PAGE_ANA_MISC, ANA_MISC_FRC,
					 ANA_MISC_FRC_CMU_EN_ON_MASK | ANA_MISC_FRC_CMU_EN_VAL_MASK,
		    0x3);
	if (ret)
		return ret;
	msleep(50);
	// Switch to auto Mode CMU(Clock Multiplier Unit(PLL))
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, PAGE_ANA_MISC, ANA_MISC_FRC,
					 ANA_MISC_FRC_CMU_EN_ON_MASK | ANA_MISC_FRC_CMU_EN_VAL_MASK,
		    0x0);
	if (ret)
		return ret;
	msleep(20);

	// Force Power OFF
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, PAGE_ANA_MISC, ANA_MISC_FRC,
					 ANA_MISC_FRC_PDOWN_ON_MASK | ANA_MISC_FRC_PDOWN_VAL_MASK,
		    0x3);
	if (ret)
		return ret;
	msleep(20);
	// Force Power ON
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, PAGE_ANA_MISC, ANA_MISC_FRC,
					 ANA_MISC_FRC_PDOWN_ON_MASK | ANA_MISC_FRC_PDOWN_VAL_MASK,
		    0x1);
	if (ret)
		return ret;
	msleep(50);
	// Power Auto Mode
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, PAGE_ANA_MISC, ANA_MISC_FRC,
					 ANA_MISC_FRC_PDOWN_ON_MASK | ANA_MISC_FRC_PDOWN_VAL_MASK,
		    0x0);
	if (ret)
		return ret;
	msleep(20);

	// Force disable Rx
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, PAGE_ANA_MISC, ANA_MISC_FRC,
					 ANA_MISC_FRC_RX_EN_VAL_MASK | ANA_MISC_FRC_RX_EN_ON_MASK,
		    0x1
		);
	if (ret)
		return ret;
	msleep(20);
	// Force enable Rx
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, PAGE_ANA_MISC, ANA_MISC_FRC,
					 ANA_MISC_FRC_RX_EN_VAL_MASK | ANA_MISC_FRC_RX_EN_ON_MASK,
		    0x3
		);
	if (ret)
		return ret;
	msleep(50);
	// Rx auto
	ret = rtl837x_sds_reg_bits_write(priv, sds_idx, PAGE_ANA_MISC, ANA_MISC_FRC,
					 ANA_MISC_FRC_RX_EN_VAL_MASK | ANA_MISC_FRC_RX_EN_ON_MASK,
		    0x0
		);
	if (ret)
		return ret;
	msleep(50);

	return 0;
}

int rtl837x_serdes_pause_frame_config(struct realtek_priv *priv, u8 sds_idx,
				      phy_interface_t interface,
				      const unsigned long *advertising)
{
	/* Auto negotiation pause/asympause advertisement */
	int adv = 0;
	int ret;

	if (linkmode_test_bit(ETHTOOL_LINK_MODE_Pause_BIT, advertising))
		adv |= BIT(0);
	if (linkmode_test_bit(ETHTOOL_LINK_MODE_Asym_Pause_BIT, advertising))
		adv |= BIT(1);

	switch (interface) {
	case PHY_INTERFACE_MODE_100BASEX:
		ret = rtl837x_sds_reg_bits_write(priv, sds_idx,
						 PAGE_WDIG, 5,
						 0x1 << 2, 0x1);
		if (ret)
			return ret;

		ret = rtl837x_sds_reg_bits_write(priv, sds_idx,
						 PAGE_WDIG, 5,
						 0x1 << 3, 0x1);
		if (ret)
			return ret;

		ret = rtl837x_sds_reg_bits_write(priv, sds_idx,
						 PAGE_FIB,
						 FIB_XSG_AN,
						 FIB_XSG_AN_10_100_ASYM_PAUSE_MASK |
						 FIB_XSG_AN_10_100_PAUSE_MASK,
						 adv);
		if (ret)
			return ret;
		break;
	case PHY_INTERFACE_MODE_1000BASEX:
	case PHY_INTERFACE_MODE_2500BASEX:
		ret = rtl837x_sds_reg_bits_write(priv, sds_idx,
						 PAGE_WDIG, 5,
						 0x1 << 2, 0x1);
		if (ret)
			return ret;

		ret = rtl837x_sds_reg_bits_write(priv, sds_idx,
						 PAGE_WDIG, 5,
						 0x1 << 3, 0x0);
		if (ret)
			return ret;

		ret = rtl837x_sds_reg_bits_write(priv, sds_idx,
						 PAGE_FIB,
						 FIB_XSG_AN,
						 FIB_XSG_AN_1G_ASYM_PAUSE_MASK |
						 FIB_XSG_AN_1G_PAUSE_MASK,
						 adv);
		if (ret)
			return ret;
		break;
	case PHY_INTERFACE_MODE_10GBASER:
		ret = rtl837x_sds_reg_bits_write(priv, sds_idx,
						 PAGE_WDIG,
						 WDIG_10GR_AN,
						 WDIG_10GR_AN_ASYM_PAUSE_MASK |
						 WDIG_10GR_AN_PAUSE_MASK,
						 adv);
		if (ret)
			return ret;
		break;
	default:
		break;
	}
	return 0;
}

/*
 * Switch a SerDes in USXGMII mode from Realtek's proprietary USXGMII variant
 * (the chip default) to standard USXGMII, so that it works with PHYs in their
 * default setup (e.g. RTL8261N/BE with the unmodified rtl8261n.bin firmware).
 * Same values as the RTL930x PCS driver in OpenWrt; pages 6 and 7 are
 * TGR_PRO_0 and TGR_PRO_1.
 */
static int rtl837x_serdes_usxgmii_std(struct realtek_priv *priv, u8 sds_idx)
{
	static const u16 cfg[][3] = {
		{ PAGE_TGR_PRO_0, 0x0d, 0x0f00 },
		{ PAGE_TGR_PRO_0, 0x1d, 0x0600 },
		{ PAGE_TGR_PRO_1, TGR_PRO_1_REG06, 0x1401 },
		{ PAGE_TGR_PRO_1, TGR_PRO_1_REG08, 0x1401 },
		{ PAGE_TGR_PRO_1, TGR_PRO_1_REG10, 0x1401 },
		{ PAGE_TGR_PRO_1, TGR_PRO_1_REG12, 0x1401 },
		/* AM insert period */
		{ PAGE_TGR_PRO_0, TGR_PRO_0_AM_PERIOD, 0x00a4 },
		/* no alignment markers */
		{ PAGE_TGR_PRO_0, TGR_PRO_0_AM_CFG0, 0x0000 },
		{ PAGE_TGR_PRO_0, TGR_PRO_0_AM_CFG1, 0x0000 },
		{ PAGE_TGR_PRO_0, TGR_PRO_0_AM_CFG2, 0x0000 },
		{ PAGE_TGR_PRO_0, TGR_PRO_0_AM_CFG3, 0x0000 },
		{ PAGE_TGR_PRO_0, TGR_PRO_0_AM_CFG4, 0x0000 },
		{ PAGE_TGR_PRO_0, TGR_PRO_0_AM_CFG5, 0x0000 },
	};
	int ret;
	int i;

	for (i = 0; i < ARRAY_SIZE(cfg); i++) {
		ret = rtl837x_sds_reg_write(priv, sds_idx, cfg[i][0], cfg[i][1],
					    cfg[i][2]);
		if (ret)
			return ret;
	}

	/* USXGMII AN opcode: 0x03 standard, 0xaa Realtek */
	return rtl837x_sds_reg_bits_write(priv, sds_idx,
					  PAGE_TGR_PRO_1,
					  TGR_PRO_1_REG16,
					  TGR_PRO_1_REG16_CFG_QHSG_AN_OPC,
					  TGR_PRO_1_REG16_CFG_QHSG_AN_OPC_STD);
}

int rtl837x_serdes_an_config(struct realtek_priv *priv, u8 sds_idx,
			     phy_interface_t interface,
			     unsigned int neg_mode)
{
	int ret;

	switch (interface) {
	case PHY_INTERFACE_MODE_SGMII:
	case PHY_INTERFACE_MODE_1000BASEX:
	case PHY_INTERFACE_MODE_2500BASEX:
		if (neg_mode == PHYLINK_PCS_NEG_INBAND_ENABLED)
			ret = rtl837x_sds_reg_bits_write(priv, sds_idx,
							 PAGE_SDS,
							 SDS_REG02,
							 SDS_REG02_XSG_SP_SDS_FRC_AN_EN |
							 SDS_REG02_XSG_SP_SDS_FRC_AN,
							 0x3);
		else
			ret = rtl837x_sds_reg_bits_write(priv, sds_idx,
							 PAGE_SDS,
							 SDS_REG02,
							 SDS_REG02_XSG_SP_SDS_FRC_AN_EN |
							 SDS_REG02_XSG_SP_SDS_FRC_AN,
							 0x1);
		if (ret)
			return ret;

		/* Set SP_CFG_EN_LINK_FIB1G to enable fiberNwayForceLink */
		ret = rtl837x_sds_reg_bits_write(priv, sds_idx,
						 PAGE_SDS,
						 SDS_REG04,
						 SDS_REG04_SP_CFG_EN_LINK_FIB1G_MASK,
						 0x1);
		if (ret)
			return ret;
		break;
	case PHY_INTERFACE_MODE_USXGMII:
		if (neg_mode == PHYLINK_PCS_NEG_INBAND_ENABLED)
			ret = rtl837x_sds_reg_bits_write(priv, sds_idx,
							 PAGE_TGR_PRO_1,
							 TGR_PRO_1_REG17,
							 TGR_PRO_1_REG17_QHSG_AN_CH0_EN_MASK |
							 TGR_PRO_1_REG17_QHSG_AN_CH1_EN_MASK |
							 TGR_PRO_1_REG17_QHSG_AN_CH2_EN_MASK |
							 TGR_PRO_1_REG17_QHSG_AN_CH3_EN_MASK,
							 0xf);
		else
			ret = rtl837x_sds_reg_bits_write(priv, sds_idx,
							 PAGE_TGR_PRO_1,
							 TGR_PRO_1_REG17,
							 TGR_PRO_1_REG17_QHSG_AN_CH0_EN_MASK |
							 TGR_PRO_1_REG17_QHSG_AN_CH1_EN_MASK |
							 TGR_PRO_1_REG17_QHSG_AN_CH2_EN_MASK |
							 TGR_PRO_1_REG17_QHSG_AN_CH3_EN_MASK,
							 0x0);
		if (ret)
			return ret;

		ret = rtl837x_serdes_usxgmii_std(priv, sds_idx);
		if (ret)
			return ret;
		break;
	default:
		break;
	}

	return 0;
}
